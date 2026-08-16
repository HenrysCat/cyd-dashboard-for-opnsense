#pragma once
#include <Arduino.h>

// Connects to Wi-Fi, using WiFiManager's captive portal to collect SSID/
// password + the middleware base URL on first boot (or whenever there's no
// saved Wi-Fi connection). Blocks until connected. Returns the configured
// middleware base URL (persisted in NVS across reboots).
String wifi_setup_begin();

// Splits a full base URL back into its "host:port" part and a scheme flag,
// for pre-filling a form field with what is currently stored.
void wifi_setup_split_url(const String &url, String &hostPort, bool &isHttps);

// Builds a full base URL from a "host:port" form field plus an HTTPS flag.
// Tolerates a complete URL pasted into a field that only asks for host:port,
// in which case the scheme in the text wins over the flag. Returns an empty
// string if nothing usable was typed.
String wifi_setup_build_url(String hostPort, bool useHttps);

// Persists the middleware base URL to NVS on its own, leaving Wi-Fi
// credentials alone -- so the address can be corrected from the settings UI
// without a trip through the captive portal.
void wifi_setup_store_middleware_url(const String &url);

// Wipes saved Wi-Fi credentials + middleware URL and restarts into the
// captive portal again.
void wifi_setup_reset_and_restart();

// Call every loop() iteration. Watches the board's physical BOOT button
// (GPIO0) and triggers wifi_setup_reset_and_restart() if it's held down
// continuously for 10 seconds.
void wifi_setup_poll_reset_button();

// Consumes a pending short press (press and release inside ~1s) of the BOOT
// button, returning true once per press. The 10s hold remains a factory reset.
bool wifi_setup_take_short_press();

// Call every loop() iteration. Re-establishes Wi-Fi if the link drops, so an
// AP reboot or a brief outage doesn't leave the dashboard dead until someone
// power-cycles it. Rate-limited internally.
void wifi_setup_poll_connection();
