/*
 * headless-test.c — advance N frames of the static-recomp core without
 * opening a window or audio device. Use for CI smoke tests and headless
 * verification on machines without a display server.
 *
 * Usage: headless-test <rom-path> [frames]
 *   frames defaults to 300.
 *
 * Exits 0 when the requested frames advance without a route failure;
 * exits 1 on any error. Audio is silently discarded each frame.
 */

#include "simcity_static_recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s <rom-path> [frames]\n", argv0);
    fprintf(stderr, "  frames defaults to 300\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    int frames = 300;
    if (argc >= 3) {
        frames = atoi(argv[2]);
        if (frames <= 0) {
            fprintf(stderr, "Invalid frame count: %s\n", argv[2]);
            return 1;
        }
    }

    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open ROM: %s\n", rom_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long rom_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (rom_len != (long)SIMCITY_RECOMP_ROM_SIZE) {
        fprintf(stderr, "ROM size mismatch: expected %u, got %ld\n",
                SIMCITY_RECOMP_ROM_SIZE, (long)rom_len);
        fclose(f);
        return 1;
    }

    uint8_t *rom = (uint8_t *)malloc(SIMCITY_RECOMP_ROM_SIZE);
    if (!rom) {
        fprintf(stderr, "ROM allocation failed\n");
        fclose(f);
        return 1;
    }

    size_t read = fread(rom, 1, SIMCITY_RECOMP_ROM_SIZE, f);
    fclose(f);

    if (read != (size_t)SIMCITY_RECOMP_ROM_SIZE) {
        fprintf(stderr, "ROM read failed: expected %u, got %zu\n",
                SIMCITY_RECOMP_ROM_SIZE, read);
        free(rom);
        return 1;
    }

    SimCityRecomp *recomp = NULL;
    char error[256];
    memset(error, 0, sizeof(error));

    if (simcity_recomp_create(&recomp, rom, SIMCITY_RECOMP_ROM_SIZE,
                              error, sizeof(error)) != 1) {
        fprintf(stderr, "simcity_recomp_create failed: %s\n", error);
        free(rom);
        return 1;
    }
    free(rom);

    if (simcity_recomp_reset(recomp, error, sizeof(error)) != 1) {
        fprintf(stderr, "simcity_recomp_reset failed: %s\n", error);
        simcity_recomp_destroy(recomp);
        return 1;
    }

    uint16_t input_mask = 0;
    int failed = 0;

    for (int i = 0; i < frames; i++) {
        SimCityRecompFrameResult result;
        memset(&result, 0, sizeof(result));

        if (simcity_recomp_advance(recomp, input_mask, 1, &result) != 1) {
            fprintf(stderr, "Frame %d: %s\n", i + 1,
                    simcity_recomp_last_error(recomp));
            failed = 1;
            break;
        }

        if (result.frame_rendered) {
            if (simcity_recomp_render_current_frame(recomp, error,
                                                    sizeof(error)) != 1) {
                fprintf(stderr, "Frame %d render: %s\n", i + 1, error);
                failed = 1;
                break;
            }
        }

        /* Discard any produced audio */
        simcity_recomp_audio_discard(recomp);
    }

    if (!failed) {
        printf("OK: %d frames advanced successfully\n", frames);
        printf("Current frame: %u\n",
               (unsigned)simcity_recomp_current_frame(recomp));
        printf("Instructions: %llu\n",
               (unsigned long long)simcity_recomp_instruction_count(recomp));
        printf("Master clock: %llu\n",
               (unsigned long long)simcity_recomp_master_clock(recomp));
    }

    simcity_recomp_destroy(recomp);
    return failed ? 1 : 0;
}