#include "page_crowdsec.h"

#include <stdio.h>

#include "../data_model.h"
#include "../display_driver.h"
#include "../ui_theme.h"

static lv_obj_t *label_summary;
static lv_obj_t *rows[MAX_CS_RECENT];

static const lv_coord_t ROW_TOP = 74;
static const lv_coord_t ROW_HEIGHT = 15;

lv_obj_t *page_crowdsec_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "CrowdSec");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    label_summary = lv_label_create(scr);
    lv_label_set_text(label_summary, "--");
    lv_obj_align(label_summary, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *hdr = lv_label_create(scr);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x888888), 0);
    lv_label_set_text(hdr, "recent alerts");
    lv_obj_set_pos(hdr, 8, 58);

    for (size_t i = 0; i < MAX_CS_RECENT; i++) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, SCREEN_WIDTH - 16);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(l, ui_red(), 0);
        lv_label_set_text(l, "");
        lv_obj_set_pos(l, 8, ROW_TOP + (lv_coord_t)i * ROW_HEIGHT);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        rows[i] = l;
    }

    return scr;
}

void page_crowdsec_update() {
    if (!g_dashboard.crowdsec_valid) return;

    char buf[128];
    snprintf(buf, sizeof(buf), "%s   %lu active   %lu alerts",
             g_dashboard.crowdsec_running ? "running" : "STOPPED",
             (unsigned long)g_dashboard.cs_active_decisions,
             (unsigned long)g_dashboard.cs_alerts_total);
    lv_label_set_text(label_summary, buf);
    lv_obj_set_style_text_color(
        label_summary, g_dashboard.crowdsec_running ? ui_green() : ui_red(), 0);

    for (size_t i = 0; i < MAX_CS_RECENT; i++) {
        if (i < g_dashboard.cs_recent_count) {
            const CrowdsecEntry &e = g_dashboard.cs_recent[i];
            // Country and AS give the useful context at a glance -- an IP alone
            // rarely tells you anything.
            snprintf(buf, sizeof(buf), "%s  %s  %s", e.ip.c_str(), e.country.c_str(),
                     e.reason.c_str());
            lv_label_set_text(rows[i], buf);
            lv_obj_clear_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
