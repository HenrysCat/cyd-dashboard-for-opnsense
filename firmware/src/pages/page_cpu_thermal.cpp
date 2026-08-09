#include "page_cpu_thermal.h"

#include <stdio.h>

#include "../data_model.h"
#include "../ui_theme.h"

static lv_obj_t *label_cpu_big;
static lv_obj_t *label_load;
static lv_obj_t *label_cores;
static lv_obj_t *label_zones;
static lv_obj_t *chart_temp;
static lv_chart_series_t *series_temp;

// Typical CYD-attached firewall CPUs idle around 45-60C, so a fixed 20-80
// window shows real variation. A 0-100 range would render normal operation as
// an almost flat line.
static const lv_coord_t TEMP_CHART_MIN = 20;
static const lv_coord_t TEMP_CHART_MAX = 80;

lv_obj_t *page_cpu_thermal_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "CPU & Thermal");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *cpu_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(cpu_caption, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(cpu_caption, "CPU");
    lv_obj_align(cpu_caption, LV_ALIGN_TOP_LEFT, 10, 32);

    label_cpu_big = lv_label_create(scr);
    lv_obj_set_style_text_font(label_cpu_big, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_cpu_big, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(label_cpu_big, "--%");
    lv_obj_align(label_cpu_big, LV_ALIGN_TOP_LEFT, 10, 50);

    label_load = lv_label_create(scr);
    lv_obj_set_style_text_color(label_load, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_load, "load: -- -- --");
    lv_obj_align(label_load, LV_ALIGN_TOP_LEFT, 10, 82);

    // Cores and zones are separate columns rather than stacked blocks: the
    // core list grows with core count, so anything positioned below it at a
    // fixed offset would collide on a machine with more cores.
    lv_obj_t *cores_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(cores_caption, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(cores_caption, "cores");
    lv_obj_set_pos(cores_caption, 186, 32);

    label_cores = lv_label_create(scr);
    lv_label_set_text(label_cores, "--");
    lv_obj_set_pos(label_cores, 186, 50);

    lv_obj_t *zones_caption = lv_label_create(scr);
    lv_obj_set_style_text_color(zones_caption, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_label_set_text(zones_caption, "zones");
    lv_obj_set_pos(zones_caption, 254, 32);

    label_zones = lv_label_create(scr);
    lv_obj_set_style_text_color(label_zones, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(label_zones, "--");
    lv_obj_set_pos(label_zones, 254, 50);

    chart_temp = lv_chart_create(scr);
    lv_obj_set_size(chart_temp, 300, 96);
    lv_obj_align(chart_temp, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y, TEMP_CHART_MIN, TEMP_CHART_MAX);
    lv_chart_set_point_count(chart_temp, SPARKLINE_SAMPLES);
    lv_chart_set_div_line_count(chart_temp, 3, 0);
    lv_obj_set_style_size(chart_temp, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(chart_temp, lv_color_hex(0x141414), 0);
    lv_obj_set_style_border_color(chart_temp, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(chart_temp, 1, 0);
    lv_obj_set_style_line_color(chart_temp, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart_temp, 2, 0);
    series_temp =
        lv_chart_add_series(chart_temp, ui_red(), LV_CHART_AXIS_PRIMARY_Y);

    return scr;
}

void page_cpu_thermal_update() {
    char buf[96];

    if (g_dashboard.system_valid) {
        snprintf(buf, sizeof(buf), "load: %.2f %.2f %.2f", g_dashboard.load_avg[0],
                 g_dashboard.load_avg[1], g_dashboard.load_avg[2]);
        lv_label_set_text(label_load, buf);
    }

    if (!g_dashboard.resources_valid) return;

    snprintf(buf, sizeof(buf), "%.0f%%", g_dashboard.cpu_pct);
    lv_label_set_text(label_cpu_big, buf);

    // One per line so a hot outlier is obvious rather than averaged away.
    String cores;
    for (size_t i = 0; i < g_dashboard.cpu_temp_count; i++) {
        char t[16];
        snprintf(t, sizeof(t), "%s%.0f C", i ? "\n" : "", g_dashboard.cpu_temps[i]);
        cores += t;
    }
    lv_label_set_text(label_cores, g_dashboard.cpu_temp_count ? cores.c_str() : "--");

    String zones;
    for (size_t i = 0; i < g_dashboard.zone_temp_count; i++) {
        char t[16];
        snprintf(t, sizeof(t), "%s%.0f C", i ? "\n" : "", g_dashboard.zone_temps[i]);
        zones += t;
    }
    lv_label_set_text(label_zones, g_dashboard.zone_temp_count ? zones.c_str() : "--");

    for (size_t i = 0; i < g_dashboard.temp_history.count; i++) {
        lv_chart_set_value_by_id(chart_temp, series_temp, i,
                                 (lv_coord_t)g_dashboard.temp_history.at(i));
    }
    lv_chart_refresh(chart_temp);
}
