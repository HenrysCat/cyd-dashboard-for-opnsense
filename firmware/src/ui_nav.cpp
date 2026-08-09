#include "ui_nav.h"

#include <WiFi.h>
#include <lvgl.h>

#include "data_model.h"
#include "display_driver.h"
#include "pages/page_clients.h"
#include "pages/page_cpu_thermal.h"
#include "pages/page_crowdsec.h"
#include "pages/page_firewall.h"
#include "pages/page_interfaces.h"
#include "pages/page_overview.h"
#include "pages/page_services.h"
#include "pages/page_sysinfo.h"
#include "pages/page_traffic.h"
#include "settings.h"
#include "ui_theme.h"

enum PageIndex {
    PAGE_OVERVIEW = 0,
    PAGE_TRAFFIC,
    PAGE_CPU_THERMAL,
    PAGE_INTERFACES_GATEWAYS,
    PAGE_SERVICES,
    PAGE_FIREWALL,
    PAGE_CLIENTS,
    PAGE_CROWDSEC,
    PAGE_SYSINFO,
    PAGE_COUNT,
};

static lv_obj_t *s_pages[PAGE_COUNT];
static lv_obj_t *s_dots[PAGE_COUNT];
static lv_obj_t *s_status_dot;

// Pages are created up front, but only the available ones take part in
// navigation. Optional pages depend on OPNsense plugins that may not be
// installed -- rather than showing an empty screen, they're left out of the
// rotation and the dot indicator shrinks to match.
static int s_visible[PAGE_COUNT];
static int s_visibleCount = 0;
static int s_currentVisible = 0;

// System Info is deliberately never "available": it is an overlay reached by
// the BOOT button, not part of the swipe rotation, so it must not appear in
// the page list or the dot indicator.
static bool page_is_available(int page) {
    if (page == PAGE_SYSINFO) return false;
    if (page == PAGE_CROWDSEC) return g_dashboard.crowdsec_available;
    return true;
}

static bool s_sysinfoShown = false;

static int current_page() {
    return s_visibleCount ? s_visible[s_currentVisible] : PAGE_OVERVIEW;
}

static void layout_dots() {
    const lv_coord_t total_width = (lv_coord_t)s_visibleCount * 10;
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i < s_visibleCount) {
            lv_obj_set_pos(s_dots[i], SCREEN_WIDTH / 2 - total_width / 2 + i * 10,
                           SCREEN_HEIGHT - 10);
            lv_obj_clear_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_dots() {
    for (int i = 0; i < s_visibleCount; i++) {
        lv_obj_set_style_bg_color(
            s_dots[i], i == s_currentVisible ? lv_color_white() : lv_palette_main(LV_PALETTE_GREY),
            0);
    }
}

// Recomputes which pages are in the rotation. Called at startup and whenever
// an optional page's availability changes (which happens once, shortly after
// the first successful fetch).
static void rebuild_visible() {
    const int wasShowing = current_page();

    s_visibleCount = 0;
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (page_is_available(i)) s_visible[s_visibleCount++] = i;
    }

    // Stay on the same page across a rebuild where possible, so a page
    // appearing doesn't yank the user somewhere else.
    s_currentVisible = 0;
    for (int i = 0; i < s_visibleCount; i++) {
        if (s_visible[i] == wasShowing) {
            s_currentVisible = i;
            break;
        }
    }

    layout_dots();
    update_dots();
}

static void go_to(int visibleIndex, bool animate_forward) {
    if (s_visibleCount == 0) return;
    if (visibleIndex < 0) visibleIndex = s_visibleCount - 1;
    if (visibleIndex >= s_visibleCount) visibleIndex = 0;
    s_currentVisible = visibleIndex;
    lv_scr_load_anim(s_pages[current_page()],
                     animate_forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                     200, 0, false);
    update_dots();
}

// Touch is handled directly rather than through LVGL's input device and
// clickable zones: the panel is read edge-triggered with a debounce, which is
// far more reliable on a noisy resistive screen.
static const uint32_t TOUCH_DEBOUNCE_MS = 300;
static bool s_touchWasDown = false;
static uint32_t s_lastTouchActionMs = 0;
static uint32_t s_lastAutoAdvanceMs = 0;

static void handle_touch() {
    uint16_t x, y;
    const bool touched = display_read_touch(&x, &y);
    const uint32_t now = millis();

    // Act only on the transition into a press, so holding a finger down pages
    // once rather than continuously.
    if (touched && !s_touchWasDown && now - s_lastTouchActionMs >= TOUCH_DEBOUNCE_MS) {
        s_lastTouchActionMs = now;
        // A tap while the overlay is up closes it -- doing nothing would just
        // look like the screen had frozen.
        if (s_sysinfoShown) {
            ui_nav_toggle_sysinfo();
            s_touchWasDown = touched;
            return;
        }
        const bool tappedLeft = x < SCREEN_WIDTH / 2;
        if (tappedLeft != getSettings().swapNav) {
            go_to(s_currentVisible - 1, false);
        } else {
            go_to(s_currentVisible + 1, true);
        }
        // Give a full interval after any manual paging, so auto-cycling
        // doesn't move the page out from under someone who just tapped.
        s_lastAutoAdvanceMs = now;
    }

    s_touchWasDown = touched;
}

static void create_dot_indicator() {
    lv_obj_t *top = lv_layer_top();
    for (int i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *dot = lv_obj_create(top);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        s_dots[i] = dot;
    }

    // Warning-only indicator: shown solely when something is wrong, so a
    // healthy dashboard stays visually clean.
    s_status_dot = lv_obj_create(top);
    lv_obj_remove_style_all(s_status_dot);
    lv_obj_set_size(s_status_dot, 6, 6);
    lv_obj_set_style_radius(s_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_status_dot, ui_red(), 0);
    lv_obj_set_pos(s_status_dot, 6, SCREEN_HEIGHT - 10);
    lv_obj_add_flag(s_status_dot, LV_OBJ_FLAG_HIDDEN);
}

// Hidden while data is arriving over a live Wi-Fi link; a red dot appears for
// no data yet, a dropped link, or values that have gone stale -- so what's on
// screen is never silently out of date. has_data covers the boot case, where
// last_fetch_ms is still 0.
static void update_status_dot() {
    const bool ok = g_dashboard.has_data && WiFi.status() == WL_CONNECTED &&
                    (millis() - g_dashboard.last_fetch_ms) < DATA_STALE_MS;

    static int last = -1;
    if ((int)ok == last) return;
    last = (int)ok;
    if (ok) {
        lv_obj_add_flag(s_status_dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_status_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_nav_init() {
    s_pages[PAGE_OVERVIEW] = page_overview_create();
    s_pages[PAGE_TRAFFIC] = page_traffic_create();
    s_pages[PAGE_CPU_THERMAL] = page_cpu_thermal_create();
    s_pages[PAGE_INTERFACES_GATEWAYS] = page_interfaces_create();
    s_pages[PAGE_SERVICES] = page_services_create();
    s_pages[PAGE_FIREWALL] = page_firewall_create();
    s_pages[PAGE_CLIENTS] = page_clients_create();
    s_pages[PAGE_CROWDSEC] = page_crowdsec_create();
    s_pages[PAGE_SYSINFO] = page_sysinfo_create();

    create_dot_indicator();
    rebuild_visible();

    lv_scr_load(s_pages[PAGE_OVERVIEW]);
}

void ui_nav_toggle_sysinfo() {
    s_sysinfoShown = !s_sysinfoShown;
    if (s_sysinfoShown) {
        page_sysinfo_update();  // populate before it becomes visible
        lv_scr_load(s_pages[PAGE_SYSINFO]);
        for (int i = 0; i < PAGE_COUNT; i++) lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_scr_load(s_pages[current_page()]);
        layout_dots();
        update_dots();
    }
}

static void handle_auto_cycle() {
    const DisplaySettings &s = getSettings();
    // Paused while the System Info overlay is up: that's a deliberate
    // inspection view, not part of the rotation.
    if (!s.autoCycle || s_sysinfoShown || s_visibleCount < 2) return;

    const uint16_t secs = constrain(s.autoCycleSeconds, AUTO_CYCLE_MIN_S, AUTO_CYCLE_MAX_S);
    const uint32_t now = millis();
    if (now - s_lastAutoAdvanceMs < (uint32_t)secs * 1000UL) return;
    s_lastAutoAdvanceMs = now;
    go_to(s_currentVisible + 1, true);
}

void ui_nav_update() {
    handle_touch();
    handle_auto_cycle();
    update_status_dot();

    // Availability is only known after the first fetch, so optional pages join
    // the rotation shortly after boot rather than at init.
    static bool lastCrowdsec = false;
    if (g_dashboard.crowdsec_available != lastCrowdsec) {
        lastCrowdsec = g_dashboard.crowdsec_available;
        rebuild_visible();
    }

    // Page content only changes when new data arrives, or when you switch to a
    // different page. Without this guard the active page's update() ran on
    // every loop iteration (~5ms), issuing hundreds of lv_label_set_text() and
    // style writes per second -- each one invalidating the object and forcing a
    // re-layout, which starves LVGL's redraw and makes the UI stop responding.
    const int active = s_sysinfoShown ? (int)PAGE_SYSINFO : current_page();

    static unsigned long lastRenderedFetch = 0;
    static int lastRenderedPage = -1;
    if (g_dashboard.last_fetch_ms == lastRenderedFetch && active == lastRenderedPage) {
        return;
    }
    lastRenderedFetch = g_dashboard.last_fetch_ms;
    lastRenderedPage = active;

    switch (active) {
        case PAGE_OVERVIEW:
            page_overview_update();
            break;
        case PAGE_TRAFFIC:
            page_traffic_update();
            break;
        case PAGE_CPU_THERMAL:
            page_cpu_thermal_update();
            break;
        case PAGE_INTERFACES_GATEWAYS:
            page_interfaces_update();
            break;
        case PAGE_SERVICES:
            page_services_update();
            break;
        case PAGE_FIREWALL:
            page_firewall_update();
            break;
        case PAGE_CLIENTS:
            page_clients_update();
            break;
        case PAGE_CROWDSEC:
            page_crowdsec_update();
            break;
        case PAGE_SYSINFO:
            page_sysinfo_update();
            break;
        default:
            break;
    }
}
