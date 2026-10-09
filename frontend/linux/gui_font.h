/*
 * gui_font.h — minimal 5x7 bitmap font and primitives for the Linux overlay.
 *
 * Hand-rolled so the frontend needs no font library and no TTF file: the menu
 * must work on a bare Steam Deck with nothing installed but SDL2. Glyphs are
 * 5 columns of 7 rows, bit 0 = top row.
 */

#ifndef SIMCITY_GUI_FONT_H
#define SIMCITY_GUI_FONT_H

#include <SDL.h>

#define SIMCITY_GLYPH_W 5
#define SIMCITY_GLYPH_H 7
#define SIMCITY_GLYPH_ADVANCE 6  /* 5 columns plus one of spacing */

typedef struct {
    SDL_Renderer *renderer;
    /* Integer pixel multiplier. The glyph grid is 5x7, which is unreadable on
       a 1280-wide window, so callers raise this instead of rewriting every
       coordinate in their own scale. 1 = unscaled. */
    int scale;
    SDL_Color foreground;
    SDL_Color background;
} SimCityGui;

void simcity_gui_init(SimCityGui *gui, SDL_Renderer *renderer);

/* Set the integer pixel multiplier. Values below 1 are clamped to 1. */
void simcity_gui_set_scale(SimCityGui *gui, int scale);

/* Draws text with the built-in font. Returns the x coordinate just past the
   last glyph. Characters outside the supported range are skipped. */
int simcity_gui_text(SimCityGui *gui, int x, int y, const char *text);

/* Draws text centred on cx. */
int simcity_gui_text_centered(SimCityGui *gui, int cx, int y, const char *text);

/* Width in renderer pixels at the given integer scale. */
int simcity_gui_text_width_scaled(const char *text, int scale);

void simcity_gui_fill_rect(SimCityGui *gui, int x, int y, int w, int h);
void simcity_gui_frame_rect(SimCityGui *gui, int x, int y, int w, int h);
void simcity_gui_set_colors(SimCityGui *gui, SDL_Color fg, SDL_Color bg);

#endif