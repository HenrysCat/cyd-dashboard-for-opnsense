#include "page_overview.h"

#include <stdio.h>

#include "../data_model.h"
#include "../display_driver.h"

static lv_obj_t *label_hostname;
static lv_obj_t *label_uptime;
static lv_obj_t *label_load;

static lv_obj_t *arc_cpu, *arc_mem, *arc_disk;
static lv_obj_t *label_cpu_val, *label_mem_val, *label_disk_val;

static lv_obj_t *chart_cpu;
static lv_chart_series_t *chart_cpu_series;
static lv_obj_t *label_update;

// LVGL's lv_label_set_text_fmt() uses its own cut-down printf that does NOT
// implement %f unless LV_SPRINTF_USE_FLOAT is enabled -- it emits the literal
// characters instead (a percentage rendered as "f%"). Formatting through the
// C library's snprintf and setting the finished string avoids depending on
// that build option at all.
static void set_label_float(lv_obj_t *label, const char *fmt, float a, float b = 0, float c = 0) {
    char buf[48];
    snprintf(buf, sizeof(buf), fmt, a, b, c);
    lv_label_set_text(label, buf);
}

static void make_gauge(lv_obj_t *parent, lv_coord_t x, const char *caption, lv_obj_t **out_arc,
                       lv_obj_t **out_label) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 100, 105);
    lv_obj_set_pos(cont, x, 50);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *arc = lv_arc_create(cont);
    lv_obj_set_size(arc, 76, 76);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 0);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    // Unstyled, the default theme draws the track near-white, which reads as a
    // solid ring and hides the indicator.
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(arc);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "--");
    lv_obj_center(label);

    lv_obj_t *caption_label = lv_label_create(cont);
    lv_obj_set_style_text_color(caption_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(caption_label, caption);
    lv_obj_align(caption_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    *out_arc = arc;
    *out_label = label;
}

lv_obj_t *page_overview_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    label_hostname = lv_label_create(scr);
    lv_obj_set_style_text_font(label_hostname, &lv_font_montserrat_20, 0);
    lv_label_set_text(label_hostname, "OPNsense");
    lv_obj_align(label_hostname, LV_ALIGN_TOP_MID, 0, 2);

    label_uptime = lv_label_create(scr);
    lv_obj_set_style_text_color(label_uptime, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_uptime, "uptime: --");
    lv_obj_align(label_uptime, LV_ALIGN_TOP_LEFT, 6, 30);

    label_load = lv_label_create(scr);
    lv_obj_set_style_text_color(label_load, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_load, "load: -- -- --");
    lv_obj_align(label_load, LV_ALIGN_TOP_RIGHT, -6, 30);

    make_gauge(scr, 5, "CPU", &arc_cpu, &label_cpu_val);
    make_gauge(scr, 110, "RAM", &arc_mem, &label_mem_val);
    make_gauge(scr, 215, "Disk", &arc_disk, &label_disk_val);

    chart_cpu = lv_chart_create(scr);
    lv_obj_set_size(chart_cpu, 300, 56);
    // Leaves room below for the page-dot indicator drawn on the top layer.
    lv_obj_align(chart_cpu, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_chart_set_type(chart_cpu, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart_cpu, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(chart_cpu, SPARKLINE_SAMPLES);
    lv_chart_set_div_line_count(chart_cpu, 0, 0);
    lv_obj_set_style_size(chart_cpu, 0, LV_PART_INDICATOR);  // hide point markers
    lv_obj_set_style_bg_color(chart_cpu, lv_color_hex(0x141414), 0);
    lv_obj_set_style_border_color(chart_cpu, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(chart_cpu, 1, 0);
    lv_obj_set_style_pad_all(chart_cpu, 2, 0);
    chart_cpu_series =
        lv_chart_add_series(chart_cpu, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y);

    // Sits on the bottom row beside the page dots, which is otherwise empty
    // space -- shown only when an update is actually pending.
    label_update = lv_label_create(scr);
    lv_obj_set_style_text_font(label_update, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_update, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(label_update, "update available");
    lv_obj_set_pos(label_update, 18, SCREEN_HEIGHT - 14);
    lv_obj_add_flag(label_update, LV_OBJ_FLAG_HIDDEN);

    return scr;
}

void page_overview_update() {
    if (g_dashboard.updates_valid) {
        if (g_dashboard.update_available) {
            lv_obj_clear_flag(label_update, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_update, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_dashboard.system_valid) {
        lv_label_set_text(label_hostname, g_dashboard.hostname.c_str());
        lv_label_set_text_fmt(label_uptime, "uptime: %s", g_dashboard.uptime.c_str());
        set_label_float(label_load, "load: %.2f %.2f %.2f", g_dashboard.load_avg[0],
                        g_dashboard.load_avg[1], g_dashboard.load_avg[2]);
    }

    if (g_dashboard.resources_valid) {
        lv_arc_set_value(arc_cpu, (int16_t)g_dashboard.cpu_pct);
        set_label_float(label_cpu_val, "%.0f%%", g_dashboard.cpu_pct);

        lv_arc_set_value(arc_mem, (int16_t)g_dashboard.mem_pct);
        set_label_float(label_mem_val, "%.0f%%", g_dashboard.mem_pct);

        lv_arc_set_value(arc_disk, (int16_t)g_dashboard.disk_pct);
        set_label_float(label_disk_val, "%.0f%%", g_dashboard.disk_pct);

        for (size_t i = 0; i < g_dashboard.cpu_history.count; i++) {
            lv_chart_set_value_by_id(chart_cpu, chart_cpu_series, i,
                                     (lv_coord_t)g_dashboard.cpu_history.at(i));
        }
        lv_chart_refresh(chart_cpu);
    }
}
