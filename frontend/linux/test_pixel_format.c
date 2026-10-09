/*
 * test_pixel_format.c — pins the SDL pixel format against the core's output.
 *
 * The core's bgra() (static-recomp/src/sc_v28_video.c) returns
 *   0xFF000000 | blue | (green << 8) | (red << 16)
 * so red occupies bits 16-23 and blue bits 0-7.
 *
 * The matching SDL format is SDL_PIXELFORMAT_ARGB8888 (Rmask 0x00FF0000).
 * SDL_PIXELFORMAT_ABGR8888 has Rmask 0x000000FF and therefore renders our
 * blue byte as red, which shows as swapped red/blue on screen.
 *
 * Earlier this was checked by reading SDL's mask fields. That proved the masks
 * matched but never proved a pixel actually reached the display correctly, and
 * a swapped-colour report survived it. This version drives a real renderer into
 * an offscreen target and reads the result back, so it observes what SDL does
 * rather than what it declares.
 */

#include <SDL.h>

#include <stdio.h>

static int failures;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s\n", (msg));                             \
            failures++;                                                       \
        }                                                                     \
    } while (0)

/* The format the frontend uploads with, kept in one place. */
#ifndef SIMCITY_TEXTURE_PIXELFORMAT
#define SIMCITY_TEXTURE_PIXELFORMAT SDL_PIXELFORMAT_ARGB8888
#endif

int main(void)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *target;
    SDL_Texture *source;
    SDL_Surface *surface;
    Uint32 pixel;
    Uint8 r, g, b, a;
    int ok;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("pixel-format-test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    CHECK(window != NULL, "window created");
    if (!window) return 1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    CHECK(renderer != NULL, "software renderer created");
    if (!renderer) return 1;

    /* Read-back surface: 32-bit so SDL_GetRGBA can decompose it. */
    surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32,
                                            SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface != NULL, "readback surface created");
    if (!surface) return 1;

    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_TARGET, 1, 1);
    source = SDL_CreateTexture(renderer, SIMCITY_TEXTURE_PIXELFORMAT,
                               SDL_TEXTUREACCESS_STREAMING, 1, 1);
    CHECK(target != NULL && source != NULL, "textures created");
    if (!target || !source) return 1;

    SDL_SetRenderTarget(renderer, target);

    /* What the core emits for a fully saturated CGRAM entry: red. */
    SDL_UpdateTexture(source, NULL, &(Uint32){0xFFFF0000u},
                      (int)sizeof(Uint32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, source, NULL, NULL);
    SDL_RenderPresent(renderer);

    ok = SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                              surface->pixels, surface->pitch);
    CHECK(ok == 0, "pixels read back");

    if (ok == 0) {
        pixel = *(Uint32 *)surface->pixels;
        /* surface->format is ARGB8888 by construction above, so unpack by
           shift rather than via SDL_GetRGBA, which segfaults without a live
           video subsystem. */
        a = (Uint8)(pixel >> 24); r = (Uint8)(pixel >> 16);
        g = (Uint8)(pixel >> 8);  b = (Uint8)pixel;
        fprintf(stderr, "core red 0xFFFF0000 rendered as (%u,%u,%u,%u)\n",
                r, g, b, a);
        if (!(r == 255 && g == 0 && b == 0)) {
            fprintf(stderr, "  expected (255,0,0): red and blue are swapped\n");
            failures++;
        }

        /* And the same for blue, to catch a format that is merely wrong. */
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(source, NULL, &(Uint32){0xFF0000FFu},
                          (int)sizeof(Uint32));
        SDL_RenderCopy(renderer, source, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch);
        pixel = *(Uint32 *)surface->pixels;
        a = (Uint8)(pixel >> 24); r = (Uint8)(pixel >> 16);
        g = (Uint8)(pixel >> 8);  b = (Uint8)pixel;
        fprintf(stderr, "core blue 0xFF0000FF rendered as (%u,%u,%u,%u)\n",
                r, g, b, a);
        if (!(r == 0 && g == 0 && b == 255)) {
            fprintf(stderr, "  expected (0,0,255): red and blue are swapped\n");
            failures++;
        }
    }

    SDL_DestroyTexture(source);
    SDL_DestroyTexture(target);
    SDL_FreeSurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (failures) {
        fprintf(stderr, "\n%d pixel format failure(s)\n", failures);
        return 1;
    }
    printf("OK: core colours survive SDL upload and render unchanged\n");
    return 0;
}