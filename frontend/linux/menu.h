/*
 * menu.h — in-window settings overlay for the Linux frontend.
 *
 * Drawn on top of the game frame rather than in a separate window, so the menu
 * needs no second renderer, no font library and no TTF file: it works on a bare
 * Steam Deck with nothing installed but SDL2. Navigated with the d-pad, arrow
 * keys or a gamepad, which is what the Deck actually has.
 */

#ifndef SIMCITY_MENU_H
#define SIMCITY_MENU_H

#include "gui_font.h"
#include "settings_ini.h"

typedef enum {
    SIMCITY_MENU_ACTION_NONE = 0,
    SIMCITY_MENU_ACTION_RESUME,
    SIMCITY_MENU_ACTION_APPLY_MONEY,
    SIMCITY_MENU_ACTION_APPLY_START_MONEY,
    SIMCITY_MENU_ACTION_TOGGLE_MONEY_NEVER_DECREASE,
    SIMCITY_MENU_ACTION_TOGGLE_EARN_PER_BUILD,
    SIMCITY_MENU_ACTION_TOGGLE_TIME_FASTER,
    SIMCITY_MENU_ACTION_TOGGLE_TIME_SLOWER,
    SIMCITY_MENU_ACTION_TOGGLE_ALL_POWER,
    SIMCITY_MENU_ACTION_TOGGLE_DEMOLISH_ANYTHING,
    SIMCITY_MENU_ACTION_QUIT
} SimCityMenuAction;

/* Navigation timing. Holding a direction used to move one row per frame, so at
   60 Hz a held d-pad ran through the whole list before the player could react.
   The first step is immediate, then the key repeats. */
#define SIMCITY_MENU_REPEAT_DELAY_MS 400
#define SIMCITY_MENU_REPEAT_RATE_MS 90

/* Cheats got their own submenu: freezing money is not a display setting, and
   burying it one line below mouse sensitivity made it easy to toggle by
   accident while adjusting the pointer. */
typedef enum {
    SIMCITY_CHEATS_BACK = 0,
    SIMCITY_CHEATS_MONEY,
    SIMCITY_CHEATS_FREEZE_MONEY,
    SIMCITY_CHEATS_START_3000,
    SIMCITY_CHEATS_START_40000,
    SIMCITY_CHEATS_START_60000,
    SIMCITY_CHEATS_START_65535,
    SIMCITY_CHEATS_MONEY_NEVER_DECREASE,
    SIMCITY_CHEATS_EARN_PER_BUILD,
    SIMCITY_CHEATS_TIME_FASTER,
    SIMCITY_CHEATS_TIME_SLOWER,
    SIMCITY_CHEATS_ALL_POWER,
    SIMCITY_CHEATS_DEMOLISH_ANYTHING,
    SIMCITY_CHEATS_COUNT
} SimCityCheatRow;

typedef struct {
    SimCityGui gui;
    SimCityLinuxConfig *config;
    int open;
    int selected;
    int money_applied;   /* one-shot cheat fired this session */
    int start_money_value; /* value for start money cheats */

    /* Cheats submenu. */
    int in_cheats;
    int cheat_selected;

    /* Edge detection for navigation. prev_input is last frame's mask; the
       per-axis timestamps track when each held direction started (index 0 = up,
       1 = down) and when it last stepped. Kept separate per direction so one
       held axis cannot reset the other's repeat timer. */
    uint16_t prev_input;
    Uint32 axis_hold_ms[2];
    Uint32 axis_last_ms[2];
} SimCityMenu;

/* Resolution presets cycled by the RESOLUTION row. Index order matches
   --resolution on the command line. */
extern const int SIMCITY_MENU_RES_COUNT;
void simcity_menu_res_size(int index, int *w, int *h);
const char *simcity_menu_res_name(int index);

void simcity_menu_init(SimCityMenu *menu, SimCityLinuxConfig *config,
                       SDL_Renderer *renderer);

void simcity_menu_set_open(SimCityMenu *menu, int open);
int  simcity_menu_is_open(const SimCityMenu *menu);

/* Applies input to the menu. Returns an action for the caller to perform.
   input_mask is the guest controller mask so the d-pad works from the pad. */
SimCityMenuAction simcity_menu_handle_input(SimCityMenu *menu, uint16_t input_mask);

void simcity_menu_draw(SimCityMenu *menu, int window_w, int window_h);

#endif