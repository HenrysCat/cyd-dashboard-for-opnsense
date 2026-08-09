#include "page_services.h"

#include <stdio.h>

#include "../data_model.h"
#include "../ui_theme.h"

static lv_obj_t *label_summary;

// One label per slot, created up front and reused. Rebuilding the list on
// every poll would churn the LVGL heap; instead each slot's text/colour is
// updated in place and unused slots are hidden.
static const size_t ROWS = 11;
static const size_t COLS = 2;
static lv_obj_t *slots[ROWS * COLS];

static const lv_coord_t ROW_HEIGHT = 16;
static const lv_coord_t COL_WIDTH = 152;
static const lv_coord_t LIST_TOP = 52;

lv_obj_t *page_services_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Services");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    label_summary = lv_label_create(scr);
    lv_obj_set_style_text_color(label_summary, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_summary, "-- running");
    lv_obj_align(label_summary, LV_ALIGN_TOP_MID, 0, 30);

    for (size_t i = 0; i < ROWS * COLS; i++) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, COL_WIDTH);
        lv_label_set_text(l, "");
        lv_obj_set_pos(l, 8 + (lv_coord_t)(i / ROWS) * COL_WIDTH,
                       LIST_TOP + (lv_coord_t)(i % ROWS) * ROW_HEIGHT);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        slots[i] = l;
    }

    return scr;
}

void page_services_update() {
    if (!g_dashboard.services_valid) return;

    char buf[64];
    // services_total is the real count from the middleware, which can exceed
    // what fits on screen -- report the true figure, not just what's shown.
    snprintf(buf, sizeof(buf), "%u of %u running", (unsigned)g_dashboard.services_running,
             (unsigned)g_dashboard.services_total);
    lv_label_set_text(label_summary, buf);

    for (size_t i = 0; i < ROWS * COLS; i++) {
        if (i < g_dashboard.service_count) {
            const ServiceEntry &s = g_dashboard.services[i];
            lv_label_set_text(slots[i], s.name.c_str());
            lv_obj_set_style_text_color(
                slots[i],
                s.running ? ui_green() : ui_red(), 0);
            lv_obj_clear_flag(slots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(slots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
