#include "simcity_static_recomp.h"

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

static const Resolution RESOLUTIONS[] = {
    { 1280, 720,  "720p"  },
    { 1280, 800,  "800p"  },
    { 1920, 1080, "1080p" },
    { 512,  480,  "480p"  },  /* default / fallback */
};

static SimCityRecomp *g_recomp = NULL;
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static SDL_GameController *g_gamepad = NULL;
static SDL_mutex *g_audio_mutex = NULL;
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

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    SimCityRecomp *r = g_recomp;
    if (!r) return;

    size_t frames_avail = simcity_recomp_audio_available(r);
    size_t frames_to_read = (size_t)len / (sizeof(int16_t) * 2);
    if (frames_to_read > frames_avail) frames_to_read = frames_avail;

    if (frames_to_read > 0) {
        simcity_recomp_audio_read(r, (int16_t *)stream, frames_to_read);
    }

    if ((size_t)len > frames_to_read * (sizeof(int16_t) * 2)) {
        memset(stream + frames_to_read * (sizeof(int16_t) * 2), 0,
               (size_t)len - frames_to_read * (sizeof(int16_t) * 2));
    }
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

    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
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

    /* Core produces uint32_t in format 0xAARRGGBB.
     * In little-endian memory: BB GG RR AA.
     * SDL_PIXELFORMAT_ABGR8888 expects exactly this layout.
     * Previously using XRGB8888 which expects BB GG RR XX (X=0),
     * but our alpha byte is 0xFF, not 0. */
    g_texture = SDL_CreateTexture(g_renderer,
                                   SDL_PIXELFORMAT_ABGR8888,
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
    spec.samples = 8192;  /* Larger buffer for Steam Deck to prevent underruns */
    spec.callback = audio_callback;

    if (SDL_OpenAudio(&spec, NULL) != 0) {
        fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudio(0);

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
}

static int advance_frame(uint16_t input_mask)
{
    SimCityRecompFrameResult result;
    SDL_zero(result);

    int rc = simcity_recomp_advance(g_recomp, input_mask, 1, &result);
    if (rc != 1) {
        fprintf(stderr, "simcity_recomp_advance failed: %s\n",
                simcity_recomp_last_error(g_recomp));
        return -1;
    }

    g_frame_count++;

    if (result.frame_rendered) {
        simcity_recomp_render_current_frame(g_recomp, NULL, 0);
    }

    return 0;
}

static void render_frame(void)
{
    const uint32_t *bgra = simcity_recomp_frame_bgra(g_recomp);
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

    SDL_RenderPresent(g_renderer);
}

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options] --rom <path/to/rom.sfc>\n", argv0);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --rom <path>     Path to SimCity (USA).sfc ROM\n");
    fprintf(stderr, "  --resolution N   Resolution preset: 0=480p, 1=720p, 2=800p, 3=1080p (default: 0)\n");
    fprintf(stderr, "  --widescreen     Enable widescreen mode (398x239 core output)\n");
    fprintf(stderr, "  --help           Show this help\n");
    fprintf(stderr, "\nEnvironment variables:\n");
    fprintf(stderr, "  SIMCITY_ROM_PATH  ROM path (alternative to --rom)\n");
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    int resolution_idx = 0;  /* 480p default */
    int widescreen = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            resolution_idx = atoi(argv[++i]);
            if (resolution_idx < 0 || resolution_idx >= (int)(sizeof(RESOLUTIONS)/sizeof(RESOLUTIONS[0]))) {
                fprintf(stderr, "Invalid resolution index. Use 0-3.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--widescreen") == 0) {
            widescreen = 1;
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

    g_target_width = RESOLUTIONS[resolution_idx].width;
    g_target_height = RESOLUTIONS[resolution_idx].height;

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

    Uint32 last_time = SDL_GetTicks();
    const Uint32 FRAME_DELAY_MS = 16;

    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_running = 0;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    g_running = 0;
                }
            }
        }

        uint16_t input_mask = 0;
        handle_input(&input_mask);

        if (advance_frame(input_mask) != 0) {
            break;
        }

        render_frame();

        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - last_time;
        if (elapsed < FRAME_DELAY_MS) {
            SDL_Delay(FRAME_DELAY_MS - elapsed);
        }
        last_time = SDL_GetTicks();
    }

    simcity_recomp_destroy(g_recomp);
    g_recomp = NULL;

    if (g_gamepad) {
        SDL_GameControllerClose(g_gamepad);
        g_gamepad = NULL;
    }

    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    
    if (g_audio_mutex) {
        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;
    }
    
    SDL_Quit();

    return 0;
}