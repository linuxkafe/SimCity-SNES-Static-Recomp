#ifndef SC_V28_VIDEO_H
#define SC_V28_VIDEO_H

#include "sc_v11_runtime.h"
#include "sc_v11_video.h"
#include "simcity_static_recomp.h"
#include <stddef.h>
#include <stdint.h>

#define SC_V28_FRAME_WIDTH SC_V11_VIDEO_WIDTH
#define SC_V28_MAX_FRAME_WIDTH SIMCITY_RECOMP_WIDESCREEN_WIDTH
#define SC_V28_FRAME_HEIGHT SC_V11_VIDEO_HEIGHT
#define SC_V28_FRAME_PIXELS SC_V11_VIDEO_PIXELS
#define SC_V28_MAX_FRAME_PIXELS \
    (SC_V28_MAX_FRAME_WIDTH * SC_V28_FRAME_HEIGHT)

typedef struct SCV28VideoReport {
    uint8_t passed;
    uint8_t mode;
    uint8_t brightness;
    uint8_t forced_blank;
    uint8_t main_screen_mask;
    uint8_t sub_screen_mask;
    uint8_t color_math;
    uint8_t reached_feature_set_supported;
    uint32_t nonblack_pixels;
    uint32_t unique_colors;
    char bgr555_sha256[65];
    char bgra_sha256[65];
    char error[192];
} SCV28VideoReport;

/* hud_shift_x is a user-controlled X offset in pixels applied to HUD chrome in
   the widened city view. Chrome is everything that is not the map cursor,
   which the renderer already identifies via the guest's map-focus flag; no OAM
   index threshold is used, because one tears multi-entry chrome sprites that
   straddle it. It is ignored when left_margin is 0, so the native 4:3 frame
   never moves. */

int sc_v28_render_first_visible_frame(const SCV11Runtime *runtime,
                                      unsigned output_width,
                                      unsigned left_margin,
                                      int hud_shift_x,
                                      uint16_t *bgr555,
                                      size_t bgr555_capacity,
                                      uint32_t *bgra,
                                      size_t bgra_capacity,
                                      SCV28VideoReport *report);
int sc_v28_render_first_visible_frame_columns(const SCV11Runtime *runtime,
                                              unsigned output_width,
                                              unsigned left_margin,
                                              int hud_shift_x,
                                              unsigned x_begin,
                                              unsigned x_end,
                                              uint16_t *bgr555,
                                              size_t bgr555_capacity,
                                              uint32_t *bgra,
                                              size_t bgra_capacity,
                                              SCV28VideoReport *report);

/* T071: dirty-tile render function (implemented in sc_v28_video.c) */
void sc_v28_render_dirty_tiles(const uint8_t *dirty_bitmap, uint32_t dirty_tile_count,
                               int hud_shift_x, unsigned width, unsigned height,
                               uint32_t *bgra, const SCV11Runtime *rt);

#endif
