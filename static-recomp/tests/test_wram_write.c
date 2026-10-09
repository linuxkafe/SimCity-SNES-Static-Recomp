/*
 * test_wram_write.c — contract for the host-side WRAM write API.
 *
 * simcity_recomp_write_wram() lets a frontend apply cheats and mods between
 * frames. Unlike simcity_recomp_read_wram() it mutates emulated state, so the
 * parts that can go wrong are the bounds check and a partial write leaving the
 * image half-modified. This test pins the contract; it does not try to judge
 * gameplay effects, which need a real session to observe.
 */

#include "simcity_static_recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s\n", (msg));                             \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static unsigned read_u24(SimCityRecomp *instance, size_t offset)
{
    uint8_t bytes[3];
    if (!simcity_recomp_read_wram(instance, (uint32_t)offset, bytes, 3u))
        return 0u;
    return (unsigned)(bytes[0] | ((unsigned)bytes[1] << 8) |
                      ((unsigned)bytes[2] << 16));
}

int main(int argc, char **argv)
{
    FILE *rom_file;
    long rom_size;
    uint8_t *rom;
    SimCityRecomp *instance = NULL;
    char error[256];
    uint8_t probe[8];
    uint8_t mirror[8];
    uint8_t money[3];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom-path>\n", argv[0]);
        return 1;
    }

    rom_file = fopen(argv[1], "rb");
    if (!rom_file) {
        fprintf(stderr, "Cannot open ROM: %s\n", argv[1]);
        return 1;
    }
    fseek(rom_file, 0, SEEK_END);
    rom_size = ftell(rom_file);
    fseek(rom_file, 0, SEEK_SET);
    if (rom_size != (long)SIMCITY_RECOMP_ROM_SIZE) {
        fprintf(stderr, "ROM size mismatch: expected %u, got %ld\n",
                SIMCITY_RECOMP_ROM_SIZE, rom_size);
        fclose(rom_file);
        return 1;
    }
    rom = (uint8_t *)malloc((size_t)rom_size);
    if (!rom || fread(rom, 1, (size_t)rom_size, rom_file) != (size_t)rom_size) {
        fprintf(stderr, "ROM read failed\n");
        free(rom);
        fclose(rom_file);
        return 1;
    }
    fclose(rom_file);

    memset(error, 0, sizeof(error));
    if (simcity_recomp_create(&instance, rom, (size_t)rom_size,
                              error, sizeof(error)) != 1) {
        fprintf(stderr, "create failed: %s\n", error);
        free(rom);
        return 1;
    }
    free(rom);

    if (simcity_recomp_reset(instance, error, sizeof(error)) != 1) {
        fprintf(stderr, "reset failed: %s\n", error);
        simcity_recomp_destroy(instance);
        return 1;
    }

    memset(probe, 0, sizeof(probe));

    /* Argument and bounds contract. A rejected call must write nothing. */
    CHECK(simcity_recomp_write_wram(instance, 0x0B9Du, probe, 1u) == 1,
          "in-range write is accepted");
    CHECK(simcity_recomp_write_wram(NULL, 0u, probe, 1u) == 0,
          "NULL instance is rejected");
    CHECK(simcity_recomp_write_wram(instance, 0u, NULL, 1u) == 0,
          "NULL source is rejected");
    CHECK(simcity_recomp_write_wram(instance, 0x20000u, probe, 1u) == 0,
          "offset at 128 KiB is rejected");
    CHECK(simcity_recomp_write_wram(instance, 0x1FFFFu, probe, 4u) == 0,
          "range straddling the end is rejected");
    CHECK(simcity_recomp_write_wram(instance, 0x20000u, probe, 0u) == 1,
          "zero length at the end is accepted and writes nothing");
    CHECK(simcity_recomp_write_wram(instance, 0u, probe, 0u) == 1,
          "zero length at the start is accepted");

    memset(probe, 0xAA, sizeof(probe));
    simcity_recomp_read_wram(instance, 0x1FFF8u, mirror, sizeof(mirror));
    CHECK(simcity_recomp_write_wram(instance, 0x1FFFFu, probe, 4u) == 0,
          "straddling write is still rejected");
    memset(probe, 0, sizeof(probe));
    simcity_recomp_read_wram(instance, 0x1FFF8u, probe, sizeof(probe));
    CHECK(memcmp(probe, mirror, sizeof(mirror)) == 0,
          "a rejected write leaves WRAM byte-for-byte unchanged");

    /* Round trip: what the host writes is what the core will read next frame. */
    money[0] = 0x3Fu;
    money[1] = 0x42u;
    money[2] = 0x0Fu;
    CHECK(simcity_recomp_write_wram(instance, 0x0B9Du, money, 3u) == 1,
          "funds write accepted");
    memset(probe, 0, sizeof(probe));
    simcity_recomp_read_wram(instance, 0x0B9Du, probe, 3u);
    CHECK(memcmp(money, probe, 3u) == 0, "funds round trip byte for byte");
    CHECK(read_u24(instance, 0x0B9Du) == 0x0F423Fu,
          "funds read back as 999999");

    /* The poke must not wedge the static route. */
    {
        SimCityRecompFrameResult result;
        int advanced = 0;
        int i;
        for (i = 0; i < 60; ++i) {
            memset(&result, 0, sizeof(result));
            if (simcity_recomp_advance_headless(instance, 0u, 1u, &result) != 1)
                break;
            simcity_recomp_audio_discard(instance);
            advanced++;
        }
        CHECK(advanced == 60, "route still advances after a write");
        CHECK(simcity_recomp_failed(instance) == 0,
              "route did not fail after a write");
    }

    simcity_recomp_destroy(instance);

    if (failures) {
        fprintf(stderr, "\n%d WRAM write test failure(s)\n", failures);
        return 1;
    }
    printf("OK: simcity_recomp_write_wram contract holds\n");
    return 0;
}