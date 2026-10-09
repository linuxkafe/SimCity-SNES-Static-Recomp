/*
 * test_pixel_format.c — pins the SDL pixel format against the core's output.
 *
 * The core emits uint32 0xAARRGGBB: blue in bits 0-7, green 8-15, red 16-23.
 * The SDL texture format must carry matching channel masks or red and blue are
 * silently swapped on screen. That regression happened once already
 * (SDL_PIXELFORMAT_ABGR8888 reads bits 0-7 as red), and it is invisible in a
 * headless test because nothing renders.
 *
 * This checks the masks SDL actually reports, not the format name.
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
    SDL_PixelFormat *format;

    /* The core's bgra() emits 0xAARRGGBB. Stated as masks:
     *   red   occupies bits 16-23  -> 0x00FF0000
     *   green occupies bits  8-15  -> 0x0000FF00
     *   blue  occupies bits  0-7   -> 0x000000FF
     * The SDL format must report exactly those masks. Comparing masks avoids
     * SDL_GetRGBA, which is not safe to call without a live video subsystem. */
    const Uint32 want_r = 0x00FF0000u;
    const Uint32 want_g = 0x0000FF00u;
    const Uint32 want_b = 0x000000FFu;

    format = SDL_AllocFormat(SIMCITY_TEXTURE_PIXELFORMAT);
    CHECK(format != NULL, "pixel format allocated");
    if (!format) return 1;

    CHECK(format->Rmask == want_r, "red occupies bits 16-23");
    CHECK(format->Gmask == want_g, "green occupies bits 8-15");
    CHECK(format->Bmask == want_b, "blue occupies bits 0-7");

    if (format->Rmask != want_r || format->Gmask != want_g ||
        format->Bmask != want_b) {
        fprintf(stderr,
                "channel layout is R=%08X G=%08X B=%08X, core needs "
                "R=%08X G=%08X B=%08X -- red and blue are swapped\n",
                format->Rmask, format->Gmask, format->Bmask,
                want_r, want_g, want_b);
        failures++;
    }

    SDL_FreeFormat(format);

    if (failures) {
        fprintf(stderr, "\n%d pixel format failure(s)\n", failures);
        return 1;
    }
    printf("OK: SDL pixel format matches the core's 0xAARRGGBB layout\n");
    return 0;
}
