/*
 * menu.c — settings overlay.
 *
 * Only the resolution presets, widescreen, pointer cursor, pointer sensitivity
 * and the money cheat are offered. Cheat addresses are limited to WRAM
 * locations verified against this core; an unverified address would silently do
 * nothing, which is worse than not offering it.
 */

#include "menu.h"
#include "simcity_static_recomp.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    ROW_RESOLUTION = 0,
    ROW_WIDESCREEN,
    ROW_SOFT_MOUSE,
    ROW_MOUSE_SENS,
    ROW_COLOR,
    ROW_CHEATS,
    ROW_RESUME,
    ROW_QUIT,
    ROW_COUNT
} RowId;

/* Must match the --resolution order in main.c. */
static const struct {
    const char *name;
    int width;
    int height;
} kResolutions[] = {
    { "480P",  512,  480 },
    { "720P",  1280, 720 },
    { "800P",  1280, 800 },
    { "1080P", 1920, 1080 },
};

const int SIMCITY_MENU_RES_COUNT = (int)(sizeof(kResolutions) / sizeof(kResolutions[0]));

void simcity_menu_res_size(int index, int *w, int *h)
{
    int i = index;
    if (i < 0) i = 0;
    if (i >= SIMCITY_MENU_RES_COUNT) i = SIMCITY_MENU_RES_COUNT - 1;
    if (w) *w = kResolutions[i].width;
    if (h) *h = kResolutions[i].height;
}

const char *simcity_menu_res_name(int index)
{
    int i = index;
    if (i < 0) i = 0;
    if (i >= SIMCITY_MENU_RES_COUNT) i = SIMCITY_MENU_RES_COUNT - 1;
    return kResolutions[i].name;
}

static int res_index_for(const SimCityLinuxConfig *config)
{
    int i;
    for (i = 0; i < SIMCITY_MENU_RES_COUNT; ++i) {
        if (kResolutions[i].width == config->width &&
            kResolutions[i].height == config->height)
            return i;
    }
    return -1;
}

void simcity_menu_init(SimCityMenu *menu, SimCityLinuxConfig *config,
                       SDL_Renderer *renderer)
{
    memset(menu, 0, sizeof(*menu));
    menu->config = config;
    simcity_gui_init(&menu->gui, renderer);
}

void simcity_menu_set_open(SimCityMenu *menu, int open)
{
    menu->open = open != 0;
    if (menu->open) menu->selected = ROW_RESUME;
}

int simcity_menu_is_open(const SimCityMenu *menu)
{
    return menu->open;
}

static void adjust_row(SimCityMenu *menu, int row, int direction)
{
    SimCityLinuxConfig *c = menu->config;
    switch (row) {
        case ROW_RESOLUTION: {
            int index = res_index_for(c);
            if (index < 0) index = 2;  /* custom starts from the 800p preset */
            index += direction;
            if (index < 0) index = 0;
            if (index >= SIMCITY_MENU_RES_COUNT) index = SIMCITY_MENU_RES_COUNT - 1;
            c->width = kResolutions[index].width;
            c->height = kResolutions[index].height;
            break;
        }
        case ROW_WIDESCREEN:
            c->widescreen = !c->widescreen;
            break;
        case ROW_SOFT_MOUSE:
            c->soft_mouse = !c->soft_mouse;
            break;
        case ROW_MOUSE_SENS:
            c->mouse_sens += direction;
            if (c->mouse_sens < SIMCITY_MOUSE_SENS_MIN)
                c->mouse_sens = SIMCITY_MOUSE_SENS_MIN;
            if (c->mouse_sens > SIMCITY_MOUSE_SENS_MAX)
                c->mouse_sens = SIMCITY_MOUSE_SENS_MAX;
            break;
        case ROW_COLOR:
            if (direction != 0) {
                int next = c->color_mode + (direction > 0 ? 1 : -1);
                if (next < SIMCITY_COLOR_NORMAL) next = SIMCITY_COLOR_SWAP_RB;
                if (next > SIMCITY_COLOR_SWAP_RB) next = SIMCITY_COLOR_NORMAL;
                c->color_mode = (SimCityColorMode)next;
            }
            break;
        case ROW_CHEATS:
            break;   /* enters the submenu on B, not on left/right */
        default:
            break;
    }
}

/* True once, on the frame a direction becomes held. */
static int nav_pressed(SimCityMenu *menu, uint16_t mask, uint16_t bit)
{
    return (mask & bit) != 0 && (menu->prev_input & bit) == 0;
}

/* True when a held direction should step again: not until the initial delay
   has passed, then at a fixed rate. Without this the selection advanced one row
   per frame while the key was down. */
static int nav_repeats(SimCityMenu *menu, uint16_t mask, uint16_t bit)
{
    Uint32 now, elapsed;

    if ((mask & bit) == 0) {
        menu->hold_started_ms = 0;
        return 0;
    }
    now = SDL_GetTicks();
    if (menu->hold_started_ms == 0) {
        menu->hold_started_ms = now;
        return 0;               /* this is the initial press */
    }
    elapsed = now - menu->hold_started_ms;
    if (elapsed < SIMCITY_MENU_REPEAT_DELAY_MS) return 0;
    if (now - menu->last_step_ms < SIMCITY_MENU_REPEAT_RATE_MS) return 0;
    return 1;
}

static void cheats_adjust(SimCityMenu *menu, int direction)
{
    SimCityLinuxConfig *c = menu->config;
    (void)direction;
    switch ((SimCityCheatRow)menu->cheat_selected) {
        case SIMCITY_CHEATS_FREEZE_MONEY:
            c->freeze_money = !c->freeze_money;
            break;
        default:
            break;
    }
}

static SimCityMenuAction cheats_input(SimCityMenu *menu, uint16_t input_mask)
{
    int now_step = nav_pressed(menu, input_mask, SIMCITY_INPUT_UP) ||
                   nav_repeats(menu, input_mask, SIMCITY_INPUT_UP) ||
                   nav_pressed(menu, input_mask, SIMCITY_INPUT_DOWN) ||
                   nav_repeats(menu, input_mask, SIMCITY_INPUT_DOWN);
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_UP) ||
        nav_repeats(menu, input_mask, SIMCITY_INPUT_UP)) {
        menu->cheat_selected =
            (menu->cheat_selected + SIMCITY_CHEATS_COUNT - 1) % SIMCITY_CHEATS_COUNT;
        menu->last_step_ms = SDL_GetTicks();
    }
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_DOWN) ||
        nav_repeats(menu, input_mask, SIMCITY_INPUT_DOWN)) {
        menu->cheat_selected =
            (menu->cheat_selected + 1) % SIMCITY_CHEATS_COUNT;
        menu->last_step_ms = SDL_GetTicks();
    }
    (void)now_step;

    if (input_mask & SIMCITY_INPUT_B) {
        switch ((SimCityCheatRow)menu->cheat_selected) {
            case SIMCITY_CHEATS_MONEY:
                menu->money_applied = 1;
                return SIMCITY_MENU_ACTION_APPLY_MONEY;
            case SIMCITY_CHEATS_FREEZE_MONEY:
                cheats_adjust(menu, 1);
                break;
            case SIMCITY_CHEATS_BACK:
            default:
                menu->in_cheats = 0;
                break;
        }
    }
    if (input_mask & SIMCITY_INPUT_A) {
        menu->in_cheats = 0;
    }
    return SIMCITY_MENU_ACTION_NONE;
}

SimCityMenuAction simcity_menu_handle_input(SimCityMenu *menu, uint16_t input_mask)
{
    int row;

    if (!menu->open) {
        menu->prev_input = input_mask;
        return SIMCITY_MENU_ACTION_NONE;
    }
    if (menu->in_cheats) {
        SimCityMenuAction a = cheats_input(menu, input_mask);
        menu->prev_input = input_mask;
        return a;
    }

    row = menu->selected;

    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_UP) ||
        nav_repeats(menu, input_mask, SIMCITY_INPUT_UP)) {
        menu->selected = (row > 0) ? row - 1 : ROW_COUNT - 1;
        menu->last_step_ms = SDL_GetTicks();
    }
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_DOWN) ||
        nav_repeats(menu, input_mask, SIMCITY_INPUT_DOWN)) {
        menu->selected = (row + 1) % ROW_COUNT;
        menu->last_step_ms = SDL_GetTicks();
    }

    /* Adjustments are edge-triggered: holding right must not run the value to
       its limit in a single frame. */
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_LEFT))
        adjust_row(menu, menu->selected, -1);
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_RIGHT))
        adjust_row(menu, menu->selected, +1);

    /* B is confirm, matching the guest, where B confirms and A cancels. */
    if (nav_pressed(menu, input_mask, SIMCITY_INPUT_B)) {
        switch ((RowId)menu->selected) {
            case ROW_CHEATS:
                menu->in_cheats = 1;
                menu->cheat_selected = SIMCITY_CHEATS_BACK;
                break;
            case ROW_RESUME:
                menu->open = 0;
                menu->prev_input = 0;
                return SIMCITY_MENU_ACTION_RESUME;
            case ROW_QUIT:
                menu->prev_input = 0;
                return SIMCITY_MENU_ACTION_QUIT;
            default:
                break;   /* toggles act immediately on left/right */
        }
    }

    menu->prev_input = input_mask;
    return SIMCITY_MENU_ACTION_NONE;
}

static void value_text(const SimCityMenu *menu, int row, char *out, size_t cap)
{
    const SimCityLinuxConfig *c = menu->config;
    switch (row) {
        case ROW_RESOLUTION: {
            int index = res_index_for(c);
            if (index >= 0)
                snprintf(out, cap, "%s", kResolutions[index].name);
            else
                snprintf(out, cap, "%dX%d", c->width, c->height);
            break;
        }
        case ROW_WIDESCREEN:
            snprintf(out, cap, "%s", c->widescreen ? "ON" : "OFF");
            break;
        case ROW_SOFT_MOUSE:
            snprintf(out, cap, "%s", c->soft_mouse ? "ON" : "OFF");
            break;
        case ROW_MOUSE_SENS:
            snprintf(out, cap, "%d", c->mouse_sens);
            break;
        case ROW_COLOR:
            snprintf(out, cap, "%s", c->color_mode == SIMCITY_COLOR_SWAP_RB
                                       ? "SWAP R/B" : "NORMAL");
            break;
        case ROW_CHEATS:
            snprintf(out, cap, "%s", "ENTER");
            break;
        default:
            out[0] = '\0';
            break;
    }
}

static void draw_cheats(SimCityMenu *menu, int window_w, int window_h)
{
    static const char *const kCheatLabels[SIMCITY_CHEATS_COUNT] = {
        "BACK", "ADD MONEY", "FREEZE MONEY"
    };
    static const SDL_Color kPanel = { 16, 16, 16, 236 };
    static const SDL_Color kText  = { 250, 250, 250, 255 };
    static const SDL_Color kValue = { 120, 200, 255, 255 };
    static const SDL_Color kSelBg = { 40, 40, 40, 255 };

    const int panel_h_base = 40 + SIMCITY_CHEATS_COUNT * 22 + 34;
    int panel_w, panel_h, x0, y0, y, i, sc = 1;
    char value[32];

    sc = window_h / 200;
    if (sc < 1) sc = 1;
    if (sc > 4) sc = 4;
    while (sc > 1 && (380 * sc > window_w - 20 ||
                      panel_h_base * sc > window_h - 20)) {
        sc--;
    }

    panel_w = 380 * sc;
    panel_h = panel_h_base * sc;
    x0 = (window_w - panel_w) / 2;
    y0 = (window_h - panel_h) / 2;

    simcity_gui_set_scale(&menu->gui, sc);
    simcity_gui_set_colors(&menu->gui, kText, kPanel);
    simcity_gui_fill_rect(&menu->gui, x0, y0, panel_w, panel_h);
    simcity_gui_frame_rect(&menu->gui, x0, y0, panel_w, panel_h);

    y = y0 + 14 * sc;
    simcity_gui_text_centered(&menu->gui, window_w / 2, y, "CHEATS");
    y += 22 * sc;

    for (i = 0; i < SIMCITY_CHEATS_COUNT; ++i) {
        int row_x = x0 + 14 * sc;
        int row_w = panel_w - 28 * sc;
        if (i == menu->cheat_selected) {
            simcity_gui_set_colors(&menu->gui, kValue, kSelBg);
            simcity_gui_fill_rect(&menu->gui, x0 + 8 * sc, y - 3 * sc,
                                  panel_w - 16 * sc, 15);
        } else {
            simcity_gui_set_colors(&menu->gui, kText, kPanel);
        }
        simcity_gui_text(&menu->gui, row_x, y, kCheatLabels[i]);

        value[0] = '\0';
        switch ((SimCityCheatRow)i) {
            case SIMCITY_CHEATS_MONEY:
                snprintf(value, sizeof(value), "%s",
                         menu->money_applied ? "DONE" : "PRESS B");
                break;
            case SIMCITY_CHEATS_FREEZE_MONEY:
                snprintf(value, sizeof(value), "%s",
                         menu->config->freeze_money ? "ON" : "OFF");
                break;
            default:
                break;
        }
        if (value[0]) {
            simcity_gui_text(&menu->gui,
                             row_x + row_w - simcity_gui_text_width_scaled(value, sc),
                             y, value);
        }
        y += 18 * sc;
    }
}

void simcity_menu_draw(SimCityMenu *menu, int window_w, int window_h)
{
    static const char *const kLabels[ROW_COUNT] = {
        "RESOLUTION", "WIDESCREEN", "SOFT MOUSE", "MOUSE SENS", "COLOR",
        "CHEATS", "RESUME", "QUIT"
    };
    static const SDL_Color kPanel = { 16, 16, 16, 236 };
    static const SDL_Color kText  = { 250, 250, 250, 255 };
    static const SDL_Color kValue = { 120, 200, 255, 255 };
    static const SDL_Color kSelBg = { 40, 40, 40, 255 };

    const int panel_h_base = 40 + ROW_COUNT * 22 + 34;
    int panel_w, panel_h, x0, y0, y, i, menu_scale = 1;
    char value[32];

    if (!menu->open) return;
    if (menu->in_cheats) { draw_cheats(menu, window_w, window_h); return; }

    /* The glyph grid is 5x7 pixels, so the panel is built at 1:1 and then
       scaled by an integer factor derived from the window. Without this the
       menu is a few hundred pixels of unreadable text in the middle of a
       1280x800 window. Factor 1 on a 640-wide window, up to 4x when there is
       room. */
    {
        int scale = window_h / 200;
        if (scale < 1) scale = 1;
        if (scale > 4) scale = 4;
        /* Never wider or taller than the window even after scaling. */
        while (scale > 1 &&
               (380 * scale > window_w - 20 || panel_h_base * scale > window_h - 20)) {
            scale--;
        }
        menu_scale = scale;
    }

    panel_w = 380 * menu_scale;
    panel_h = panel_h_base * menu_scale;
    x0 = (window_w - panel_w) / 2;
    y0 = (window_h - panel_h) / 2;

    simcity_gui_set_scale(&menu->gui, menu_scale);
    simcity_gui_set_colors(&menu->gui, kText, kPanel);
    simcity_gui_fill_rect(&menu->gui, x0, y0, panel_w, panel_h);
    simcity_gui_frame_rect(&menu->gui, x0, y0, panel_w, panel_h);

    y = y0 + 14 * menu_scale;
    simcity_gui_text_centered(&menu->gui, window_w / 2, y, "SETTINGS");
    y += 22 * menu_scale;

    for (i = 0; i < ROW_COUNT; ++i) {
        int row_x = x0 + 14 * menu_scale;
        int row_w = panel_w - 28 * menu_scale;
        if (i == menu->selected) {
            simcity_gui_set_colors(&menu->gui, kValue, kSelBg);
            simcity_gui_fill_rect(&menu->gui, x0 + 8 * menu_scale,
                                  y - 3 * menu_scale,
                                  panel_w - 16 * menu_scale, 15);
        } else {
            simcity_gui_set_colors(&menu->gui, kText, kPanel);
        }
        simcity_gui_text(&menu->gui, row_x, y, kLabels[i]);
        value_text(menu, i, value, sizeof(value));
        simcity_gui_text(&menu->gui,
                         row_x + row_w - simcity_gui_text_width_scaled(value,
                                                                        menu_scale),
                         y, value);
        y += 18 * menu_scale;
    }

    simcity_gui_set_colors(&menu->gui, kValue, kPanel);
    simcity_gui_text_centered(&menu->gui, window_w / 2, y0 + panel_h - 16,
                             "ARROWS CHANGE   B SELECT   F1 MENU");
}