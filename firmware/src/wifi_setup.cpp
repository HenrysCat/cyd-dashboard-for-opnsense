#include "wifi_setup.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <lvgl.h>

#include "display_driver.h"
#include "settings.h"

static const char *AP_NAME = "CYD-Dashboard-Setup";
static const char *AP_PASSWORD = "dashboard123";  // WPA2 minimum is 8 chars

static const uint8_t BOOT_BUTTON_PIN = 0;  // Standard ESP32 BOOT button, active-low
static const unsigned long RESET_HOLD_MS = 10000;
static unsigned long s_buttonDownSince = 0;
static bool s_shortPressPending = false;
// Below this a press is treated as contact bounce rather than intent; above
// SHORT_PRESS_MAX_MS the user is on their way to the 10s factory reset, so
// releasing there should do nothing at all.
static const unsigned long SHORT_PRESS_MIN_MS = 50;
static const unsigned long SHORT_PRESS_MAX_MS = 1000;

static void show_portal_message() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, SCREEN_WIDTH - 20);
    lv_label_set_text_fmt(
        label,
        "Wi-Fi setup needed.\n\nConnect to Wi-Fi network:\n\"%s\"\n(password: %s)\n\n"
        "Then open http://192.168.4.1 in a browser to configure your Wi-Fi and "
        "the dashboard's middleware address.",
        AP_NAME, AP_PASSWORD);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);

    lv_scr_load(scr);
    // WiFiManager's portal blocks the main loop, so flush this static screen
    // to the physical panel now -- there won't be another chance until it returns.
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        delay(10);
    }
}

// Set by ap_mode_callback, which WiFiManager only invokes when it actually
// has to show the config portal (no/failed saved Wi-Fi). Used below to tell
// apart "portal was shown and the checkbox was left unchecked" from "portal
// never ran, so the checkbox param never got a real value" -- both look like
// an empty getValue() otherwise.
static bool s_portalShown = false;

static void ap_mode_callback(WiFiManager *wm) {
    s_portalShown = true;
    show_portal_message();
}

static String checkboxHtml(bool checked) {
    return String("type=\"checkbox\"") + (checked ? " checked" : "");
}

// Fixes up a stored "mw_url" that predates the host:port + HTTPS-checkbox
// split below (e.g. a bare "192.168.0.5:8098" typed before scheme handling
// existed at all) so it self-heals on the next boot without another portal
// round-trip.
static String normalizeStoredUrl(String url) {
    url.trim();
    if (url.length() == 0) return url;
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "http://" + url;
    }
    while (url.endsWith("/")) {
        url.remove(url.length() - 1);
    }
    return url;
}

// Splits a full "http(s)://host:port" URL into its host:port part and a
// scheme flag, for pre-filling the separate host field + HTTPS checkbox shown
// by both the captive portal and the settings UI.
void wifi_setup_split_url(const String &url, String &hostPort, bool &isHttps) {
    if (url.startsWith("https://")) {
        isHttps = true;
        hostPort = url.substring(8);
    } else if (url.startsWith("http://")) {
        isHttps = false;
        hostPort = url.substring(7);
    } else {
        isHttps = false;
        hostPort = url;
    }
}

String wifi_setup_begin() {
    Preferences prefs;
    prefs.begin("netcfg", true);
    String storedUrl = normalizeStoredUrl(prefs.getString("mw_url", ""));
    prefs.end();

    String storedHostPort;
    bool storedIsHttps;
    wifi_setup_split_url(storedUrl, storedHostPort, storedIsHttps);

    // Shares the DisplaySettings store with the web UI rather than keeping a
    // second copy -- this is just an escape hatch for fixing an upside-down
    // panel before Wi-Fi (and therefore the web UI) is reachable.
    bool storedFlip180 = getSettings().flip180;

    WiFiManager wm;
    wm.setAPCallback(ap_mode_callback);

    WiFiManagerParameter customMwHost("mwhost", "Middleware host:port, e.g. 192.168.1.50:8098",
                                       storedHostPort.c_str(), 64);
    wm.addParameter(&customMwHost);

    // The middleware only serves plain HTTP out of the box (see its README) --
    // this is here for anyone who's put TLS termination in front of it
    // themselves, not because HTTPS works by default. Left unchecked, it does.
    String httpsHtml = checkboxHtml(storedIsHttps);
    WiFiManagerParameter customUseHttps(
        "usehttps",
        "Use HTTPS for the middleware (only if you've set up TLS in front of it yourself -- "
        "plain HTTP is what it serves by default)",
        "T", 2, httpsHtml.c_str());
    wm.addParameter(&customUseHttps);

    // Mounting orientation. Flips the display and the touch mapping together,
    // so taps keep landing where they look regardless of which way it's hung.
    String flipHtml = checkboxHtml(storedFlip180);
    WiFiManagerParameter customFlip180(
        "flip180", "Flip display 180 degrees (if the screen is upside down)", "T", 2,
        flipHtml.c_str());
    wm.addParameter(&customFlip180);

    bool connected = wm.autoConnect(AP_NAME, AP_PASSWORD);
    if (!connected) {
        // No timeout is set (waits indefinitely in the portal), so in practice
        // this only happens on an unexpected WiFiManager failure -- restart
        // and let it try again from a clean state.
        ESP.restart();
    }

    // getValue() only reflects an actual form submission -- if the portal
    // never ran (autoConnect succeeded via saved creds), each parameter just
    // returns its raw constructor default, and for a checkbox that default is
    // always the literal "T" regardless of the checked/unchecked HTML shown.
    // Reading them outside an actual submission is meaningless, so the write
    // below is gated on s_portalShown.
    if (s_portalShown) {
        String newUrl = wifi_setup_build_url(String(customMwHost.getValue()),
                                             String(customUseHttps.getValue()) == "T");

        if (newUrl.length() > 0 && newUrl != storedUrl) {
            wifi_setup_store_middleware_url(newUrl);
            storedUrl = newUrl;
        }

        bool newFlip180 = String(customFlip180.getValue()) == "T";
        if (newFlip180 != storedFlip180) {
            DisplaySettings ds = getSettings();
            ds.flip180 = newFlip180;
            settingsSave(ds);
            // Orientation is a MADCTL write now, so it takes effect straight
            // away -- no restart needed.
            display_apply_settings();
        }
    }

    return storedUrl;
}

String wifi_setup_build_url(String hostPort, bool useHttps) {
    hostPort.trim();
    // Both entry points label this field "host:port", but a pasted address is
    // the obvious thing to try -- take the scheme off rather than concatenating
    // it into "http://http://host".
    if (hostPort.startsWith("https://")) {
        useHttps = true;
        hostPort = hostPort.substring(8);
    } else if (hostPort.startsWith("http://")) {
        useHttps = false;
        hostPort = hostPort.substring(7);
    }
    while (hostPort.endsWith("/")) {
        hostPort.remove(hostPort.length() - 1);
    }
    if (hostPort.length() == 0) return "";
    return String(useHttps ? "https://" : "http://") + hostPort;
}

void wifi_setup_store_middleware_url(const String &url) {
    Preferences prefs;
    prefs.begin("netcfg", false);
    prefs.putString("mw_url", url);
    prefs.end();
}

void wifi_setup_reset_and_restart() {
    WiFiManager wm;
    wm.resetSettings();

    Preferences prefs;
    prefs.begin("netcfg", false);
    prefs.remove("mw_url");
    prefs.end();
    // Display/panel corrections are deliberately NOT cleared here: they
    // describe the hardware, not the network, and losing them would leave an
    // affected board unreadable after what is meant to be a network reset.

    ESP.restart();
}

bool wifi_setup_take_short_press() {
    if (!s_shortPressPending) return false;
    s_shortPressPending = false;
    return true;
}

void wifi_setup_poll_connection() {
    static const uint32_t CHECK_INTERVAL_MS = 5000;
    static const uint32_t RETRY_INTERVAL_MS = 20000;
    static uint32_t lastCheck = 0;
    static uint32_t lastAttempt = 0;

    uint32_t now = millis();
    if (now - lastCheck < CHECK_INTERVAL_MS) return;
    lastCheck = now;

    if (WiFi.status() == WL_CONNECTED) return;

    // Spaced out deliberately: WiFi.reconnect() takes a while to resolve, and
    // retrying every few seconds just keeps the radio thrashing (and blocks
    // the loop) while an AP is still coming back up.
    if (lastAttempt != 0 && now - lastAttempt < RETRY_INTERVAL_MS) return;
    lastAttempt = now;

    Serial.println("Wi-Fi link lost -- attempting reconnect");
    WiFi.disconnect();
    WiFi.reconnect();  // uses the credentials WiFiManager already stored
}

void wifi_setup_poll_reset_button() {
    static bool pinConfigured = false;
    if (!pinConfigured) {
        pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
        pinConfigured = true;
    }

    const bool pressed = digitalRead(BOOT_BUTTON_PIN) == LOW;
    const unsigned long now = millis();

    if (!pressed) {
        if (s_buttonDownSince != 0) {
            const unsigned long held = now - s_buttonDownSince;
            if (held >= SHORT_PRESS_MIN_MS && held < SHORT_PRESS_MAX_MS) {
                s_shortPressPending = true;
            }
            s_buttonDownSince = 0;
        }
        return;
    }

    if (s_buttonDownSince == 0) {
        s_buttonDownSince = now;
        return;
    }

    if (now - s_buttonDownSince >= RESET_HOLD_MS) {
        Serial.println("BOOT button held 10s -- resetting Wi-Fi config");
        wifi_setup_reset_and_restart();
    }
}
