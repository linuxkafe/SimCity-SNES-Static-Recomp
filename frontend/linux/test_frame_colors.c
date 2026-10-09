/*
 * test_frame_colors.c — end-to-end colour check through the real core.
 *
 * Two earlier checks each passed while the game still showed swapped red and
 * blue:
 *
 *   1. Reading SDL's declared channel masks. That only proves the masks match,
 *      never that a pixel survives upload.
 *   2. A synthetic upload of the literal value 0xFFFF0000. That proves SDL is
 *      sane but says nothing about whether the core actually emits 0xAARRGGBB
 *      for a pixel that looks red.
 *
 * So this drives the real pipeline: run the core on the real ROM, take the
 * framebuffer it produces, upload it exactly as the frontend does, read it
 * back, and require that every pixel keeps the same channel order. Needs
 * SIMCITY_TEST_ROM.
 */

#include <SDL.h>

#include "simcity_static_recomp.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned unpack_r(Uint32 p) { return (p >> 16) & 0xFFu; }
static unsigned unpack_g(Uint32 p) { return (p >> 8) & 0xFFu; }
static unsigned unpack_b(Uint32 p) { return p & 0xFFu; }

int main(int argc, char **argv)
{
    SimCityRecomp *recomp;
    SimCityRecompFrameResult result;
    const uint32_t *frame;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *source, *target;
    SDL_Surface *out;
    unsigned width, height, x, y;
    size_t rom_size, bad = 0, checked = 0;
    char err[256] = {0};
    int rc = 1;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom.sfc>\n", argv[0]);
        return 2;
    }

    rom_size = (size_t)SIMCITY_RECOMP_ROM_SIZE;
    {
        FILE *f = fopen(argv[1], "rb");
        uint8_t *rom;
        if (!f) { perror(argv[1]); return 2; }
        rom = (uint8_t *)malloc(rom_size);
        if (!rom || fread(rom, 1, rom_size, f) != rom_size) {
            fprintf(stderr, "ROM is not %zu bytes\n", rom_size);
            fclose(f); free(rom); return 2;
        }
        fclose(f);

        if (simcity_recomp_create(&recomp, rom, rom_size,
                                  err, sizeof(err)) != 1) {
            fprintf(stderr, "create failed: %s\n", err);
            free(rom);
            return 2;
        }
        free(rom);
    }
    if (!recomp) return 2;
    if (simcity_recomp_reset(recomp, err, sizeof(err)) != 1) {
        fprintf(stderr, "reset failed: %s\n", err);
        simcity_recomp_destroy(recomp);
        return 2;
    }

    memset(&result, 0, sizeof(result));
    for (int i = 0; i < 240; ++i) {
        if (simcity_recomp_advance(recomp, 0u, 1u, &result) != 1) {
            fprintf(stderr, "advance %d failed: %s\n", i,
                    simcity_recomp_last_error(recomp));
            simcity_recomp_destroy(recomp);
            return 2;
        }
    }
    if (simcity_recomp_render_current_frame(recomp, NULL, 0) != 1) {
        fprintf(stderr, "render failed: %s\n", simcity_recomp_last_error(recomp));
        simcity_recomp_destroy(recomp);
        return 2;
    }

    frame = simcity_recomp_frame_bgra(recomp);
    if (!frame) { fprintf(stderr, "no frame\n"); simcity_recomp_destroy(recomp); return 2; }
    width  = simcity_recomp_widescreen_enabled(recomp)
                 ? SIMCITY_RECOMP_WIDESCREEN_WIDTH : SIMCITY_RECOMP_FRAME_WIDTH;
    height = SIMCITY_RECOMP_FRAME_HEIGHT;

    /* Sample the core's own output on a grid. Report the strongest red-dominant
       and blue-dominant pixels so a channel swap is visible in the log even if
       the assertion below is only summarised. */
    {
        Uint32 best_red = 0, best_blue = 0;
        int red_score = -1, blue_score = -1;
        for (y = 0; y < height; y += 7) {
            for (x = 0; x < width; x += 7) {
                Uint32 p = frame[y * width + x];
                int rs = (int)unpack_r(p) - (int)unpack_b(p);
                int bs = (int)unpack_b(p) - (int)unpack_r(p);
                if (rs > red_score)  { red_score = rs;  best_red = p; }
                if (bs > blue_score) { blue_score = bs; best_blue = p; }
            }
        }
        fprintf(stderr,
                "core framebuffer: most red-dominant 0x%08X -> r=%u g=%u b=%u\n",
                best_red, unpack_r(best_red), unpack_g(best_red), unpack_b(best_red));
        fprintf(stderr,
                "core framebuffer: most blue-dominant 0x%08X -> r=%u g=%u b=%u\n",
                best_blue, unpack_r(best_blue), unpack_g(best_blue), unpack_b(best_blue));
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        simcity_recomp_destroy(recomp);
        return 2;
    }
    window = SDL_CreateWindow("frame-colors", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    source = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         (int)width, (int)height) : NULL;
    target = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_TARGET,
                                         (int)width, (int)height) : NULL;
    out = renderer ? SDL_CreateRGBSurfaceWithFormat(0, (int)width, (int)height,
                                                   32, SDL_PIXELFORMAT_ARGB8888)
                   : NULL;
    if (!window || !renderer || !source || !target || !out) {
        fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
        simcity_recomp_destroy(recomp);
        return 2;
    }

    /* Exactly what render_frame() in main.c does. */
    SDL_UpdateTexture(source, NULL, frame, (int)(width * sizeof(uint32_t)));

    SDL_SetRenderTarget(renderer, target);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, source, NULL, NULL);
    SDL_RenderPresent(renderer);
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             out->pixels, out->pitch) != 0) {
        fprintf(stderr, "read back failed: %s\n", SDL_GetError());
        simcity_recomp_destroy(recomp);
        return 2;
    }

    for (y = 0; y < height; y += 3) {
        for (x = 0; x < width; x += 3) {
            Uint32 src = frame[y * width + x];
            Uint32 dst = ((Uint32 *)out->pixels)[y * (width) + x];
            checked++;
            if (unpack_r(src) != unpack_r(dst) ||
                unpack_g(src) != unpack_g(dst) ||
                unpack_b(src) != unpack_b(dst)) {
                if (bad < 5) {
                    fprintf(stderr,
                            "mismatch at %u,%u: core 0x%08X (r%u g%u b%u) -> "
                            "screen 0x%08X (r%u g%u b%u)\n",
                            x, y, src, unpack_r(src), unpack_g(src), unpack_b(src),
                            dst, unpack_r(dst), unpack_g(dst), unpack_b(dst));
                }
                bad++;
            }
        }
    }

    /* Dump the frame the core produced as a PPM so it can be looked at, not
       only compared against itself. Colors swapped inside the core would pass
       every comparison above and still be wrong on screen. */
    {
        FILE *f = fopen("/tmp/simcity-frame.ppm", "wb");
        if (f) {
            fprintf(f, "P6\n%u %u\n255\n", width, height);
            for (y = 0; y < height; ++y) {
                for (x = 0; x < width; ++x) {
                    Uint32 p = frame[y * width + x];
                    unsigned char rgb[3];
                    rgb[0] = (unsigned char)unpack_r(p);
                    rgb[1] = (unsigned char)unpack_g(p);
                    rgb[2] = (unsigned char)unpack_b(p);
                    fwrite(rgb, 1, 3, f);
                }
            }
            fclose(f);
            fprintf(stderr, "wrote /tmp/simcity-frame.ppm\n");
        }
    }

    fprintf(stderr, "compared %zu pixels, %zu mismatched\n", checked, bad);
    if (bad) {
        fprintf(stderr, "FAIL: the core's framebuffer does not reach the screen "
                        "unchanged\n");
    } else {
        fprintf(stderr, "OK: core framebuffer reaches the screen unchanged\n");
        rc = 0;
    }

    SDL_FreeSurface(out);
    SDL_DestroyTexture(source);
    SDL_DestroyTexture(target);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    simcity_recomp_destroy(recomp);
    return rc;
}