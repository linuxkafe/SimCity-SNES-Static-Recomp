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

/* The Windows launcher writes Widescreen under [Display] while the Linux keys
   live in [General], so accept either rather than silently ignoring one. */
static int in_general(const char *section)
{
    return strcmp(section, "General") == 0;
}

static int in_display(const char *section)
{
    return strcmp(section, "Display") == 0;
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
        if (!in_general(section) && !in_display(section)) continue;

        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        trim(line);
        {
            char *value_text = eq + 1;
            int value;
            trim(value_text);
            value = (int)strtol(value_text, NULL, 10);

            if (!in_general(section)) continue;
            if (strcmp(line, "Width") == 0)
                config->width = clamp_int(value, SIMCITY_MIN_DIM, SIMCITY_MAX_DIM);
            else if (strcmp(line, "Height") == 0)
                config->height = clamp_int(value, SIMCITY_MIN_DIM, SIMCITY_MAX_DIM);
            else if (strcmp(line, "Widescreen") == 0 &&
                     (in_general(section) || in_display(section)))
                config->widescreen = value != 0;
            else if (strcmp(line, "SoftMouse") == 0 && in_general(section))
                config->soft_mouse = value != 0;
            else if (strcmp(line, "MouseSens") == 0)
                config->mouse_sens = clamp_int(value, SIMCITY_MOUSE_SENS_MIN,
                                            SIMCITY_MOUSE_SENS_MAX);
            else if (strcmp(line, "FreezeMoney") == 0)
                config->freeze_money = value != 0;
            else if (strcmp(line, "Renderer") == 0) {
                int v = value;
                if (v >= 0 && v <= 2) config->renderer = v;
            }
        }
    }
    fclose(file);

    if (config->width < SIMCITY_MIN_DIM) config->width = SIMCITY_MIN_DIM;
    if (config->height < SIMCITY_MIN_DIM) config->height = SIMCITY_MIN_DIM;
    return 1;
}

/* The Linux keys this frontend owns. Anything else in the file belongs to the
   Windows launcher and must survive a Linux run untouched. */
static const char kBanner[] = "; SimCity SNES Static Recomp settings.\n";

static const char *const owned_keys[] = {
    "Width", "Height", "Widescreen", "SoftMouse", "MouseSens", "FreezeMoney", "Renderer"
};
#define SIMCITY_INI_OWNED_COUNT (sizeof(owned_keys) / sizeof(owned_keys[0]))

static int is_owned_key(const char *name)
{
    size_t i;
    for (i = 0; i < SIMCITY_INI_OWNED_COUNT; ++i) {
        if (strcmp(name, owned_keys[i]) == 0) return 1;
    }
    return 0;
}

/* Inside [General] every owned key is ours to replace. Elsewhere only the keys
   the Windows launcher never uses may be removed, which heals files damaged by
   an earlier version that appended them at end of file. Widescreen must be
   spared outside [General]: the Windows launcher stores it under [Display]. */
static int key_may_be_replaced(const char *name, int inside_general)
{
    if (!is_owned_key(name)) return 0;
    if (inside_general) return 1;
    return strcmp(name, "Widescreen") != 0;
}

static void format_owned_value(const SimCityLinuxConfig *config,
                               const char *name, char *out, size_t cap)
{
    if (strcmp(name, "Width") == 0)          snprintf(out, cap, "%d", config->width);
    else if (strcmp(name, "Height") == 0)    snprintf(out, cap, "%d", config->height);
    else if (strcmp(name, "Widescreen") == 0) snprintf(out, cap, "%d", config->widescreen != 0);
    else if (strcmp(name, "SoftMouse") == 0)  snprintf(out, cap, "%d", config->soft_mouse != 0);
    else if (strcmp(name, "MouseSens") == 0)  snprintf(out, cap, "%d", config->mouse_sens);
    else                                      snprintf(out, cap, "%d", config->freeze_money != 0);
}

int simcity_settings_ini_save(const char *path, const SimCityLinuxConfig *config)
{
    FILE *file;
    FILE *out;
    char line[512];
    char tmp[600];
    int emitted = 0;
    int inside_general = 0;
    size_t i;

    if (!path || !*path || !config) return 0;
    if ((size_t)snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= sizeof(tmp))
        return 0;

    /* Rewrite through a temporary: a failure part-way must not truncate the
       settings the Windows launcher also depends on. */
    out = fopen(tmp, "w");
    if (!out) return 0;
    fprintf(out, "%s", kBanner);

    file = fopen(path, "r");
    if (file) {
        while (fgets(line, (int)sizeof(line), file)) {
            if (line[0] == '[') {
                char *close = strchr(line, ']');
                if (close) {
                    char name[64];
                    size_t len;
                    *close = '\0';
                    len = strlen(line + 1);
                    if (len >= sizeof(name)) len = sizeof(name) - 1u;
                    memcpy(name, line + 1, len);
                    name[len] = '\0';
                    *close = ']';
                    inside_general = (strcmp(name, "General") == 0);
                    if (inside_general && !emitted) {
                        /* Write our keys inside the section that declares
                           them, not at end of file where they would belong to
                           whichever section happened to come last. */
                        fputs(line, out);
                        for (i = 0; i < SIMCITY_INI_OWNED_COUNT; ++i) {
                            char value[32];
                            format_owned_value(config, owned_keys[i], value,
                                               sizeof(value));
                            fprintf(out, "%s=%s\n", owned_keys[i], value);
                        }
                        emitted = 1;
                        continue;
                    }
                }
                fputs(line, out);
                continue;
            }

            /* Drop our own banner so repeated runs do not stack copies. */
            if (strncmp(line, kBanner, sizeof(kBanner) - 1u) == 0) continue;

            /* Drop only the keys this frontend owns. Keys belonging to the
               Windows launcher must survive a Linux run untouched. */
            if (line[0] != ';' && line[0] != '#') {
                char *eq = strchr(line, '=');
                if (eq) {
                    char name[128];
                    size_t len = (size_t)(eq - line);
                    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
                        len--;
                    if (len >= sizeof(name)) len = sizeof(name) - 1u;
                    memcpy(name, line, len);
                    name[len] = '\0';
                    if (key_may_be_replaced(name, inside_general)) continue;
                }
            }
            fputs(line, out);
        }
        fclose(file);
    }

    if (!emitted) {
        fprintf(out, "\n[General]\n");
        for (i = 0; i < SIMCITY_INI_OWNED_COUNT; ++i) {
            char value[32];
            format_owned_value(config, owned_keys[i], value, sizeof(value));
            fprintf(out, "%s=%s\n", owned_keys[i], value);
        }
    }

    if (fclose(out) != 0) {
        (void)remove(tmp);
        return 0;
    }
    if (rename(tmp, path) != 0) {
        (void)remove(tmp);
        return 0;
    }
    return 1;
}
