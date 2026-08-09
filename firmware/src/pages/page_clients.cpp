#include "page_clients.h"

#include <stdio.h>

#include "../data_model.h"
#include "../display_driver.h"
#include "../ui_theme.h"

static lv_obj_t *label_summary;
// Fixed pool of row labels, reused in place rather than rebuilt each poll.
static lv_obj_t *rows[MAX_CLIENTS];

static const size_t ROWS = 11;
static const size_t COLS = 2;
static const lv_coord_t ROW_TOP = 52;
static const lv_coord_t ROW_HEIGHT = 14;
static const lv_coord_t COL_WIDTH = 154;

lv_obj_t *page_clients_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Clients");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    label_summary = lv_label_create(scr);
    lv_obj_set_style_text_color(label_summary, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_summary, "--");
    lv_obj_align(label_summary, LV_ALIGN_TOP_MID, 0, 30);

    for (size_t i = 0; i < ROWS * COLS; i++) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, COL_WIDTH - 6);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_label_set_text(l, "");
        lv_obj_set_pos(l, 6 + (lv_coord_t)(i / ROWS) * COL_WIDTH,
                       ROW_TOP + (lv_coord_t)(i % ROWS) * ROW_HEIGHT);
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        rows[i] = l;
    }

    return scr;
}

void page_clients_update() {
    if (!g_dashboard.clients_valid) return;

    char buf[96];
    // Totals come from the middleware, so the count stays honest even though
    // only the first MAX_CLIENTS entries are held for display.
    snprintf(buf, sizeof(buf), "%u of %u online", (unsigned)g_dashboard.clients_online,
             (unsigned)g_dashboard.clients_total);
    lv_label_set_text(label_summary, buf);

    for (size_t i = 0; i < ROWS * COLS; i++) {
        if (i < g_dashboard.client_count) {
            const ClientEntry &c = g_dashboard.clients[i];
            // Last octet only: the subnet is the same for everything here, so
            // the full address would just crowd out the device name.
            const char *last = strrchr(c.ip.c_str(), '.');
            snprintf(buf, sizeof(buf), "%s  %s", last ? last + 1 : c.ip.c_str(), c.name.c_str());
            lv_label_set_text(rows[i], buf);
            lv_obj_set_style_text_color(
                rows[i], c.online ? ui_green() : lv_color_hex(0x777777), 0);
            lv_obj_clear_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
