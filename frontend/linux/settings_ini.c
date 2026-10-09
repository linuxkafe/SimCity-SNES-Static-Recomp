/*
 * settings_ini.c — minimal INI reader/writer for the Linux frontend.
 *
 * Deliberately tiny: the Windows launcher writes settings.ini through the
 * profile API, and only a handful of scalar keys are shared. Parsing the same
 * file keeps one settings.ini working across both frontends.
 */

#include "settings_ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIMCITY_MIN_DIM 160
#define SIMCITY_MAX_DIM 7680

void simcity_linux_config_defaults(SimCityLinuxConfig *config)
{
    if (!config) return;
    config->width = 1280;
    config->height = 800;
    config->widescreen = 1;
    config->soft_mouse = 0;
    config->mouse_sens = 2;
    config->freeze_money = 0;
}

static void trim(char *s)
{
    size_t n;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1u);
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static int clamp_int(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int in_general(const char *section)
{
    return strcmp(section, "General") == 0;
}

int simcity_settings_ini_load(const char *path, SimCityLinuxConfig *config)
{
    FILE *file;
    char line[256];
    char section[64] = "";

    if (!config) return 0;
    simcity_linux_config_defaults(config);
    if (!path || !*path) return 0;

    file = fopen(path, "r");
    if (!file) return 0;

    while (fgets(line, (int)sizeof(line), file)) {
        char *eq;
        trim(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            char *close = strchr(line, ']');
            size_t name_len;
            if (!close) continue;
            *close = '\0';
            name_len = strlen(line + 1);
            if (name_len >= sizeof(section)) name_len = sizeof(section) - 1u;
            memcpy(section, line + 1, name_len);
            section[name_len] = '\0';
            continue;
        }
        if (!in_general(section)) continue;

        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        trim(line);
        {
            char *value_text = eq + 1;
            int value;
            trim(value_text);
            value = (int)strtol(value_text, NULL, 10);

            if (strcmp(line, "Width") == 0)
                config->width = clamp_int(value, SIMCITY_MIN_DIM, SIMCITY_MAX_DIM);
            else if (strcmp(line, "Height") == 0)
                config->height = clamp_int(value, SIMCITY_MIN_DIM, SIMCITY_MAX_DIM);
            else if (strcmp(line, "Widescreen") == 0)
                config->widescreen = value != 0;
            else if (strcmp(line, "SoftMouse") == 0)
                config->soft_mouse = value != 0;
            else if (strcmp(line, "MouseSens") == 0)
                config->mouse_sens = clamp_int(value, 1, 64);
            else if (strcmp(line, "FreezeMoney") == 0)
                config->freeze_money = value != 0;
        }
    }
    fclose(file);

    if (config->width < SIMCITY_MIN_DIM) config->width = SIMCITY_MIN_DIM;
    if (config->height < SIMCITY_MIN_DIM) config->height = SIMCITY_MIN_DIM;
    return 1;
}

int simcity_settings_ini_save(const char *path, const SimCityLinuxConfig *config)
{
    FILE *file;
    if (!path || !*path || !config) return 0;
    file = fopen(path, "w");
    if (!file) return 0;
    fprintf(file,
            "; SimCity SNES Static Recomp settings\n"
            "; Shared by the Linux frontend and the Windows launcher.\n"
            "[General]\n"
            "Width=%d\n"
            "Height=%d\n"
            "Widescreen=%d\n"
            "SoftMouse=%d\n"
            "MouseSens=%d\n"
            "FreezeMoney=%d\n",
            config->width, config->height, config->widescreen != 0,
            config->soft_mouse != 0, config->mouse_sens,
            config->freeze_money != 0);
    return fclose(file) == 0;
}