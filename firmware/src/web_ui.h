#pragma once

// Starts the on-device settings web server (port 80) and advertises it over
// mDNS. Call once from setup(), after Wi-Fi is connected.
void web_ui_begin();

// Services pending HTTP requests. Call every loop() iteration.
void web_ui_loop();
