/*
 * test_settings_ini.c — contract for the shared settings.ini reader/writer.
 *
 * The Windows launcher owns settings.ini too. These tests exist because an
 * earlier version of this file truncated it on save, which silently deleted
 * every Windows key on the first Linux run. The preservation cases below are
 * the regression guard for that.
 */

#include "settings_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL: %s\n", (msg));                             \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[512];
    int found = 0;
    if (!f) return 0;
    while (fgets(line, (int)sizeof(line), f)) {
        if (strstr(line, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static int count_occurrences(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[512];
    int n = 0;
    if (!f) return -1;
    while (fgets(line, (int)sizeof(line), f)) {
        if (strstr(line, needle)) n++;
    }
    fclose(f);
    return n;
}

int main(void)
{
    static const char kWindows[] =
        "; Windows launcher settings\n"
        "[General]\n"
        "IntegerScale=1\n"
        "FullScreenOnPlay=1\n"
        "WelcomeShown=1\n"
        "[Display]\n"
        "Widescreen=1\n"
        "[Input]\n"
        "Source=3\n"
        "Action0=7\n";
    const char *path = "/tmp/simcity-settings-test.ini";
    SimCityLinuxConfig config;
    SimCityLinuxConfig loaded;
    int i;

    /* Defaults apply when the file is absent. */
    CHECK(simcity_settings_ini_load("/tmp/simcity-no-such-file.ini", &config) == 0,
          "absent file reports absent");
    CHECK(config.width > 0 && config.height > 0, "defaults give a usable size");
    CHECK(config.mouse_sens >= SIMCITY_MOUSE_SENS_MIN &&
          config.mouse_sens <= SIMCITY_MOUSE_SENS_MAX, "default sens in range");
    /* renderer was left uninitialised by the defaults, so AUTO was whatever the
       stack held.  A default that is not written down is not a default. */
    CHECK(config.renderer == (SimCityRenderer)SIMCITY_RENDERER_AUTO,
          "default renderer is AUTO");

    /* Round trip of our own keys. */
    config.width = 1024; config.height = 768;
    config.widescreen = 0; config.soft_mouse = 1;
    config.mouse_sens = 7; config.freeze_money = 1;
    config.renderer = SIMCITY_RENDERER_OPENGL;
    CHECK(simcity_settings_ini_save(path, &config) == 1, "save succeeds");
    CHECK(simcity_settings_ini_load(path, &loaded) == 1, "load succeeds");
    CHECK(loaded.width == 1024 && loaded.height == 768, "size round trips");
    CHECK(loaded.widescreen == 0 && loaded.soft_mouse == 1, "flags round trip");
    CHECK(loaded.mouse_sens == 7, "sens round trips");
    CHECK(loaded.freeze_money == 1, "cheat flag round trips");
    CHECK(loaded.renderer == SIMCITY_RENDERER_OPENGL, "renderer round trips");

    /* Regression T022: format_owned_value() had no Renderer branch and fell
       through to the trailing `else`, writing freeze_money's value under the
       Renderer key.  Renderer=0 and freeze_money=1 therefore saved as
       "Renderer=1" and silently forced the software renderer on every load.
       Each value must be written and read back independently. */
    {
        SimCityLinuxConfig r;
        int expect;
        for (expect = SIMCITY_RENDERER_AUTO; expect <= SIMCITY_RENDERER_OPENGL; ++expect) {
            simcity_linux_config_defaults(&r);
            r.renderer = (SimCityRenderer)expect;
            r.freeze_money = (expect % 2);   /* differ from renderer every time */
            CHECK(simcity_settings_ini_save(path, &r) == 1, "renderer save succeeds");
            CHECK(simcity_settings_ini_load(path, &loaded) == 1, "renderer load succeeds");
            CHECK((int)loaded.renderer == expect,
                  "Renderer key carries the renderer, not FreezeMoney");
            CHECK(loaded.freeze_money == (expect % 2),
                  "FreezeMoney key is not overwritten by Renderer");
        }
    }

    /* An out-of-range Renderer must not be adopted. */
    write_file(path, "[General]\nRenderer=99\n");
    CHECK(simcity_settings_ini_load(path, &loaded) == 1, "bad renderer loads");
    CHECK(loaded.renderer == (SimCityRenderer)SIMCITY_RENDERER_AUTO, "out-of-range renderer ignored");
    write_file(path, "[General]\nRenderer=-1\n");
    CHECK(simcity_settings_ini_load(path, &loaded) == 1, "negative renderer loads");
    CHECK(loaded.renderer == (SimCityRenderer)SIMCITY_RENDERER_AUTO, "negative renderer ignored");

    /* Out-of-range values are clamped rather than trusted. */
    write_file(path,
        "[General]\nWidth=99999999\nHeight=-5\nMouseSens=9999\n");
    CHECK(simcity_settings_ini_load(path, &loaded) == 1, "bad file still loads");
    CHECK(loaded.width <= 7680, "oversize width clamped");
    CHECK(loaded.height >= 160, "negative height clamped");
    CHECK(loaded.mouse_sens <= SIMCITY_MOUSE_SENS_MAX, "oversize sens clamped");
    CHECK(loaded.mouse_sens >= SIMCITY_MOUSE_SENS_MIN, "sens lower bound");

    /* Junk must not crash and must leave a usable config. */
    {
        SimCityLinuxConfig fallback;
        simcity_linux_config_defaults(&fallback);
        write_file(path,
            "not an ini file\n[General]\nWidth=abc\n[Other]\nHeight=640\n\n=oops\n");
        CHECK(simcity_settings_ini_load(path, &loaded) == 1, "junk file loads");
        CHECK(loaded.width >= 160, "junk leaves a valid width");
        /* Height lives under [Other], which is not a section this frontend
           owns, so it must fall back to the default rather than be adopted. */
        CHECK(loaded.height == fallback.height,
              "keys outside [General] are ignored, not adopted");
    }

    /* Widescreen is accepted from [General] and from [Display]. */
    write_file(path, "[General]\nSoftMouse=1\n[Display]\nWidescreen=1\n");
    CHECK(simcity_settings_ini_load(path, &loaded) == 1, "Display section loads");
    CHECK(loaded.widescreen == 1, "Widescreen read from [Display]");
    CHECK(loaded.soft_mouse == 1, "General keys read alongside");

    /* The regression that mattered: saving must not delete Windows settings. */
    write_file(path, kWindows);
    CHECK(simcity_settings_ini_load(path, &config) == 1, "windows file loads");
    config.width = 1280; config.height = 720;
    CHECK(simcity_settings_ini_save(path, &config) == 1, "save over windows file");
    CHECK(file_contains(path, "IntegerScale=1"), "IntegerScale preserved");
    CHECK(file_contains(path, "FullScreenOnPlay=1"), "FullScreenOnPlay preserved");
    CHECK(file_contains(path, "WelcomeShown=1"), "WelcomeShown preserved");
    CHECK(file_contains(path, "Source=3"), "[Input] Source preserved");
    CHECK(file_contains(path, "Action0=7"), "[Input] Action0 preserved");

    /* [Display] Widescreen belongs to the Windows launcher and must survive. */
    {
        int n = 0;
        FILE *f = fopen(path, "r");
        char line[512];
        CHECK(f != NULL, "saved file readable");
        if (f) {
            int in_display = 0;
            while (fgets(line, (int)sizeof(line), f)) {
                if (line[0] == '[') {
                    in_display = (strncmp(line, "[Display]", 9) == 0);
                    continue;
                }
                if (in_display && strncmp(line, "Widescreen=", 10) == 0) { n++; break; }
            }
            fclose(f);
        }
        CHECK(n == 1, "Widescreen under [Display] preserved");
    }

    /* Repeated saves are idempotent: no duplicated keys, no stacked banner. */
    for (i = 0; i < 3; ++i) {
        CHECK(simcity_settings_ini_load(path, &config) == 1, "reload for idempotence");
        CHECK(simcity_settings_ini_save(path, &config) == 1, "resave for idempotence");
    }
    CHECK(count_occurrences(path, "Width=") == 1, "Width written exactly once");
    CHECK(count_occurrences(path, "Height=") == 1, "Height written exactly once");
    CHECK(count_occurrences(path, "SoftMouse=") == 1, "SoftMouse written once");
    CHECK(count_occurrences(path, "Renderer=") == 1, "Renderer written once");
    CHECK(count_occurrences(path, "FreezeMoney=") == 1, "FreezeMoney written once");
    CHECK(count_occurrences(path, "SimCity SNES Static Recomp settings.") == 1,
          "banner not stacked");
    CHECK(count_occurrences(path, "IntegerScale=1") == 1,
          "Windows key not duplicated");

    (void)remove(path);

    if (failures) {
        fprintf(stderr, "\n%d settings_ini failure(s)\n", failures);
        return 1;
    }
    printf("OK: settings_ini preserves foreign keys, clamps, and is idempotent\n");
    return 0;
}