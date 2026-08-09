#include <Arduino.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <lvgl.h>

#include "api_client.h"
#include "data_model.h"
#include "display_driver.h"
#include "settings.h"
#include "ui_nav.h"
#include "web_ui.h"
#include "wifi_setup.h"

DashboardData g_dashboard;

static const int WDT_TIMEOUT_S = 30;

void setup() {
    Serial.begin(115200);

    // Panel (TFT + LVGL) first, no touch: the Wi-Fi captive portal message
    // screen needs somewhere to draw, but shouldn't be gated behind touch
    // calibration succeeding -- a touch axis swap makes calibration
    // impossible to complete, which would otherwise deadlock setup() before
    // the portal (the only place that swap can be fixed) ever ran.
    // Settings must load before the panel is initialised: orientation and
    // backlight are applied as part of display_init_panel().
    settingsLoad();

    display_init_panel();

    String middlewareUrl = wifi_setup_begin();
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Middleware URL: ");
    Serial.println(middlewareUrl);

    display_init_touch();

    web_ui_begin();
    g_apiClient.begin(middlewareUrl.c_str());

    ui_nav_init();

    // Auto-recovery for a stalled main loop. This is not hypothetical: an LVGL
    // heap exhaustion earlier in development left the device hard-frozen with
    // no crash and no reboot -- it simply stopped, and only a power cycle
    // brought it back. A wall-mounted unit needs to recover on its own.
    //
    // The timeout is generous because a Wi-Fi reconnect attempt can block the
    // loop for several seconds; this is meant to catch a genuine hang, not to
    // police latency.
    esp_task_wdt_init(WDT_TIMEOUT_S, true);  // true = panic (reset) on timeout
    esp_task_wdt_add(NULL);                  // watch the Arduino loop task
    Serial.print("Task watchdog armed: ");
    Serial.print(WDT_TIMEOUT_S);
    Serial.println("s");
}

void loop() {
    static uint32_t last_tick = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    // Feed first: everything below is what could plausibly stall.
    esp_task_wdt_reset();

    display_loop();
    g_apiClient.loop();
    ui_nav_update();
    web_ui_loop();
    wifi_setup_poll_reset_button();
    if (wifi_setup_take_short_press()) ui_nav_toggle_sysinfo();
    wifi_setup_poll_connection();

    delay(5);
}
