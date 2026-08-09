// Trimmed LVGL 8.3 configuration for an ESP32 with no PSRAM (CYD).
// Anything not set here falls back to LVGL's own defaults via
// lv_conf_internal.h -- this only overrides what actually matters for this
// board/project (memory budget, color format, tick source, widgets/fonts we
// use).
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Use the ESP32's own heap rather than a fixed static pool.
 *
 * A static pool has to be sized up front out of a ~160KB static DRAM budget
 * shared with the WiFi/HTTP/JSON buffers and the display draw buffer: 48KB
 * overflowed that budget outright, and the 24KB that did fit was then not
 * enough for six pages' worth of objects and label text -- LVGL's allocation
 * failure assert is an infinite loop, which showed up as the whole device
 * hard-freezing with no crash output at all.
 *
 * malloc draws from the runtime heap instead (~200KB free here), so LVGL takes
 * only what it actually needs and isn't capped by a compile-time guess. */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

/* ILI9341 is RGB565 */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* We call lv_tick_inc() ourselves from a millis()-based timer in main.cpp */
#define LV_TICK_CUSTOM 0

#define LV_USE_LOG 0

/* Widgets used by the dashboard pages */
#define LV_USE_ARC     1
#define LV_USE_BAR     1
#define LV_USE_CHART   1
#define LV_USE_LABEL   1
#define LV_USE_LINE    1

/* Fonts: default 14 for body text, larger sizes for gauge readouts */
#define LV_FONT_MONTSERRAT_12 1  /* dense firewall log rows */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* The built-in theme defaults to its LIGHT palette, which gives charts and
 * arcs near-white backgrounds that look wrong against this dashboard's black
 * pages (and can't be fully undone with per-widget styles). Switch the theme
 * itself to dark so widget defaults are sane. */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

#endif /*LV_CONF_H*/
#endif /*1*/
