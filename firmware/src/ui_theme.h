#pragma once
#include <lvgl.h>

// Status colours matching TFT_eSPI's TFT_GREEN / TFT_RED, as used by the other
// CYD dashboards on this hardware. LVGL's own lv_palette_main(LV_PALETTE_*)
// gives muted Material tones (#4CAF50 / #F44336) which read as dull next to
// them. At LV_COLOR_DEPTH 16 these convert to exactly 0x07E0 and 0xF800, so
// they are the same values TFT_eSPI would write.
static inline lv_color_t ui_green() {
    return lv_color_hex(0x00FF00);
}

static inline lv_color_t ui_red() {
    return lv_color_hex(0xFF0000);
}
