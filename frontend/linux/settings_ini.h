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

/* Channel order used when handing the frame to the renderer.
 *
 * The numeric values are written to settings.ini, so they must keep their
 * meaning: 0 stays NORMAL and 1 stays SWAP_RB even though the default changed.
 *
 * SWAP_RB is the default because it is what the Deck actually needs: with it
 * the controller logo and the in-game R/C/I read correctly on the machine this
 * was verified on.  The frontend uploads 0xAARRGGBB through
 * SDL_PIXELFORMAT_ARGB8888, which reproduces the core byte for byte in every
 * automated check, so NORMAL is the nominal answer -- but on the reported
 * hardware the presented picture is the one SWAP_RB produces, and that was
 * confirmed by eye.  Leave the menu on NORMAL to go back to the raw order. */
typedef enum {
    SIMCITY_COLOR_NORMAL = 0,
    SIMCITY_COLOR_SWAP_RB
} SimCityColorMode;

typedef struct {
    int width;
    int height;
    int widescreen;
    int soft_mouse;
    int mouse_sens;
    int freeze_money;
    int renderer;   /* SimCityRenderer */
    int color_mode; /* SimCityColorMode */
} SimCityLinuxConfig;

void simcity_linux_config_defaults(SimCityLinuxConfig *config);

/* Reads settings.ini. Missing or malformed values fall back to defaults and
   leave the caller's struct valid. Returns 1 if the file existed. */
int simcity_settings_ini_load(const char *path, SimCityLinuxConfig *config);

/* Writes settings.ini. Returns 1 on success. */
int simcity_settings_ini_save(const char *path, const SimCityLinuxConfig *config);

#endif