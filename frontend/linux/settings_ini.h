/*
 * settings_ini.h — Linux frontend settings shared with the Windows launcher.
 *
 * The Windows launcher already owns settings.ini and writes INI sections with
 * the Windows profile API. This reader uses the same file, section and key
 * names so one settings.ini works on both platforms, rather than inventing a
 * second format.
 */

#ifndef SIMCITY_SETTINGS_INI_H
#define SIMCITY_SETTINGS_INI_H

/* Soft-mouse sensitivity bounds, shared by the CLI, the loader and the menu. */
#define SIMCITY_MOUSE_SENS_MIN 1
#define SIMCITY_MOUSE_SENS_MAX 64

typedef enum {
    SIMCITY_RENDERER_AUTO = 0,       /* try accelerated, fallback to software */
    SIMCITY_RENDERER_SOFTWARE,
    SIMCITY_RENDERER_OPENGL
} SimCityRenderer;

typedef struct {
    int width;
    int height;
    int widescreen;
    int soft_mouse;
    int mouse_sens;
    int freeze_money;
    int renderer;   /* SimCityRenderer */
} SimCityLinuxConfig;

void simcity_linux_config_defaults(SimCityLinuxConfig *config);

/* Reads settings.ini. Missing or malformed values fall back to defaults and
   leave the caller's struct valid. Returns 1 if the file existed. */
int simcity_settings_ini_load(const char *path, SimCityLinuxConfig *config);

/* Writes settings.ini. Returns 1 on success. */
int simcity_settings_ini_save(const char *path, const SimCityLinuxConfig *config);

#endif