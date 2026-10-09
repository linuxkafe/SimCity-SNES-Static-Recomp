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
    SIMCITY_MENU_ACTION_QUIT
} SimCityMenuAction;

typedef struct {
    SimCityGui gui;
    SimCityLinuxConfig *config;
    int open;
    int selected;
    int money_applied;   /* one-shot cheat fired this session */
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