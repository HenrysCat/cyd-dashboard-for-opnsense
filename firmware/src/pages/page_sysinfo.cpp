#include "page_sysinfo.h"

#include <WiFi.h>
#include <stdio.h>

#include "../api_client.h"
#include "../data_model.h"
#include "../display_driver.h"
#include "../ui_theme.h"

static lv_obj_t *label_url;
static lv_obj_t *label_mdns;

static const size_t INFO_ROWS = 9;
static lv_obj_t *rows[INFO_ROWS];

static const lv_coord_t ROW_TOP = 84;
static const lv_coord_t ROW_HEIGHT = 15;

static String formatUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    unsigned long days = seconds / 86400;
    unsigned long hours = (seconds % 86400) / 3600;
    unsigned long mins = (seconds % 3600) / 60;
    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "%lud %luh %lum", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, sizeof(buf), "%luh %lum", hours, mins);
    } else {
        snprintf(buf, sizeof(buf), "%lum %lus", mins, seconds % 60);
    }
    return String(buf);
}

lv_obj_t *page_sysinfo_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "System Info");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    // The web UI address is the main reason to open this page, so it gets the
    // larger font and the accent colour rather than being one row among many.
    label_url = lv_label_create(scr);
    lv_obj_set_style_text_color(label_url, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(label_url, "settings: --");
    lv_obj_set_pos(label_url, 8, 30);

    label_mdns = lv_label_create(scr);
    lv_obj_set_style_text_font(label_mdns, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_mdns, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_mdns, "http://cyd-dash.local/");
    lv_obj_set_pos(label_mdns, 8, 52);

    for (size_t i = 0; i < INFO_ROWS; i++) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, SCREEN_WIDTH - 16);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
        lv_label_set_text(l, "");
        lv_obj_set_pos(l, 8, ROW_TOP + (lv_coord_t)i * ROW_HEIGHT);
        rows[i] = l;
    }

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x777777), 0);
    lv_label_set_text(hint, "BOOT or tap to close");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    return scr;
}

void page_sysinfo_update() {
    char buf[96];

    snprintf(buf, sizeof(buf), "settings: http://%s/", WiFi.localIP().toString().c_str());
    lv_label_set_text(label_url, buf);

    size_t r = 0;

    snprintf(buf, sizeof(buf), "Wi-Fi:  %s  (%d dBm)", WiFi.SSID().c_str(), (int)WiFi.RSSI());
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "MAC:    %s", WiFi.macAddress().c_str());
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "Data:   %s", g_apiClient.baseUrl().c_str());
    lv_label_set_text(rows[r++], buf);

    // Age of the last good fetch, which is what the corner warning dot keys off.
    const bool fresh = g_dashboard.has_data &&
                       (millis() - g_dashboard.last_fetch_ms) < DATA_STALE_MS;
    if (g_dashboard.has_data) {
        snprintf(buf, sizeof(buf), "Feed:   %s, %lus ago", fresh ? "ok" : "STALE",
                 (millis() - g_dashboard.last_fetch_ms) / 1000);
    } else {
        snprintf(buf, sizeof(buf), "Feed:   no data yet");
    }
    lv_label_set_text(rows[r], buf);
    lv_obj_set_style_text_color(rows[r], fresh ? ui_green() : ui_red(), 0);
    r++;

    // Free vs total, plus the low-water mark: a min that keeps dropping over
    // days is the signature of a leak, which a single free-heap figure hides.
    snprintf(buf, sizeof(buf), "Heap:   %u KB free of %u KB (min %u)",
             (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024),
             (unsigned)(ESP.getMinFreeHeap() / 1024));
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "Frag:   largest block %u KB",
             (unsigned)(ESP.getMaxAllocHeap() / 1024));
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "Chip:   %s  %d core  %lu MHz", ESP.getChipModel(),
             (int)ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "Uptime: %s   Flash: %u MB", formatUptime(millis()).c_str(),
             (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
    lv_label_set_text(rows[r++], buf);

    snprintf(buf, sizeof(buf), "Build:  %s %s", __DATE__, __TIME__);
    lv_label_set_text(rows[r++], buf);
}
