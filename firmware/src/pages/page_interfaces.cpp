#include "page_interfaces.h"

#include <stdio.h>

#include "../data_model.h"

static lv_obj_t *label_interfaces;
static lv_obj_t *label_gateways;
static lv_obj_t *label_traffic;

lv_obj_t *page_interfaces_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Interfaces & Gateways");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *if_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(if_caption, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(if_caption, "Interfaces");
    lv_obj_align(if_caption, LV_ALIGN_TOP_LEFT, 10, 32);

    label_interfaces = lv_label_create(scr);
    lv_label_set_text(label_interfaces, "--");
    lv_obj_align(label_interfaces, LV_ALIGN_TOP_LEFT, 10, 52);

    lv_obj_t *gw_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(gw_caption, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(gw_caption, "Gateways");
    lv_obj_align(gw_caption, LV_ALIGN_TOP_LEFT, 168, 32);

    label_gateways = lv_label_create(scr);
    lv_label_set_text(label_gateways, "--");
    lv_obj_align(label_gateways, LV_ALIGN_TOP_LEFT, 168, 52);

    label_traffic = lv_label_create(scr);
    lv_obj_set_style_text_color(label_traffic, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_traffic, "");
    lv_obj_align(label_traffic, LV_ALIGN_BOTTOM_LEFT, 10, -22);

    return scr;
}

static String formatRate(uint32_t bps) {
    char buf[24];
    if (bps >= 1000000) {
        snprintf(buf, sizeof(buf), "%.1f Mbps", bps / 1000000.0);
    } else if (bps >= 1000) {
        snprintf(buf, sizeof(buf), "%.1f Kbps", bps / 1000.0);
    } else {
        snprintf(buf, sizeof(buf), "%lu bps", (unsigned long)bps);
    }
    return String(buf);
}

void page_interfaces_update() {
    if (g_dashboard.interfaces_valid) {
        String s;
        for (size_t i = 0; i < g_dashboard.interface_count; i++) {
            const InterfaceEntry &e = g_dashboard.interfaces[i];
            if (i) s += "\n";
            // Friendly name first -- that's what's meaningful at a glance; the
            // device name is the detail you only need occasionally.
            s += e.name + "\n  " + e.device;
        }
        if (g_dashboard.interface_count == 0) s = "none";
        lv_label_set_text(label_interfaces, s.c_str());
    }

    if (g_dashboard.gateways_valid) {
        String s;
        for (size_t i = 0; i < g_dashboard.gateway_count; i++) {
            const GatewayEntry &e = g_dashboard.gateways[i];
            if (i) s += "\n";
            s += e.name + "\n  " + (e.address.length() ? e.address : String("(no address)"));
        }
        if (g_dashboard.gateway_count == 0) s = "none";
        lv_label_set_text(label_gateways, s.c_str());
    }

    if (g_dashboard.traffic_valid) {
        String s = "WAN  in " + formatRate(g_dashboard.wan_in_bps) + "   out " +
                   formatRate(g_dashboard.wan_out_bps);
        lv_label_set_text(label_traffic, s.c_str());
    }
}
