/*
 * headless-profile.c — Profile a headless run to find where emulated time
 * goes, with scripted controller input so the run can reach menus, terrain
 * generation and the city view instead of idling on the title screen.
 *
 * Usage: headless-profile <rom-path> [frames] [options]
 *   --input SPEC     frame-indexed input transitions, e.g.
 *                    "60:START,62:0,180:START+DOWN,182:0"
 *                    Applied from that frame onward until the next transition.
 *   --shots DIR      write frame_XXXX.bmp screenshots into DIR
 *   --shot-every N   also write a screenshot every N frames (default 30)
 *   --window A:B     reset the profiler at frame A, dump it at frame B
 *   --dump-pc N      rows to print from the 1 KiB PC histogram (default 30)
 *
 * Prints one timing block per 50 frames (host us/frame and instructions/frame)
 * so a slow phase is visible as a spike, and dumps the PC histogram for the
 * profiled window.
 */

#define _POSIX_C_SOURCE 199309L

#include "simcity_static_recomp.h"
#include "sc_simulation_profiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#define MAX_TRANSITIONS 64

typedef struct {
    uint32_t frame;
    uint16_t mask;
} InputTransition;

static InputTransition g_transitions[MAX_TRANSITIONS];
static int g_transition_count;
static const char *g_shot_dir = NULL;
static int g_shot_every = 30;
static int g_window_start = -1;
static int g_window_end = -1;
static uint32_t g_dump_pc_rows = 30;
static int g_no_pc_profile = 0;
static int g_widescreen = 0;
static int g_print_hash = 0;
static int g_per_frame = 0;
static int g_no_render = 0;
static double advance_seconds = 0.0;
static double render_seconds = 0.0;
static uint32_t g_watch_offset[4];
static int g_watch_count = 0;
static uint16_t g_watch_last[4];
static int g_watch_have[4];
static int g_ascii_frame[8];
static int g_ascii_count = 0;
static int g_zoom_frame[4];
static int g_zoom_rect[4][4];
static int g_zoom_count = 0;
static int g_wram_frame[8];
static char g_wram_path[8][256];
static int g_wram_count = 0;
static uint32_t g_last_hash;
static int g_have_hash = 0;

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static uint16_t parse_button_names(const char *text)
{
    uint16_t mask = 0;
    char token[32];
    const char *p = text;
    while (*p) {
        size_t len = 0;
        while (p[len] && p[len] != '+' && p[len] != ',') len++;
        if (len >= sizeof(token)) len = sizeof(token) - 1;
        memcpy(token, p, len);
        token[len] = '\0';
        for (size_t i = 0; token[i]; i++)
            if (token[i] >= 'A' && token[i] <= 'Z')
                token[i] = (char)(token[i] - 'A' + 'a');
        if (!strcmp(token, "start")) mask |= SIMCITY_INPUT_START;
        else if (!strcmp(token, "select")) mask |= SIMCITY_INPUT_SELECT;
        else if (!strcmp(token, "up")) mask |= SIMCITY_INPUT_UP;
        else if (!strcmp(token, "down")) mask |= SIMCITY_INPUT_DOWN;
        else if (!strcmp(token, "left")) mask |= SIMCITY_INPUT_LEFT;
        else if (!strcmp(token, "right")) mask |= SIMCITY_INPUT_RIGHT;
        else if (!strcmp(token, "a")) mask |= SIMCITY_INPUT_A;
        else if (!strcmp(token, "b")) mask |= SIMCITY_INPUT_B;
        else if (!strcmp(token, "x")) mask |= SIMCITY_INPUT_X;
        else if (!strcmp(token, "y")) mask |= SIMCITY_INPUT_Y;
        else if (!strcmp(token, "l")) mask |= SIMCITY_INPUT_L;
        else if (!strcmp(token, "r")) mask |= SIMCITY_INPUT_R;
        else if (!strcmp(token, "0")) { /* explicitly no buttons */ }
        else if (token[0]) {
            mask |= (uint16_t)strtoul(token, NULL, 0);
        }
        p += len;
        if (*p == '+' || *p == ',') p++;
    }
    return mask;
}

static int parse_input_spec(const char *spec)
{
    const char *p = spec;
    while (*p && g_transition_count < MAX_TRANSITIONS) {
        const char *colon = strchr(p, ':');
        const char *end;
        if (!colon) return 0;
        end = strchr(colon, ',');
        if (!end) end = p + strlen(p);
        g_transitions[g_transition_count].frame =
            (uint32_t)strtoul(p, NULL, 10);
        {
            char mask_text[128];
            size_t len = (size_t)(end - colon - 1);
            if (len >= sizeof(mask_text)) len = sizeof(mask_text) - 1;
            memcpy(mask_text, colon + 1, len);
            mask_text[len] = '\0';
            g_transitions[g_transition_count].mask =
                parse_button_names(mask_text);
        }
        g_transition_count++;
        p = end;
        if (*p == ',') p++;
    }
    return 1;
}

static uint16_t input_at(int frame)
{
    uint16_t mask = 0;
    for (int i = 0; i < g_transition_count; i++) {
        if ((int)g_transitions[i].frame <= frame)
            mask = g_transitions[i].mask;
    }
    return mask;
}

static int write_bmp(const char *path, const uint32_t *bgra,
                     uint32_t width, uint32_t height)
{
    /* 24 bpp bottom-up BMP: BGR rows padded to 4 bytes. */
    uint32_t row_bytes = (width * 3u + 3u) & ~3u;
    uint32_t data_size = row_bytes * height;
    uint32_t file_size = 54u + data_size;
    uint8_t header[54];
    uint8_t *rowbuf;
    FILE *f;

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    header[2] = (uint8_t)(file_size);
    header[3] = (uint8_t)(file_size >> 8);
    header[4] = (uint8_t)(file_size >> 16);
    header[5] = (uint8_t)(file_size >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = (uint8_t)(width);
    header[19] = (uint8_t)(width >> 8);
    header[22] = (uint8_t)(height);
    header[23] = (uint8_t)(height >> 8);
    header[26] = 1;
    header[28] = 24;
    header[34] = (uint8_t)(data_size);
    header[35] = (uint8_t)(data_size >> 8);
    header[36] = (uint8_t)(data_size >> 16);
    header[37] = (uint8_t)(data_size >> 24);

    f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(header, 1, sizeof(header), f);

    rowbuf = (uint8_t *)calloc(1, row_bytes);
    if (!rowbuf) {
        fclose(f);
        return 0;
    }
    for (uint32_t y = height; y-- > 0;) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t p = bgra[y * width + x];
            rowbuf[x * 3 + 0] = (uint8_t)(p);
            rowbuf[x * 3 + 1] = (uint8_t)(p >> 8);
            rowbuf[x * 3 + 2] = (uint8_t)(p >> 16);
        }
        fwrite(rowbuf, 1, row_bytes, f);
    }
    free(rowbuf);
    fclose(f);
    return 1;
}

static int take_shot(SimCityRecomp *recomp, int frame)
{
    char path[512];
    const uint32_t *bgra;
    if (!g_shot_dir) return 1;
    bgra = simcity_recomp_frame_bgra(recomp);
    if (!bgra) return 0;
    snprintf(path, sizeof(path), "%s/frame_%04d.bmp", g_shot_dir, frame);
    if (!write_bmp(path, bgra, simcity_recomp_frame_width(recomp),
                   239u)) {
        fprintf(stderr, "screenshot failed: %s\n", path);
        return 0;
    }
    return 1;
}

static int is_transition_frame(int frame)
{
    for (int i = 0; i < g_transition_count; i++)
        if ((int)g_transitions[i].frame == frame) return 1;
    return 0;
}

/* FNV-1a over the rendered frame: a text-only way to see when the screen
   changes without looking at screenshots. */
static uint32_t frame_hash(const uint32_t *bgra, uint32_t pixels)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < pixels; i++) {
        h ^= bgra[i];
        h *= 16777619u;
    }
    return h;
}

static void print_screen_hash(SimCityRecomp *recomp, int frame)
{
    const uint32_t *bgra = simcity_recomp_frame_bgra(recomp);
    uint32_t h;
    if (!g_print_hash || !bgra) return;
    h = frame_hash(bgra, simcity_recomp_frame_width(recomp) *
                             SIMCITY_RECOMP_FRAME_HEIGHT);
    if (g_have_hash && h == g_last_hash) return;
    printf("screen: frame %d hash %08X%s\n", frame, h,
           g_have_hash ? "" : " (first)");
    g_last_hash = h;
    g_have_hash = 1;
}

/* Coarse luminescence-to-ASCII rendition, so a text-only environment can tell
   a menu from a city map from a title logo. */
static void print_ascii(SimCityRecomp *recomp, int frame)
{
    static const char ramp[] = " .:-=+*#%@";
    const uint32_t *bgra = simcity_recomp_frame_bgra(recomp);
    uint32_t width = simcity_recomp_frame_width(recomp);
    uint32_t height = SIMCITY_RECOMP_FRAME_HEIGHT;
    uint32_t cols = 128, rows = 48;
    uint32_t cx, cy;
    if (!bgra) return;
    if (cols > width) cols = width;
    printf("\n--- ASCII frame %d (%ux%u -> %ux%u) ---\n", frame, width, height,
           cols, rows);
    for (cy = 0; cy < rows; cy++) {
        for (cx = 0; cx < cols; cx++) {
            uint32_t x0 = cx * width / cols, x1 = (cx + 1) * width / cols;
            uint32_t y0 = cy * height / rows, y1 = (cy + 1) * height / rows;
            uint64_t sum = 0;
            uint32_t n = 0;
            for (uint32_t y = y0; y < y1; y++)
                for (uint32_t x = x0; x < x1; x++) {
                    uint32_t p = bgra[y * width + x];
                    sum += ((p >> 16) & 0xFFu) + ((p >> 8) & 0xFFu) + (p & 0xFFu);
                    n++;
                }
            {
                unsigned lum = n ? (unsigned)(sum / (n * 3u)) : 0u;
                putchar(ramp[lum * (sizeof(ramp) - 2u) / 255u]);
            }
        }
        putchar('\n');
    }
}

/* One output character per source pixel over a region, at 1:1, so 8x8 text can
   be read directly by a text-only consumer. */
static void print_zoom(SimCityRecomp *recomp, int frame, const int *r)
{
    const uint32_t *bgra = simcity_recomp_frame_bgra(recomp);
    uint32_t width = simcity_recomp_frame_width(recomp);
    uint32_t height = SIMCITY_RECOMP_FRAME_HEIGHT;
    if (!bgra) return;
    printf("\n--- ZOOM frame %d x[%d,%d) y[%d,%d) ---\n", frame, r[0], r[2],
           r[1], r[3]);
    for (int y = r[1]; y < r[3] && y < (int)height; y++) {
        for (int x = r[0]; x < r[2] && x < (int)width; x++) {
            uint32_t p = bgra[(uint32_t)y * width + (uint32_t)x];
            unsigned lum = (((p >> 16) & 0xFFu) + ((p >> 8) & 0xFFu) +
                            (p & 0xFFu)) / 3u;
            putchar(lum < 140 ? ' ' : '#');
        }
        putchar('\n');
    }
}

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s <rom-path> [frames] [options]\n", argv0);
    fprintf(stderr, "  --input SPEC      \"60:START,62:0,180:START+DOWN,182:0\"\n");
    fprintf(stderr, "  --shots DIR       write BMP screenshots into DIR\n");
    fprintf(stderr, "  --shot-every N    screenshot every N frames (default 30)\n");
    fprintf(stderr, "  --window A:B      reset profiler at frame A, dump at B\n");
    fprintf(stderr, "  --dump-pc N       PC histogram rows (default 30)\n");
    fprintf(stderr, "  --no-pc-profile   disable per-instruction PC histogram (timing only)\n");
    fprintf(stderr, "  --widescreen      render the 398x239 city view (exercises city_background_pixel)\n");
    fprintf(stderr, "  --hash            print a line whenever the screen changes\n");
    fprintf(stderr, "  --per-frame       print 'pf <frame> <us> <instr>' per frame\n");
    fprintf(stderr, "  --watch OFFSET    print a WRAM 16-bit value whenever it changes\n");
    fprintf(stderr, "  --wram-dump N:F   dump the 128 KiB WRAM image at frame N to file F\n");
    fprintf(stderr, "  --no-render       skip software rendering (emulation-only timing)\n");
    fprintf(stderr, "  --ascii N         print a coarse ASCII rendition of frame N\n");
    fprintf(stderr, "  --zoom N:x0:y0:x1:y1  print a 1-char-per-pixel region of frame N\n");
    fprintf(stderr, "  frames defaults to 300\n");
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    int frames = 300;
    int positional_seen = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--input") && i + 1 < argc) {
            if (!parse_input_spec(argv[++i])) {
                fprintf(stderr, "Bad --input spec\n");
                return 1;
            }
        } else if (!strcmp(arg, "--shots") && i + 1 < argc) {
            g_shot_dir = argv[++i];
        } else if (!strcmp(arg, "--shot-every") && i + 1 < argc) {
            g_shot_every = atoi(argv[++i]);
        } else if (!strcmp(arg, "--dump-pc") && i + 1 < argc) {
            g_dump_pc_rows = (uint32_t)atoi(argv[++i]);
        } else if (!strcmp(arg, "--no-pc-profile")) {
            g_no_pc_profile = 1;
        } else if (!strcmp(arg, "--widescreen")) {
            g_widescreen = 1;
        } else if (!strcmp(arg, "--hash")) {
            g_print_hash = 1;
        } else if (!strcmp(arg, "--no-render")) {
            g_no_render = 1;
        } else if (!strcmp(arg, "--per-frame")) {
            g_per_frame = 1;
        } else if (!strcmp(arg, "--zoom") && i + 1 < argc) {
            int fr, x0, y0, x1, y1;
            if (sscanf(argv[++i], "%d:%d:%d:%d:%d",
                       &fr, &x0, &y0, &x1, &y1) != 5 ||
                g_zoom_count >= 4) {
                fprintf(stderr, "Bad --zoom spec: %s\n", argv[i]);
                return 1;
            }
            g_zoom_frame[g_zoom_count] = fr;
            g_zoom_rect[g_zoom_count][0] = x0;
            g_zoom_rect[g_zoom_count][1] = y0;
            g_zoom_rect[g_zoom_count][2] = x1;
            g_zoom_rect[g_zoom_count][3] = y1;
            g_zoom_count++;
        } else if (!strcmp(arg, "--ascii") && i + 1 < argc) {
            if (g_ascii_count >= 8) {
                fprintf(stderr, "Too many --ascii options\n");
                return 1;
            }
            g_ascii_frame[g_ascii_count++] = atoi(argv[++i]);
        } else if (!strcmp(arg, "--wram-dump") && i + 1 < argc) {
            int frame;
            char path[256];
            if (sscanf(argv[++i], "%d:%255s", &frame, path) != 2 ||
                g_wram_count >= 8) {
                fprintf(stderr, "Bad --wram-dump spec: %s\n", argv[i]);
                return 1;
            }
            g_wram_frame[g_wram_count] = frame;
            snprintf(g_wram_path[g_wram_count], sizeof(g_wram_path[0]),
                     "%s", path);
            g_wram_count++;
        } else if (!strcmp(arg, "--watch") && i + 1 < argc) {
            if (g_watch_count >= 4) {
                fprintf(stderr, "Too many --watch options\n");
                return 1;
            }
            g_watch_offset[g_watch_count++] =
                (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(arg, "--window") && i + 1 < argc) {
            if (sscanf(argv[++i], "%d:%d", &g_window_start, &g_window_end) != 2 ||
                g_window_start < 0 || g_window_end < g_window_start) {
                fprintf(stderr, "Bad --window spec: %s\n", argv[i]);
                return 1;
            }
        } else if (arg[0] == '-' && arg[1] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            print_usage(argv[0]);
            return 1;
        } else if (!positional_seen) {
            rom_path = arg;
            positional_seen = 1;
        } else {
            frames = atoi(arg);
            if (frames <= 0) {
                fprintf(stderr, "Invalid frame count: %s\n", arg);
                return 1;
            }
        }
    }

    if (!rom_path) {
        print_usage(argv[0]);
        return 1;
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
                SIMCITY_RECOMP_ROM_SIZE, rom_len);
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

    if (g_widescreen &&
        simcity_recomp_set_widescreen(recomp, 1, error, sizeof(error)) != 1) {
        fprintf(stderr, "widescreen: %s\n", error);
        simcity_recomp_destroy(recomp);
        return 1;
    }
    if (simcity_recomp_reset(recomp, error, sizeof(error)) != 1) {
        fprintf(stderr, "simcity_recomp_reset failed: %s\n", error);
        simcity_recomp_destroy(recomp);
        return 1;
    }

    sc_sim_profiler_init();
    if (g_no_pc_profile) g_sim_profiling_enabled = 0;
    fprintf(stderr, "Profiling %d frames, %d input transition(s)%s...\n",
            frames, g_transition_count, g_shot_dir ? " + screenshots" : "");

    int failed = 0;
    uint64_t block_instructions = 0;
    double block_seconds = 0.0;
    int block_frames = 0;
    int window_dumped = 0;

    for (int i = 0; i < frames; i++) {
        SimCityRecompFrameResult result;
        uint16_t input_mask = input_at(i);
        uint64_t instructions_before;
        double started;
        double frame_seconds;

        if (g_window_start >= 0 && i == g_window_start) {
sc_sim_profiler_init();
    if (g_no_pc_profile) g_sim_profiling_enabled = 0;
            fprintf(stderr, "  profile window opened at frame %d\n", i);
        }

        instructions_before = simcity_recomp_instruction_count(recomp);
        started = now_seconds();
        memset(&result, 0, sizeof(result));

        if (simcity_recomp_advance(recomp, input_mask, 1, &result) != 1) {
            fprintf(stderr, "Frame %d: %s\n", i + 1,
                    simcity_recomp_last_error(recomp));
            failed = 1;
            break;
        }

        advance_seconds += now_seconds() - started;
        if (result.frame_rendered && !g_no_render) {
            double render_start = now_seconds();
            if (simcity_recomp_render_current_frame(recomp, error,
                                                    sizeof(error)) != 1) {
                fprintf(stderr, "Frame %d render: %s\n", i + 1, error);
                failed = 1;
                break;
            }
            render_seconds += now_seconds() - render_start;
            print_screen_hash(recomp, i);
            for (int a = 0; a < g_ascii_count; a++)
                if (g_ascii_frame[a] == i) print_ascii(recomp, i);
            for (int z = 0; z < g_zoom_count; z++)
                if (g_zoom_frame[z] == i)
                    print_zoom(recomp, i, g_zoom_rect[z]);
        }

        block_seconds += (frame_seconds = now_seconds() - started);
        block_instructions +=
            simcity_recomp_instruction_count(recomp) - instructions_before;
        block_frames++;

        for (int w = 0; w < g_watch_count; w++) {
            uint8_t bytes[2] = {0, 0};
            uint16_t value;
            if (!simcity_recomp_read_wram(recomp, g_watch_offset[w], bytes, 2))
                continue;
            value = (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
            if (!g_watch_have[w] || value != g_watch_last[w]) {
                printf("wr %d 0x%04X=0x%04X\n", i, g_watch_offset[w], value);
                g_watch_last[w] = value;
                g_watch_have[w] = 1;
            }
        }

        if (g_per_frame)
            printf("pf %d %.0f %" PRIu64 "\n", i, frame_seconds * 1e6,
                   simcity_recomp_instruction_count(recomp) - instructions_before);

        if (g_shot_dir &&
            (is_transition_frame(i) || (g_shot_every > 0 && i % g_shot_every == 0))) {
            if (!take_shot(recomp, i)) {
                failed = 1;
                break;
            }
        }

        /* Discard any produced audio */
        simcity_recomp_audio_discard(recomp);

        for (int d = 0; d < g_wram_count; d++) {
            uint8_t wram[128 * 1024];
            FILE *wf;
            if (g_wram_frame[d] != i) continue;
            if (!simcity_recomp_read_wram(recomp, 0, wram, sizeof(wram))) {
                fprintf(stderr, "WRAM dump failed at frame %d\n", i);
                break;
            }
            wf = fopen(g_wram_path[d], "wb");
            if (wf) {
                fwrite(wram, 1, sizeof(wram), wf);
                fclose(wf);
                fprintf(stderr, "  WRAM dumped at frame %d -> %s\n", i,
                        g_wram_path[d]);
            }
        }

        if (g_window_end >= 0 && i == g_window_end) {
            fprintf(stderr, "  profile window closed at frame %d\n", i);
            printf("=== WINDOW %d:%d ===\n", g_window_start, g_window_end);
            sc_sim_profiler_dump();
            sc_sim_profiler_dump_pc(g_dump_pc_rows);
            window_dumped = 1;
        }

        if ((i + 1) % 50 == 0) {
            fprintf(stderr,
                    "  frame %d/%d  %.1f us/frame (adv %.1f + render %.1f)  %" PRIu64 " instr/frame  "
                    "input=0x%04X\n",
                    i + 1, frames,
                    block_frames ? block_seconds * 1e6 / block_frames : 0.0,
                    block_frames ? advance_seconds * 1e6 / block_frames : 0.0,
                    block_frames ? render_seconds * 1e6 / block_frames : 0.0,
                    block_frames ? block_instructions / (uint64_t)block_frames
                                 : 0ull,
                    input_mask);
            block_seconds = 0.0;
            advance_seconds = 0.0;
            render_seconds = 0.0;
            block_instructions = 0;
            block_frames = 0;
        }
    }

    if (!window_dumped) {
        double dump_started = now_seconds();
        sc_sim_profiler_dump();
        sc_sim_profiler_dump_pc(g_dump_pc_rows);
        fprintf(stderr, "  dump: %.1f ms\n",
                (now_seconds() - dump_started) * 1e3);
    }

    /* Top 10 town-route groups, kept for the native-reimplementation report. */
    uint32_t top_groups[10];
    double top_started = now_seconds();
    int n = sc_sim_profiler_get_top_groups(top_groups, 10);
    fprintf(stderr, "  top-groups select: %.1f ms\n",
            (now_seconds() - top_started) * 1e3);
    if (n > 0) {
        fprintf(stderr, "Top %d v35 town-route groups by total cycles:\n", n);
        for (int i = 0; i < n; i++) {
            uint32_t gid = top_groups[i];
            fprintf(stderr, "  [%d] Group 0x%04X: %" PRIu64 " calls, %" PRIu64 " cycles\n",
                    i, (unsigned int)gid, g_sim_group_profiles[gid].call_count,
                    g_sim_group_profiles[gid].total_cycles);
        }
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
