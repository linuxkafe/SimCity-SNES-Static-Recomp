#include "simcity_static_recomp.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 480

static SimCityRecomp *g_recomp = NULL;
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static int g_running = 1;
static uint32_t g_frame_count = 0;

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

    g_window = SDL_CreateWindow("SimCity SNES Static Recomp",
                                 100, 100,
                                 WINDOW_WIDTH, WINDOW_HEIGHT,
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

    g_texture = SDL_CreateTexture(g_renderer,
                                   SDL_PIXELFORMAT_ABGR8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   SIMCITY_RECOMP_FRAME_WIDTH,
                                   SIMCITY_RECOMP_FRAME_HEIGHT);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = SIMCITY_RECOMP_HOST_AUDIO_SAMPLE_RATE;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = audio_callback;

    if (SDL_OpenAudio(&spec, NULL) != 0) {
        fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudio(0);

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

    int w, h;
    SDL_GetWindowSize(g_window, &w, &h);

    SDL_UpdateTexture(g_texture, NULL, bgra,
                      (int)(simcity_recomp_frame_width(g_recomp) * sizeof(uint32_t)));

    SDL_RenderClear(g_renderer);

    SDL_Rect dst = {0, 0, w, h};
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);

    SDL_RenderPresent(g_renderer);
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            rom_path = argv[++i];
        }
    }

    if (!rom_path) {
        rom_path = getenv("SIMCITY_ROM_PATH");
    }

    if (!rom_path) {
        fprintf(stderr, "Usage: %s --rom <path/to/rom.sfc>\n", argv[0]);
        fprintf(stderr, "Or set SIMCITY_ROM_PATH environment variable.\n");
        return 1;
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

    if (init_sdl() != 0) {
        simcity_recomp_destroy(g_recomp);
        return 1;
    }

    if (simcity_recomp_reset(g_recomp, error, sizeof(error)) != 0) {
        fprintf(stderr, "simcity_recomp_reset failed: %s\n", error);
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

    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}