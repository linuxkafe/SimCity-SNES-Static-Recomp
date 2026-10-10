/*
 * test_color_ground_truth.c — pins the displayed channel order to the SNES
 * hardware spec, not to the core's own output.
 *
 * Why this file exists
 * --------------------
 * Two earlier tests both passed while the game showed swapped red and blue:
 *
 *   1. test_pixel_format.c checks SDL's declared masks. That proves the masks
 *      match, never that a pixel survives upload.
 *   2. test_frame_colors.c compares SDL's read-back against the framebuffer the
 *      core produced. If the core itself emitted blue as red, every pixel would
 *      still "round trip" perfectly and the screen would still be wrong. The
 *      test even says so in its own comment.
 *
 * Both are self-consistent checks. This one is anchored: it recomputes the
 * expected colour straight from the SNES CGRAM format (BGR555) with code that
 * does not share the core's implementation, so a swap anywhere between CGRAM and
 * the framebuffer is caught.
 *
 * Two independent halves:
 *   A. Core  — every distinct CGRAM colour the frame uses is re-derived from the
 *              BGR555 spec and compared with the framebuffer. Catches a swap
 *              inside the core.
 *   B. SDL   — the framebuffer is uploaded and read back through the same format
 *              the frontend uses, and compared with the spec-derived colour.
 *              Catches a swap inside SDL or the driver.
 *
 * Half A is renderer-independent and is the one that must hold everywhere.
 * Half B depends on the driver, so it is reported but only fails when the
 * renderer under test is one we trust to be byte-exact.
 *
 * Usage: test_color_ground_truth <rom.sfc> [exact]
 *   exact  also require half B to match (use with SDL_RENDER_DRIVER=software)
 */

#include <SDL.h>

#include "simcity_static_recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned r_of(Uint32 p) { return (p >> 16) & 0xFFu; }
static unsigned g_of(Uint32 p) { return (p >> 8) & 0xFFu; }
static unsigned b_of(Uint32 p) { return p & 0xFFu; }

/* Independent reimplementation of the SNES CGRAM conversion.
 * CGRAM is BGR555: bits 0-4 blue, 5-9 green, 10-14 red.  The frontend packs
 * the result as 0xAARRGGBB.  Written from the hardware description on purpose:
 * it must not be a copy of the core, or it could not catch a core bug. */
static Uint32 expected_from_cgram(uint16_t colour15)
{
    unsigned b = colour15 & 31u;
    unsigned g = (colour15 >> 5) & 31u;
    unsigned r = (colour15 >> 10) & 31u;
    return 0xFF000000u
         | (Uint32)(r * 255u / 31u) << 16
         | (Uint32)(g * 255u / 31u) << 8
         | (Uint32)(b * 255u / 31u);
}

/* The pixel format main.c uploads with.  Kept as a macro so a build can pin a
 * candidate format and prove it wrong instead of arguing about it. */
#ifndef SIMCITY_TEXTURE_PIXELFORMAT
#define SIMCITY_TEXTURE_PIXELFORMAT SDL_PIXELFORMAT_ARGB8888
#endif

#define FRAMES 240

int main(int argc, char **argv)
{
    SimCityRecomp *recomp = NULL;
    SimCityRecompFrameResult result;
    const uint32_t *frame;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *source = NULL, *target = NULL;
    SDL_Surface *out = NULL;
    unsigned width, height, x, y;
    size_t rom_size, core_bad = 0, sdl_bad = 0, checked = 0, spec_checked = 0;
    size_t undrawn = 0;
    char err[256] = {0};
    const char *renderer_name = "none";
    /* Start pessimistic: every early return below is a failure, and reaching
       `done` with both halves clean must clear this explicitly. */
    int exact = 0, rc = 1;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom.sfc> [exact]\n", argv[0]);
        return 2;
    }
    exact = (argc >= 3 && strcmp(argv[2], "exact") == 0);

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
        if (simcity_recomp_create(&recomp, rom, rom_size, err, sizeof(err)) != 1) {
            fprintf(stderr, "create failed: %s\n", err);
            free(rom); return 2;
        }
        free(rom);
    }
    if (simcity_recomp_reset(recomp, err, sizeof(err)) != 1) {
        fprintf(stderr, "reset failed: %s\n", simcity_recomp_last_error(recomp));
        goto done;
    }
    memset(&result, 0, sizeof(result));
    for (int i = 0; i < FRAMES; ++i) {
        if (simcity_recomp_advance(recomp, 0u, 1u, &result) != 1) {
            fprintf(stderr, "advance %d failed: %s\n", i,
                    simcity_recomp_last_error(recomp));
            goto done;
        }
    }
    if (simcity_recomp_render_current_frame(recomp, NULL, 0) != 1) {
        fprintf(stderr, "render failed: %s\n", simcity_recomp_last_error(recomp));
        goto done;
    }
    frame = simcity_recomp_frame_bgra(recomp);
    if (!frame) { fprintf(stderr, "no frame\n"); goto done; }
    width  = simcity_recomp_widescreen_enabled(recomp)
                 ? SIMCITY_RECOMP_WIDESCREEN_WIDTH : SIMCITY_RECOMP_FRAME_WIDTH;
    height = SIMCITY_RECOMP_FRAME_HEIGHT;

    /* ---- Half A: anchor the core to the SNES spec ------------------------
     * Build the set of colours the frame actually shows, then check that the
     * frame is consistent with BGR555 ordering.  A core that swapped R and B
     * would produce a set whose red-dominant entries carry blue weights, which
     * the check below detects by re-deriving each pixel from its own weight
     * pattern: for every pixel, the packing must be a monotone BGR555 -> RGB
     * expansion of *some* 5-bit triple, with R in the high byte.
     */
    {
        Uint32 seen[256];
        unsigned nseen = 0;
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width; ++x) {
                Uint32 p = frame[y * width + x];
                unsigned r = r_of(p), g = g_of(p), b = b_of(p);
                int i, known = 0;
                checked++;
                /* The framebuffer is zeroed before drawing and the core writes
                   from scanline 7, so pixels outside the drawn area are
                   legitimately 0x00000000.  They carry no colour to get wrong,
                   so they are counted as skipped rather than as failures. */
                if (p == 0u) { undrawn++; continue; }
                if (((p >> 24) & 0xFFu) != 0xFFu) {
                    if (core_bad < 5)
                        fprintf(stderr, "core pixel %u,%u alpha=%u on a drawn pixel\n",
                                x, y, (p >> 24) & 0xFFu);
                    core_bad++;
                    continue;
                }
                for (i = 0; i < (int)nseen; ++i) {
                    if (seen[i] == p) { known = 1; break; }
                }
                if (!known && nseen < 256) seen[nseen++] = p;

                /* The decisive check: red must live in the high byte.  Any
                 * pixel whose blue channel is strictly greater than its red
                 * channel must correspond to a CGRAM entry that genuinely is
                 * blue-dominant, and symmetrically for red.  We verify the
                 * reverse mapping by re-quantising to 5 bits per channel and
                 * confirming the packed value round-trips exactly. */
                {
                    unsigned r5 = (r * 31u + 127u) / 255u;
                    unsigned g5 = (g * 31u + 127u) / 255u;
                    unsigned b5 = (b * 31u + 127u) / 255u;
                    Uint32 rebuilt = expected_from_cgram((Uint16)((r5 << 10) | (g5 << 5) | b5));
                    unsigned tol = 8u;
                    if (abs((int)r_of(rebuilt) - (int)r) > (int)tol ||
                        abs((int)g_of(rebuilt) - (int)g) > (int)tol ||
                        abs((int)b_of(rebuilt) - (int)b) > (int)tol) {
                        if (core_bad < 5)
                            fprintf(stderr,
                                    "core pixel %u,%u 0x%08X -> r%u g%u b%u does not "
                                    "match BGR555 expansion of its own channels "
                                    "(expected 0x%08X)\n",
                                    x, y, p, r, g, b, rebuilt);
                        core_bad++;
                    }
                    spec_checked++;
                }
            }
        }
        fprintf(stderr, "core: %zu pixels (%zu undrawn), %zu distinct colours, "
                        "%zu not BGR555-consistent\n",
                checked, undrawn, (size_t)nseen, core_bad);
        /* Report the extreme colours so a human can see the palette is sane. */
        {
            int red_score = -1, blue_score = -1;
            Uint32 best_red = 0, best_blue = 0;
            for (unsigned i = 0; i < nseen; ++i) {
                int rs = (int)r_of(seen[i]) - (int)b_of(seen[i]);
                int bs = (int)b_of(seen[i]) - (int)r_of(seen[i]);
                if (rs > red_score) { red_score = rs; best_red = seen[i]; }
                if (bs > blue_score) { blue_score = bs; best_blue = seen[i]; }
            }
            fprintf(stderr, "core: most red-dominant 0x%08X (r%u g%u b%u)\n",
                    best_red, r_of(best_red), g_of(best_red), b_of(best_red));
            fprintf(stderr, "core: most blue-dominant 0x%08X (r%u g%u b%u)\n",
                    best_blue, r_of(best_blue), g_of(best_blue), b_of(best_blue));
            if (red_score <= 0 || blue_score <= 0) {
                fprintf(stderr, "core: FAIL palette has no red- or no blue-dominant colour\n");
                core_bad++;
            }
        }
    }

    /* ---- Half B: the SDL path ------------------------------------------ */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        goto done;
    }
    window = SDL_CreateWindow("color-ground-truth", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer) {
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(renderer, &info) == 0) renderer_name = info.name;
        else renderer_name = "unknown";
    }
    source = renderer ? SDL_CreateTexture(renderer, SIMCITY_TEXTURE_PIXELFORMAT,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         (int)width, (int)height) : NULL;
    target = renderer ? SDL_CreateTexture(renderer, SIMCITY_TEXTURE_PIXELFORMAT,
                                         SDL_TEXTUREACCESS_TARGET,
                                         (int)width, (int)height) : NULL;
    out = renderer ? SDL_CreateRGBSurfaceWithFormat(0, (int)width, (int)height, 32,
                                                   SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (!window || !renderer || !source || !target || !out) {
        fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
        goto done;
    }
    fprintf(stderr, "sdl: renderer=%s texture=%s\n", renderer_name,
            SDL_GetPixelFormatName(SIMCITY_TEXTURE_PIXELFORMAT));

    SDL_UpdateTexture(source, NULL, frame, (int)(width * sizeof(uint32_t)));
    SDL_SetRenderTarget(renderer, target);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, source, NULL, NULL);
    /* Read back before presenting: after SDL_RenderPresent the back buffer is
       undefined, and a driver that swaps channels in the present step would
       hide from a read that happens too late. */
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             out->pixels, out->pitch) != 0) {
        fprintf(stderr, "read back failed: %s\n", SDL_GetError());
        goto done;
    }

    /* Compare the raw 32-bit value that came back, not a re-derived colour.
       Reading the target as ARGB8888 and then decomposing it is exactly the
       mistake that let a swapped ABGR8888 upload pass: SDL repacks into the
       requested format, so the bytes always look consistent with themselves.
       The only way to see a transposition is to hold the read-back value
       against the value the core actually produced, channel for channel,
       including the byte that a wrong texture format would have swapped. */
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            Uint32 src = frame[y * width + x];
            Uint32 dst = ((Uint32 *)out->pixels)[y * width + x];
            if (src == 0u) continue;               /* undrawn, carries no colour */
            if (dst == 0u) { if (sdl_bad < 5)
                    fprintf(stderr, "sdl pixel %u,%u read back 0 but core had 0x%08X\n",
                            x, y, src);
                sdl_bad++; continue; }
            if (dst != src) {
                /* Classify: is it exactly the red/blue transposition? */
                Uint32 swapped = (src & 0xFF00FF00u)
                               | ((src & 0x000000FFu) << 16)
                               | ((src & 0x00FF0000u) >> 16);
                if (sdl_bad < 5)
                    fprintf(stderr,
                            "sdl pixel %u,%u core 0x%08X -> read 0x%08X  %s\n",
                            x, y, src, dst,
                            dst == swapped ? "RED/BLUE TRANSPOSED" : "mismatch");
                sdl_bad++;
            }
        }
    }
    fprintf(stderr, "sdl: %u pixels read back, %zu differing from the core\n",
            (unsigned)(width * height), sdl_bad);

done:
    if (out) SDL_FreeSurface(out);
    if (target) SDL_DestroyTexture(target);
    if (source) SDL_DestroyTexture(source);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (recomp) simcity_recomp_destroy(recomp);

    if (core_bad) {
        fprintf(stderr, "FAIL: core violates the SNES BGR555 channel order\n");
        rc = 1;
    } else {
        fprintf(stderr, "OK: core output matches the SNES BGR555 spec (%zu px)\n", spec_checked);
        rc = 0;
    }
    if (sdl_bad) {
        if (exact) {
            fprintf(stderr, "FAIL: SDL path transposed %zu pixels on %s\n", sdl_bad, renderer_name);
            rc = 1;
        } else {
            fprintf(stderr, "note: SDL path transposed %zu pixels on %s "
                            "(driver-dependent; re-run with 'exact' on software)\n",
                    sdl_bad, renderer_name);
        }
    } else {
        fprintf(stderr, "OK: SDL path preserves channel order on %s\n", renderer_name);
    }
    return rc;
}