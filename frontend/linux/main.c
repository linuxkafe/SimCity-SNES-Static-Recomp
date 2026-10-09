#include "simcity_static_recomp.h"

#include "menu.h"
#include "settings_ini.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Resolution presets matching common display resolutions */
typedef struct {
    int width;
    int height;
    const char *name;
} Resolution;

/* Order must match the documented --resolution indices and the default. */
static const Resolution RESOLUTIONS[] = {
    { 512,  480,  "480p"  },  /* default / fallback */
    { 1280, 720,  "720p"  },
    { 1280, 800,  "800p"  },
    { 1920, 1080, "1080p" },
};

static SimCityRecomp *g_recomp = NULL;
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static SDL_GameController *g_gamepad = NULL;
static SDL_AudioDeviceID g_audio_device = 0;

/* Soft mouse: maps the host pointer onto the guest's own d-pad cursor.
   Position feedback is deliberately not used -- the core exposes cursor X
   ($025D/$01EB) but no cursor Y, and guessing the Y address would drive the
   cursor diagonally into map edges.  Rate mapping needs no guest state. */
static int   g_soft_mouse = 0;
static int   g_mouse_sens = 2;      /* texture px of motion needed per frame */
static int   g_mouse_have = 0;      /* last pointer sample valid */
static int   g_mouse_px = 0, g_mouse_py = 0;
static int   g_mouse_dx = 0, g_mouse_dy = 0;   /* per-frame pointer delta */
static int   g_size_explicit = 0;        /* --size overrides --resolution */
static SimCityMenu g_menu;
static SimCityLinuxConfig g_config;
static char g_settings_path[512] = "settings.ini";
static int g_running = 1;
static uint32_t g_frame_count = 0;
static int g_target_width = 512;
static int g_target_height = 480;

static const struct {
    SDL_Scancode scan;
    uint16_t mask;
} KEYMAP[] = {
    { SDL_SCANCODE_UP,    SIMCITY_INPUT_UP },
    { SDL_SCANCODE_DOWN,  SIMCITY_INPUT_DOWN },
    { SDL_SCANCODE_LEFT,  SIMCITY_INPUT_LEFT },
    { SDL_SCANCODE_RIGHT, SIMCITY_INPUT_RIGHT },
    { SDL_SCANCODE_Z,     SIMCITY_INPUT_B },
    { SDL_SCANCODE_X,     SIMCITY_INPUT_A },
    { SDL_SCANCODE_A,     SIMCITY_INPUT_L },
    { SDL_SCANCODE_S,     SIMCITY_INPUT_R },
    { SDL_SCANCODE_RETURN,SIMCITY_INPUT_START },
    { SDL_SCANCODE_BACKSPACE, SIMCITY_INPUT_SELECT },
    { SDL_SCANCODE_Q,     SIMCITY_INPUT_Y },
};

static const struct {
    SDL_GameControllerButton btn;
    uint16_t mask;
} GAMEPADMAP[] = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP,    SIMCITY_INPUT_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN,  SIMCITY_INPUT_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT,  SIMCITY_INPUT_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SIMCITY_INPUT_RIGHT },
    { SDL_CONTROLLER_BUTTON_A,          SIMCITY_INPUT_B },      /* A = B (confirm) */
    { SDL_CONTROLLER_BUTTON_B,          SIMCITY_INPUT_A },      /* B = A (cancel) */
    { SDL_CONTROLLER_BUTTON_X,          SIMCITY_INPUT_Y },      /* X = Y */
    { SDL_CONTROLLER_BUTTON_Y,          SIMCITY_INPUT_X },      /* Y = X */
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SIMCITY_INPUT_L },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SIMCITY_INPUT_R },
    { SDL_CONTROLLER_BUTTON_START,      SIMCITY_INPUT_START },
    { SDL_CONTROLLER_BUTTON_BACK,       SIMCITY_INPUT_SELECT },
    { SDL_CONTROLLER_BUTTON_GUIDE,      SIMCITY_INPUT_START },  /* Steam button = Start */
};

/* Sample the host pointer and turn per-frame motion into guest d-pad presses.
   SDL_RenderCopy stretches the whole texture onto the whole window, so window
   coordinates map linearly onto texture coordinates and the guest's own cursor
   (d-pad driven) follows the pointer without any core knowledge. */
/* Funds live at WRAM $0B9D as a 24-bit little-endian value. This address was
   confirmed against this core: a fresh city holds $004E20, the documented
   $20000 starting balance. Addresses taken from other emulators are NOT
   assumed here. */
#define SIMCITY_CHEAT_FUNDS_ADDR 0x0B9Du
#define SIMCITY_CHEAT_FUNDS_MAX  999999u

static void apply_money_cheat(void)
{
    uint8_t money[3];
    unsigned value = SIMCITY_CHEAT_FUNDS_MAX;
    money[0] = (uint8_t)(value & 0xFFu);
    money[1] = (uint8_t)((value >> 8) & 0xFFu);
    money[2] = (uint8_t)((value >> 16) & 0xFFu);
    if (!simcity_recomp_write_wram(g_recomp, SIMCITY_CHEAT_FUNDS_ADDR, money, 3u))
        fprintf(stderr, "money cheat rejected by simcity_recomp_write_wram\n");
}

/* Kept in step with the menu so the overlay and the cheat cannot disagree. */
static void apply_config_to_frontend(void)
{
    int w = 0, h = 0;
    g_soft_mouse = g_config.soft_mouse;
    g_mouse_sens = g_config.mouse_sens;
    if (g_window) {
        int cw = 0, ch = 0;
        SDL_GetWindowSize(g_window, &cw, &ch);
        if (cw != g_config.width || ch != g_config.height) {
            SDL_SetWindowSize(g_window, g_config.width, g_config.height);
        }
    }
    if (g_renderer && g_recomp) {
        int want = simcity_recomp_widescreen_enabled(g_recomp) ? 1 : 0;
        if (want != g_config.widescreen) {
            char ws_error[256];
            memset(ws_error, 0, sizeof(ws_error));
            if (simcity_recomp_set_widescreen(g_recomp, g_config.widescreen,
                                              ws_error, sizeof(ws_error)) != 1)
                fprintf(stderr, "widescreen change failed: %s\n", ws_error);
            simcity_menu_res_size(SIMCITY_MENU_RES_COUNT - 1, &w, &h);
        }
        /* Texture geometry follows the widescreen flag. */
        {
            int tw = g_config.widescreen ? SIMCITY_RECOMP_WIDESCREEN_WIDTH
                                         : SIMCITY_RECOMP_FRAME_WIDTH;
            int th = SIMCITY_RECOMP_FRAME_HEIGHT;
            if (g_texture) {
                int tw_now = 0, th_now = 0;
                SDL_QueryTexture(g_texture, NULL, NULL, &tw_now, &th_now);
                if (tw_now != tw || th_now != th) {
                    SDL_Texture *fresh = SDL_CreateTexture(
                        g_renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, tw, th);
                    if (fresh) {
                        SDL_DestroyTexture(g_texture);
                        g_texture = fresh;
                    }
                }
            }
        }
    }
    if (g_soft_mouse) SDL_ShowCursor(SDL_DISABLE);
    else             SDL_ShowCursor(SDL_ENABLE);
}

static void soft_mouse_sample(void)
{
    int win_w = 0, win_h = 0, tex_w, tex_h;
    int mx = 0, my = 0;
    Uint32 buttons;

    g_mouse_dx = 0;
    g_mouse_dy = 0;
    if (!g_soft_mouse || !g_window) return;

    SDL_GetWindowSize(g_window, &win_w, &win_h);
    if (win_w <= 0 || win_h <= 0) return;

    buttons = SDL_GetMouseState(&mx, &my);
    tex_w = simcity_recomp_widescreen_enabled(g_recomp)
                ? SIMCITY_RECOMP_WIDESCREEN_WIDTH
                : SIMCITY_RECOMP_FRAME_WIDTH;
    tex_h = SIMCITY_RECOMP_FRAME_HEIGHT;

    /* window -> texture, rounding toward the centre so the pointer stays put
       when it is not moving */
    int tx = (int)(((long)mx * tex_w + win_w / 2) / win_w);
    int ty = (int)(((long)my * tex_h + win_h / 2) / win_h);
    if (tx < 0) tx = 0; else if (tx >= tex_w) tx = tex_w - 1;
    if (ty < 0) ty = 0; else if (ty >= tex_h) ty = tex_h - 1;

    if (!g_mouse_have) {
        g_mouse_px = tx; g_mouse_py = ty; g_mouse_have = 1;
        (void)buttons;
        return;
    }
    g_mouse_dx = tx - g_mouse_px;
    g_mouse_dy = ty - g_mouse_py;
    g_mouse_px = tx; g_mouse_py = ty;
}

/* PCM is handed to SDL from the main thread with SDL_QueueAudio instead of
   being pulled by SDL's audio thread.  The core's ring buffer is written by
   audio_sink() during simcity_recomp_advance() on the main thread; letting the
   device thread read the same buffer without synchronisation was a data race,
   and wrapping the whole frame advance in a lock would have blocked the device
   thread for the length of a frame.  Queueing keeps one writer and one reader
   on different objects. */
static void pump_audio(void)
{
    int16_t chunk[2048 * SIMCITY_RECOMP_AUDIO_CHANNELS];
    size_t avail, take;

    if (!g_audio_device || !g_recomp) return;

    while (SDL_GetQueuedAudioSize(g_audio_device) >
           (Uint32)(4 * SIMCITY_RECOMP_AUDIO_CHANNELS * 2048)) {
        if (!SDL_DequeueAudio(g_audio_device, chunk,
                              sizeof(chunk) / sizeof(chunk[0])))
            break;
    }

    avail = simcity_recomp_audio_available(g_recomp);
    take = avail < 2048u ? avail : 2048u;
    if (take == 0u) return;

    take = simcity_recomp_audio_read(g_recomp, chunk, take);
    if (take == 0u) return;

    SDL_QueueAudio(g_audio_device, chunk,
                   (Uint32)(take * sizeof(int16_t) *
                            SIMCITY_RECOMP_AUDIO_CHANNELS));
}

static int load_rom(const char *path, uint8_t **rom, size_t *rom_size)
{
    SDL_RWops *f = SDL_RWFromFile(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open ROM: %s - %s\n", path, SDL_GetError());
        return -1;
    }

    Sint64 size = SDL_RWsize(f);
    SDL_RWclose(f);

    if (size <= 0 || (size_t)size != (size_t)size) {
        fprintf(stderr, "Invalid ROM size\n");
        return -1;
    }

    *rom_size = (size_t)size;
    *rom = (uint8_t *)SDL_malloc(*rom_size);
    if (!*rom) {
        fprintf(stderr, "ROM allocation failed\n");
        return -1;
    }

    f = SDL_RWFromFile(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot reopen ROM: %s\n", path);
        SDL_free(*rom);
        return -1;
    }

    size_t read = SDL_RWread(f, *rom, 1, *rom_size);
    SDL_RWclose(f);

    if (read != *rom_size) {
        fprintf(stderr, "ROM read failed: expected %zu, got %zu\n", *rom_size, read);
        SDL_free(*rom);
        return -1;
    }

    return 0;
}

static int init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    g_window = SDL_CreateWindow("SimCity SNES Static Recomp",
                                 100, 100,
                                 g_target_width, g_target_height,
                                 SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    /* Core outputs at 256x239 (standard) or 398x239 (widescreen).
     * We create a texture at the core's output resolution and scale via renderer. */
    int tex_width = simcity_recomp_widescreen_enabled(g_recomp)
                        ? SIMCITY_RECOMP_WIDESCREEN_WIDTH
                        : SIMCITY_RECOMP_FRAME_WIDTH;
    int tex_height = SIMCITY_RECOMP_FRAME_HEIGHT;

    /* Core produces uint32_t 0xAARRGGBB: blue in bits 0-7, green 8-15,
     * red 16-23, alpha 24-31. The SDL format must have the same channel
     * masks, which on this platform are:
     *   SDL_PIXELFORMAT_ARGB8888  R=0x00FF0000 G=0x0000FF00 B=0x000000FF
     *   SDL_PIXELFORMAT_ABGR8888  R=0x000000FF G=0x0000FF00 B=0x00FF0000
     * ABGR8888 therefore reads our blue byte as red and swaps the two
     * channels. Verified against SDL_AllocFormat rather than by eye. */
    g_texture = SDL_CreateTexture(g_renderer,
                                   SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   tex_width, tex_height);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = SIMCITY_RECOMP_HOST_AUDIO_SAMPLE_RATE;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    /* Small device buffer.  The earlier 8192-frame buffer allowed the device
       256 ms of slack, which is exactly the audible drag: SDL drains PCM in
       real time, so a quarter-second backlog is permanent latency.  surplus PCM
       is dropped after each frame (pump_audio) instead of accumulating. */
    spec.samples = 512;
    spec.callback = NULL;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL,
                                         SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!g_audio_device) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);

    if (g_soft_mouse) {
        /* The guest draws its own cursor; showing the host pointer as well
           reads as two cursors fighting. */
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    /* Initialize gamepad support for Steam Deck */
    if (SDL_NumJoysticks() > 0) {
        g_gamepad = SDL_GameControllerOpen(0);
        if (g_gamepad) {
            fprintf(stderr, "Gamepad connected: %s\n", SDL_GameControllerName(g_gamepad));
        }
    }

    return 0;
}

static void handle_input(uint16_t *input_mask)
{
    *input_mask = 0;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    for (size_t i = 0; i < sizeof(KEYMAP) / sizeof(KEYMAP[0]); i++) {
        if (keys[KEYMAP[i].scan]) {
            *input_mask |= KEYMAP[i].mask;
        }
    }

    /* Gamepad input (Steam Deck) */
    if (g_gamepad) {
        for (size_t i = 0; i < sizeof(GAMEPADMAP) / sizeof(GAMEPADMAP[0]); i++) {
            if (SDL_GameControllerGetButton(g_gamepad, GAMEPADMAP[i].btn)) {
                *input_mask |= GAMEPADMAP[i].mask;
            }
        }
    }

    /* Soft mouse drives the guest cursor.  A real d-pad from keyboard or pad
       always wins, so the two can never fight over the cursor. */
    if (g_soft_mouse) {
        uint16_t pad = SIMCITY_INPUT_UP | SIMCITY_INPUT_DOWN |
                       SIMCITY_INPUT_LEFT | SIMCITY_INPUT_RIGHT;
        if ((*input_mask & pad) == 0u) {
            if (g_mouse_dx >= g_mouse_sens)       *input_mask |= SIMCITY_INPUT_RIGHT;
            else if (g_mouse_dx <= -g_mouse_sens) *input_mask |= SIMCITY_INPUT_LEFT;
            if (g_mouse_dy >= g_mouse_sens)       *input_mask |= SIMCITY_INPUT_DOWN;
            else if (g_mouse_dy <= -g_mouse_sens) *input_mask |= SIMCITY_INPUT_UP;
        }
    }
}

static int advance_frame(uint16_t input_mask)
{
    SimCityRecompFrameResult result;
    SDL_zero(result);

    /* Advance without the expensive host frame conversion.  simcity_recomp_
       advance() renders internally, and the previous code then called
       simcity_recomp_render_current_frame() again, running the whole PPU scan
       twice per frame.  render_frame() converts exactly once. */
    int rc = simcity_recomp_advance_headless(g_recomp, input_mask, 1, &result);
    if (rc != 1) {
        fprintf(stderr, "simcity_recomp_advance failed: %s\n",
                simcity_recomp_last_error(g_recomp));
        return -1;
    }

    g_frame_count++;
    (void)result;

    return 0;
}

static void render_frame(void)
{
    const uint32_t *bgra;

    /* advance_frame() used the headless route, so the host conversion into
       simcity_recomp_frame_bgra() has not happened yet.  Do it once here. */
    if (simcity_recomp_render_current_frame(g_recomp, NULL, 0) != 1) {
        const char *err = simcity_recomp_last_error(g_recomp);
        if (err && err[0] && g_frame_count % 60u == 0u)
            fprintf(stderr, "render_current_frame: %s\n", err);
    }

    bgra = simcity_recomp_frame_bgra(g_recomp);
    if (!bgra) return;

    int tex_width = simcity_recomp_widescreen_enabled(g_recomp)
                        ? SIMCITY_RECOMP_WIDESCREEN_WIDTH
                        : SIMCITY_RECOMP_FRAME_WIDTH;

    SDL_UpdateTexture(g_texture, NULL, bgra,
                      tex_width * sizeof(uint32_t));

    SDL_RenderClear(g_renderer);

    /* Render texture scaled to window size */
    int w, h;
    SDL_GetWindowSize(g_window, &w, &h);

    SDL_Rect dst = {0, 0, w, h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);

    /* Deliberately does NOT present. The menu path draws the overlay on top of
       this same frame and must present once; presenting here as well showed
       the game for one frame without the overlay, which read as flicker. */
}

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options] --rom <path/to/rom.sfc>\n", argv0);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --rom <path>     Path to SimCity (USA).sfc ROM\n");
    fprintf(stderr, "  --resolution N   Resolution preset: 0=480p, 1=720p, 2=800p, 3=1080p (default: 0)\n");
    fprintf(stderr, "  --size WxH       Arbitrary window size (overrides --resolution)\n");
    fprintf(stderr, "  --widescreen     Enable widescreen mode (398x239 core output)\n");
    fprintf(stderr, "  --soft-mouse     Drive the guest d-pad cursor from the host pointer\n");
    fprintf(stderr, "  --mouse-sens N   Soft-mouse sensitivity, texture px per frame (default 2)\n");
    fprintf(stderr, "  --help           Show this help\n");
    fprintf(stderr, "\nIn game:\n");
    fprintf(stderr, "  F1               Open/close the settings menu\n");
    fprintf(stderr, "  Escape           Close the menu, or quit when it is closed\n");
    fprintf(stderr, "\nEnvironment variables:\n");
    fprintf(stderr, "  SIMCITY_ROM_PATH  ROM path (alternative to --rom)\n");
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    int resolution_idx = 0;  /* 480p default */
    int widescreen = 0;

    /* Settings file first; explicit command-line options override it. */
    (void)simcity_settings_ini_load(g_settings_path, &g_config);
    g_soft_mouse = g_config.soft_mouse;
    g_mouse_sens = g_config.mouse_sens;
    /* Baseline from settings.ini. A command-line option that overrides it
       writes g_config as well, so apply_config_to_frontend() at startup does
       not resize the window straight back to the file's value. */
    g_target_width = g_config.width;
    g_target_height = g_config.height;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            resolution_idx = atoi(argv[++i]);
            if (resolution_idx < 0 || resolution_idx >= (int)(sizeof(RESOLUTIONS)/sizeof(RESOLUTIONS[0]))) {
                fprintf(stderr, "Invalid resolution index. Use 0-3.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            int sw = 0, sh = 0;
            if (sscanf(argv[++i], "%dx%d", &sw, &sh) == 2 && sw > 0 && sh > 0) {
                g_target_width = sw;
                g_target_height = sh;
                g_size_explicit = 1;
                g_config.width = sw;
                g_config.height = sh;
            } else {
                fprintf(stderr, "Invalid --size, expected WxH (e.g. 1280x800)\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--soft-mouse") == 0) {
            g_soft_mouse = 1;
            g_config.soft_mouse = 1;
        } else if (strcmp(argv[i], "--mouse-sens") == 0 && i + 1 < argc) {
            g_mouse_sens = atoi(argv[++i]);
            if (g_mouse_sens < SIMCITY_MOUSE_SENS_MIN)
                g_mouse_sens = SIMCITY_MOUSE_SENS_MIN;
            if (g_mouse_sens > SIMCITY_MOUSE_SENS_MAX)
                g_mouse_sens = SIMCITY_MOUSE_SENS_MAX;
            g_config.mouse_sens = g_mouse_sens;
        } else if (strcmp(argv[i], "--widescreen") == 0) {
            widescreen = 1;
            g_config.widescreen = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!rom_path) {
        rom_path = getenv("SIMCITY_ROM_PATH");
    }

    if (!rom_path) {
        print_usage(argv[0]);
        return 1;
    }

    if (!g_size_explicit) {
        g_target_width = RESOLUTIONS[resolution_idx].width;
        g_target_height = RESOLUTIONS[resolution_idx].height;
        g_config.width = g_target_width;
        g_config.height = g_target_height;
    }

    uint8_t *rom = NULL;
    size_t rom_size = 0;

    if (load_rom(rom_path, &rom, &rom_size) != 0) {
        return 1;
    }

    if (rom_size != SIMCITY_RECOMP_ROM_SIZE) {
        fprintf(stderr, "ROM size mismatch: expected %u, got %zu\n",
                SIMCITY_RECOMP_ROM_SIZE, rom_size);
        SDL_free(rom);
        return 1;
    }

    char error[256];
    memset(error, 0, sizeof(error));
    if (simcity_recomp_create(&g_recomp, rom, rom_size, error, sizeof(error)) != 1) {
        fprintf(stderr, "simcity_recomp_create failed: %s\n", error);
        SDL_free(rom);
        return 1;
    }

    SDL_free(rom);

    /* Reset immediately after create to validate the core before touching SDL */
    if (simcity_recomp_reset(g_recomp, error, sizeof(error)) != 1) {
        const char *inst_err = simcity_recomp_last_error(g_recomp);
        if (error[0])
            fprintf(stderr, "simcity_recomp_reset failed: %s\n", error);
        else if (inst_err && inst_err[0])
            fprintf(stderr, "simcity_recomp_reset failed: (instance) %s\n", inst_err);
        else
            fprintf(stderr, "simcity_recomp_reset failed: unknown error\n");
        simcity_recomp_destroy(g_recomp);
        return 1;
    }

    /* Enable widescreen if requested */
    if (widescreen) {
        char ws_error[256];
        memset(ws_error, 0, sizeof(ws_error));
        if (simcity_recomp_set_widescreen(g_recomp, 1, ws_error, sizeof(ws_error)) != 1) {
            fprintf(stderr, "Failed to enable widescreen: %s\n", ws_error);
        }
    }

    if (init_sdl() != 0) {
        simcity_recomp_destroy(g_recomp);
        return 1;
    }

    simcity_menu_init(&g_menu, &g_config, g_renderer);
    apply_config_to_frontend();

    Uint64 next_deadline = SDL_GetPerformanceCounter();
    const Uint64 perf_freq = SDL_GetPerformanceFrequency();
    const double frame_hz = simcity_recomp_presentation_fps();
    const Uint64 frame_period =
        perf_freq ? (Uint64)((double)perf_freq / frame_hz + 0.5) : 16641u;
    /* Keep the PCM backlog near two device buffers.  If emulation outruns the
       device we drop surplus rather than let latency grow without bound. */
    const size_t audio_high_water = 1024u;

    /* Opt-in frame timing: SIMCITY_FPS=1 prints measured throughput so the
       real frame budget can be verified on target hardware. */
    const int report_fps = getenv("SIMCITY_FPS") != NULL;
    Uint64 fps_window = next_deadline;
    uint32_t fps_frames = 0u;

    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_running = 0;
            }
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_F1) {
                simcity_menu_set_open(&g_menu, !simcity_menu_is_open(&g_menu));
            }
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE) {
                if (simcity_menu_is_open(&g_menu))
                    simcity_menu_set_open(&g_menu, 0);
                else
                    g_running = 0;
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED) {
                if (!g_gamepad && event.cdevice.which < SDL_NumJoysticks())
                    g_gamepad = SDL_GameControllerOpen(event.cdevice.which);
            }
        }

        uint16_t input_mask = 0;
        handle_input(&input_mask);

        if (simcity_menu_is_open(&g_menu)) {
            /* The pointer delta is only resampled outside the menu branch, so
               it would stay frozen at its last value and steer the menu for
               as long as it stays open. Clear it so only real input moves the
               selection. */
            g_mouse_dx = 0;
            g_mouse_dy = 0;
            /* The menu pauses the guest entirely: no advance, no audio drain.
               Advancing here would spend money or move the city underneath the
               settings the player is changing. */
            SimCityMenuAction action =
                simcity_menu_handle_input(&g_menu, input_mask);
            if (action == SIMCITY_MENU_ACTION_APPLY_MONEY)
                apply_money_cheat();
            if (action == SIMCITY_MENU_ACTION_QUIT)
                break;
            if (action == SIMCITY_MENU_ACTION_RESUME) {
                apply_config_to_frontend();
                simcity_settings_ini_save(g_settings_path, &g_config);
            }
            render_frame();
            {
                int mw = 0, mh = 0;
                SDL_GetWindowSize(g_window, &mw, &mh);
                simcity_menu_draw(&g_menu, mw, mh);
            }
            /* Exactly one present per frame: the game image and the overlay
               reach the display in the same swap. */
            SDL_RenderPresent(g_renderer);
            SDL_Delay(16);
            continue;
        }

        soft_mouse_sample();

        if (advance_frame(input_mask) != 0) {
            break;
        }

        render_frame();

        /* render_frame() only draws into the back buffer; the game path is
           the one caller with no overlay, so it presents here. */
        SDL_RenderPresent(g_renderer);

        if (g_config.freeze_money)
            apply_money_cheat();

        pump_audio();

        if (simcity_recomp_audio_available(g_recomp) > audio_high_water)
            simcity_recomp_audio_discard(g_recomp);

        next_deadline += frame_period;
        Uint64 now_perf = SDL_GetPerformanceCounter();
        if (now_perf < next_deadline) {
            Uint32 wait_ms = (Uint32)(((next_deadline - now_perf) * 1000u)
                                      / (perf_freq ? perf_freq : 1000u));
            if (wait_ms > 0u) SDL_Delay(wait_ms);
        } else {
            /* Behind schedule: resynchronise instead of accumulating debt. */
            next_deadline = now_perf;
        }

        if (report_fps) {
            fps_frames++;
            Uint64 t = SDL_GetPerformanceCounter();
            if (t - fps_window >= perf_freq) {
                fprintf(stderr, "fps %.1f (%u frames)\n",
                        (double)fps_frames * (double)perf_freq /
                            (double)(t - fps_window),
                        fps_frames);
                fps_frames = 0u;
                fps_window = t;
            }
        }
    }

    simcity_settings_ini_save(g_settings_path, &g_config);

    simcity_recomp_destroy(g_recomp);
    g_recomp = NULL;

    if (g_gamepad) {
        SDL_GameControllerClose(g_gamepad);
        g_gamepad = NULL;
    }

    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    
    if (g_audio_device) {
        SDL_PauseAudioDevice(g_audio_device, 1);
        SDL_ClearQueuedAudio(g_audio_device);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
    
    SDL_Quit();

    return 0;
}