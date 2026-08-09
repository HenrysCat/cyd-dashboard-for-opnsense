#include "page_firewall.h"

#include <stdio.h>

#include "../data_model.h"
#include "../display_driver.h"
#include "../ui_theme.h"

static lv_obj_t *label_rate;
static lv_obj_t *label_top;
// Fixed pool of row labels, reused in place. Rebuilding the list every poll
// would churn the LVGL heap on a board with no PSRAM.
static lv_obj_t *rows[MAX_FW_RECENT];

static const lv_coord_t ROW_TOP = 72;
// 12px font keeps the source IP, port, protocol and rule label on one line
// without clipping, and fits more history on screen.
static const lv_coord_t ROW_HEIGHT = 14;

lv_obj_t *page_firewall_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Firewall");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    label_rate = lv_label_create(scr);
    lv_obj_set_style_text_font(label_rate, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_rate, ui_red(), 0);
    lv_label_set_text(label_rate, "--");
    lv_obj_set_pos(label_rate, 8, 28);

    label_top = lv_label_create(scr);
    lv_obj_set_style_text_color(label_top, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_top, "");
    lv_obj_set_pos(label_top, 8, 56);

    for (size_t i = 0; i < MAX_FW_RECENT; i++) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, SCREEN_WIDTH - 16);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_label_set_text(l, "");
        lv_obj_set_pos(l, 8, ROW_TOP + (lv_coord_t)i * ROW_HEIGHT);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        rows[i] = l;
    }

    return scr;
}

void page_firewall_update() {
    if (!g_dashboard.firewall_valid) return;

    char buf[96];
    snprintf(buf, sizeof(buf), "%lu blocks/min", (unsigned long)g_dashboard.fw_blocks_per_min);
    lv_label_set_text(label_rate, buf);

    // The rate is an average over however long the sampled entries span, so
    // show that window -- 5 blocks/min over 10s and over 10min are different
    // claims, and without the window you can't tell them apart.
    if (g_dashboard.fw_top_src.length()) {
        snprintf(buf, sizeof(buf), "over %lus   top: %s (%lu)",
                 (unsigned long)g_dashboard.fw_window_seconds, g_dashboard.fw_top_src.c_str(),
                 (unsigned long)g_dashboard.fw_top_src_count);
    } else {
        snprintf(buf, sizeof(buf), "over %lus", (unsigned long)g_dashboard.fw_window_seconds);
    }
    lv_label_set_text(label_top, buf);

    for (size_t i = 0; i < MAX_FW_RECENT; i++) {
        if (i < g_dashboard.fw_recent_count) {
            const FirewallEntry &e = g_dashboard.fw_recent[i];
            snprintf(buf, sizeof(buf), "%s  %s  :%s %s  %s", e.time.c_str(), e.src.c_str(),
                     e.dst_port.c_str(), e.proto.c_str(), e.label.c_str());
            lv_label_set_text(rows[i], buf);
            lv_obj_set_style_text_color(
                rows[i],
                e.blocked ? ui_red() : ui_green(), 0);
            lv_obj_clear_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
