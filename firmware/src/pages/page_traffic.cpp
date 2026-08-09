#include "page_traffic.h"

#include "../data_model.h"

static lv_obj_t *label_in_val;
static lv_obj_t *label_out_val;
static lv_obj_t *label_lan;
static lv_obj_t *chart_traffic;
static lv_chart_series_t *series_in;
static lv_chart_series_t *series_out;

static String formatBps(uint32_t bps) {
    if (bps >= 1000000) return String(bps / 1000000.0, 1) + " Mbps";
    if (bps >= 1000) return String(bps / 1000.0, 1) + " Kbps";
    return String(bps) + " bps";
}

// lv_coord_t is a 16-bit signed value, far too small for raw bps on a fast
// WAN link. Plot in whole Kbps and clamp so a very high burst just saturates
// the chart's top rather than wrapping/overflowing.
static lv_coord_t toChartKbps(float bps) {
    long kbps = (long)(bps / 1000.0f);
    if (kbps < 0) kbps = 0;
    if (kbps > 32000) kbps = 32000;
    return (lv_coord_t)kbps;
}

lv_obj_t *page_traffic_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Traffic (WAN)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *in_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(in_caption, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(in_caption, "In");
    lv_obj_align(in_caption, LV_ALIGN_TOP_LEFT, 10, 34);

    label_in_val = lv_label_create(scr);
    lv_obj_set_style_text_font(label_in_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_in_val, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_label_set_text(label_in_val, "-- bps");
    lv_obj_align(label_in_val, LV_ALIGN_TOP_LEFT, 10, 52);

    lv_obj_t *out_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(out_caption, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(out_caption, "Out");
    lv_obj_align(out_caption, LV_ALIGN_TOP_RIGHT, -10, 34);

    label_out_val = lv_label_create(scr);
    lv_obj_set_style_text_font(label_out_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_out_val, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(label_out_val, "-- bps");
    lv_obj_align(label_out_val, LV_ALIGN_TOP_RIGHT, -10, 52);

    // LAN shown as text rather than plotted: local throughput is routinely an
    // order of magnitude larger than WAN, so sharing the chart's Y axis would
    // flatten the WAN trace into a straight line at the bottom.
    label_lan = lv_label_create(scr);
    lv_obj_set_style_text_font(label_lan, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_lan, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_lan, "LAN  in --  out --");
    lv_obj_align(label_lan, LV_ALIGN_TOP_MID, 0, 80);

    chart_traffic = lv_chart_create(scr);
    lv_obj_set_size(chart_traffic, 300, 104);
    // Leaves room below for the page-dot indicator drawn on the top layer.
    lv_obj_align(chart_traffic, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_chart_set_type(chart_traffic, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_traffic, SPARKLINE_SAMPLES);
    lv_chart_set_div_line_count(chart_traffic, 3, 0);
    lv_obj_set_style_size(chart_traffic, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(chart_traffic, lv_color_hex(0x141414), 0);
    lv_obj_set_style_border_color(chart_traffic, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(chart_traffic, 1, 0);
    lv_obj_set_style_line_color(chart_traffic, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_traffic, 2, 0);

    series_in = lv_chart_add_series(chart_traffic, lv_palette_main(LV_PALETTE_BLUE),
                                     LV_CHART_AXIS_PRIMARY_Y);
    series_out = lv_chart_add_series(chart_traffic, lv_palette_main(LV_PALETTE_ORANGE),
                                      LV_CHART_AXIS_PRIMARY_Y);

    return scr;
}

void page_traffic_update() {
    if (!g_dashboard.traffic_valid) return;

    lv_label_set_text(label_in_val, formatBps(g_dashboard.wan_in_bps).c_str());
    lv_label_set_text(label_out_val, formatBps(g_dashboard.wan_out_bps).c_str());

    String lan = "LAN  in " + formatBps(g_dashboard.lan_in_bps) + "   out " +
                 formatBps(g_dashboard.lan_out_bps);
    lv_label_set_text(label_lan, lan.c_str());

    lv_coord_t max_kbps = 100;  // floor, so an idle link doesn't look like a flat wall
    for (size_t i = 0; i < g_dashboard.traffic_in_history.count; i++) {
        max_kbps = max(max_kbps, toChartKbps(g_dashboard.traffic_in_history.at(i)));
    }
    for (size_t i = 0; i < g_dashboard.traffic_out_history.count; i++) {
        max_kbps = max(max_kbps, toChartKbps(g_dashboard.traffic_out_history.at(i)));
    }
    lv_chart_set_range(chart_traffic, LV_CHART_AXIS_PRIMARY_Y, 0, max_kbps);

    for (size_t i = 0; i < g_dashboard.traffic_in_history.count; i++) {
        lv_chart_set_value_by_id(chart_traffic, series_in, i,
                                  toChartKbps(g_dashboard.traffic_in_history.at(i)));
    }
    for (size_t i = 0; i < g_dashboard.traffic_out_history.count; i++) {
        lv_chart_set_value_by_id(chart_traffic, series_out, i,
                                  toChartKbps(g_dashboard.traffic_out_history.at(i)));
    }
    lv_chart_refresh(chart_traffic);
}
