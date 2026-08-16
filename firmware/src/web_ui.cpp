#include "web_ui.h"

#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "api_client.h"
#include "data_model.h"
#include "display_driver.h"
#include "settings.h"
#include "wifi_setup.h"

static WebServer server(80);
static const char *MDNS_NAME = "cyd-dash";

// Built as a String rather than served from PROGMEM so the current values can
// be substituted in -- the form always reflects what's actually stored.
static String renderPage(bool saved) {
    const DisplaySettings &s = getSettings();

    String h;
    h.reserve(7500);
    h += F("<!doctype html><html><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>OPNsense Dashboard settings</title><style>"
           "body{background:#111;color:#eee;font-family:system-ui,sans-serif;margin:0;"
           "padding:16px;line-height:1.45}"
           ".w{max-width:640px;margin:0 auto}"
           "h1{font-size:1.3rem;margin:0 0 4px}"
           ".sub{color:#999;font-size:.85rem;margin-bottom:20px}"
           "fieldset{border:1px solid #333;border-radius:8px;margin:0 0 18px;padding:14px}"
           "legend{color:#ff9800;padding:0 6px;font-weight:600}"
           ".row{padding:10px 0;border-bottom:1px solid #222}"
           ".row:last-child{border-bottom:0}"
           "label.t{display:flex;align-items:center;gap:10px;font-weight:600;cursor:pointer}"
           "input[type=checkbox]{width:20px;height:20px;accent-color:#ff9800;flex:none}"
           ".hint{color:#999;font-size:.85rem;margin:4px 0 0 30px}"
           "input[type=range]{width:100%;accent-color:#ff9800}"
           "input[type=text]{width:100%;box-sizing:border-box;padding:8px;background:#222;"
           "color:#eee;border:1px solid #444;border-radius:6px;font-size:1rem}"
           ".val{color:#ff9800;font-weight:600}"
           "button{background:#ff9800;color:#111;border:0;border-radius:8px;padding:12px 20px;"
           "font-size:1rem;font-weight:600;width:100%;cursor:pointer}"
           ".ok{background:#1b5e20;border:1px solid #2e7d32;padding:10px 12px;border-radius:8px;"
           "margin-bottom:16px}"
           ".meta{color:#777;font-size:.8rem;margin-top:18px;text-align:center}"
           "</style></head><body><div class='w'>");

    h += F("<h1>OPNsense Dashboard</h1><div class='sub'>Display settings</div>");
    if (saved) h += F("<div class='ok'>Settings saved and applied.</div>");

    h += F("<form method='POST' action='/save'><fieldset><legend>Display</legend>");

    h += F("<div class='row'><label class='t' for='b'>Backlight brightness</label>"
           "<input type='range' id='b' name='brightness' min='5' max='100' value='");
    h += String(s.brightness);
    h += F("' oninput=\"document.getElementById('bv').textContent=this.value+'%'\">"
           "<div class='hint'>Current: <span class='val' id='bv'>");
    h += String(s.brightness);
    h += F("%</span></div></div>");

    struct Toggle {
        const char *name;
        const char *title;
        const char *hint;
        bool value;
    };
    const Toggle toggles[] = {
        {"swaprb", "Swap red/blue display channels",
         "Enable this only when red appears blue and yellow appears cyan. It is applied "
         "immediately and saved for this board.",
         s.swapRedBlue},
        {"rot90", "Rotate display 90&deg;",
         "Enable this if the screen shows portrait and cropped on first start.", s.rotate90},
        {"flip180", "Flip display 180&deg;", "Enable this if the screen is upside down.",
         s.flip180},
        {"mirror", "Mirror display",
         "Enable this if the screen shows text and images left-right reversed, as on some CYD "
         "panel variants.",
         s.mirror},
        {"invert", "Invert display colours",
         "Enable this if colours appear as their negative/inverse, as on some CYD panel "
         "variants.",
         s.invertColours},
        {"swapnav", "Swap left/right page navigation",
         "Enable this if tapping the left/right edge of the screen changes pages in the wrong "
         "direction, as on some touch controller variants.",
         s.swapNav},
    };

    for (const Toggle &t : toggles) {
        h += F("<div class='row'><label class='t'><input type='checkbox' name='");
        h += t.name;
        h += F("'");
        if (t.value) h += F(" checked");
        h += F("><span>");
        h += t.title;
        h += F("</span></label><div class='hint'>");
        h += t.hint;
        h += F("</div></div>");
    }

    h += F("</fieldset><fieldset><legend>Pages</legend>");

    h += F("<div class='row'><label class='t'><input type='checkbox' name='autocycle'");
    if (s.autoCycle) h += F(" checked");
    h += F("><span>Cycle pages automatically</span></label>"
           "<div class='hint'>Advances to the next page on a timer, for unattended wall use. "
           "Tapping the screen still works and restarts the timer.</div></div>");

    h += F("<div class='row'><label class='t' for='cs'>Seconds per page</label>"
           "<div class='hint' style='margin:8px 0 0 30px'>"
           "<input type='number' id='cs' name='cyclesecs' min='5' max='600' value='");
    h += String(s.autoCycleSeconds);
    h += F("' style='width:90px;padding:6px;background:#222;color:#eee;border:1px solid #444;"
           "border-radius:6px'> seconds (5-600)</div></div>");

    h += F("</fieldset><fieldset><legend>Middleware</legend>");

    String hostPort;
    bool isHttps = false;
    wifi_setup_split_url(g_apiClient.baseUrl(), hostPort, isHttps);

    h += F("<div class='row'><label class='t' for='mw'>Address</label>"
           "<div class='hint' style='margin:8px 0 0 0'>"
           "<input type='text' id='mw' name='mwhost' placeholder='192.168.1.50:8098' value='");
    h += hostPort;
    h += F("'></div>"
           "<div class='hint' style='margin-left:0'>Host and port of the middleware container. "
           "Applies straight away -- no reboot, and Wi-Fi credentials are left alone. "
           "Leave blank to keep the current address.</div></div>");

    h += F("<div class='row'><label class='t'><input type='checkbox' name='mwhttps'");
    if (isHttps) h += F(" checked");
    h += F("><span>Use HTTPS</span></label>"
           "<div class='hint'>Only if you have put TLS in front of the middleware yourself. "
           "It serves plain HTTP out of the box.</div></div>");

    h += F("</fieldset><button type='submit'>Save &amp; apply</button></form>");

    // Separate form: a file upload can't share the settings POST, and keeping
    // it distinct makes it harder to trigger a firmware flash by accident.
    h += F("<fieldset style='margin-top:18px'><legend>Firmware</legend>"
           "<div class='row'><div class='hint' style='margin-left:0'>Upload a new "
           "<code>firmware.bin</code> to update over Wi-Fi. The device reboots when it "
           "finishes. Do not power it off during the upload.</div>"
           "<form method='POST' action='/update' enctype='multipart/form-data' "
           "style='margin-top:10px' onsubmit=\"this.querySelector('button').textContent="
           "'Uploading...'\">"
           "<input type='file' name='firmware' accept='.bin' required "
           "style='color:#eee;margin-bottom:10px'>"
           "<button type='submit'>Upload firmware</button></form></div></fieldset>");

    h += F("<div class='meta'>");
    h += g_dashboard.hostname.length() ? g_dashboard.hostname : String("OPNsense");
    h += F(" &middot; ");
    h += WiFi.localIP().toString();
    h += F("</div></div></body></html>");
    return h;
}

static void handleRoot() {
    server.send(200, "text/html", renderPage(false));
}

static void handleSave() {
    DisplaySettings s = getSettings();

    if (server.hasArg("brightness")) {
        s.brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 5, 100);
    }
    // An unchecked checkbox is simply absent from the POST body, so presence
    // is the value -- don't try to read it as "false".
    s.swapRedBlue = server.hasArg("swaprb");
    s.rotate90 = server.hasArg("rot90");
    s.flip180 = server.hasArg("flip180");
    s.mirror = server.hasArg("mirror");
    s.invertColours = server.hasArg("invert");
    s.swapNav = server.hasArg("swapnav");
    s.autoCycle = server.hasArg("autocycle");
    if (server.hasArg("cyclesecs")) {
        s.autoCycleSeconds = (uint16_t)constrain(server.arg("cyclesecs").toInt(),
                                                 (long)AUTO_CYCLE_MIN_S, (long)AUTO_CYCLE_MAX_S);
    }

    // Blank means "leave it as it is": the device is useless without a
    // reachable middleware, so an empty field must not wipe a working address.
    if (server.hasArg("mwhost")) {
        String url = wifi_setup_build_url(server.arg("mwhost"), server.hasArg("mwhttps"));
        if (url.length() > 0 && url != g_apiClient.baseUrl()) {
            wifi_setup_store_middleware_url(url);
            g_apiClient.setBaseUrl(url);
        }
    }

    settingsSave(s);
    display_apply_settings();

    server.send(200, "text/html", renderPage(true));
}

// Streams the uploaded image straight into the OTA partition -- it is far too
// large to buffer in RAM. Update.write() is fed chunk by chunk as they arrive.
static void handleUpdateUpload() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.print("OTA: starting ");
        Serial.println(upload.filename);
        // UPDATE_SIZE_UNKNOWN: the browser doesn't tell us the length up front,
        // so let the Update library size it against the free OTA partition.
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.print("OTA: success, bytes: ");
            Serial.println((unsigned)upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

static void handleUpdateDone() {
    const bool ok = !Update.hasError();
    String body = F("<!doctype html><html><head><meta charset='utf-8'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<style>body{background:#111;color:#eee;font-family:system-ui,sans-serif;"
                    "padding:24px;text-align:center}a{color:#ff9800}</style></head><body>");
    body += ok ? F("<h2>Update successful</h2><p>Rebooting...</p>")
               : F("<h2>Update failed</h2><p>The device kept its existing firmware.</p>");
    body += F("<p><a href='/'>Back to settings</a></p></body></html>");
    server.send(200, "text/html", body);

    if (ok) {
        delay(500);  // let the response actually reach the browser first
        ESP.restart();
    }
}

void web_ui_begin() {
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
    }
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();

    Serial.printf("Settings UI: http://%s/ (or http://%s.local/)\n",
                  WiFi.localIP().toString().c_str(), MDNS_NAME);
}

void web_ui_loop() {
    server.handleClient();
}
