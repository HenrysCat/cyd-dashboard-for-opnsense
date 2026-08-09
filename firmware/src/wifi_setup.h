#pragma once
#include <Arduino.h>

// Connects to Wi-Fi, using WiFiManager's captive portal to collect SSID/
// password + the middleware base URL on first boot (or whenever there's no
// saved Wi-Fi connection). Blocks until connected. Returns the configured
// middleware base URL (persisted in NVS across reboots).
String wifi_setup_begin();

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
