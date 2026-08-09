#pragma once
#include <Arduino.h>
#include <cstdint>

static const uint16_t SCREEN_WIDTH = 320;
static const uint16_t SCREEN_HEIGHT = 240;

// TFT init, LVGL init, and display driver registration -- no touch. Call
// once from setup(), before wifi_setup_begin(), so the captive-portal
// message screen can be drawn even if touch is unusable.
void display_init_panel();

// Touch controller SPI/pin init. No calibration step: fixed panel bounds are
// used (see display_driver.cpp). Call once from setup().
void display_init_touch();

// Re-applies the current DisplaySettings (orientation, colour order, colour
// inversion, backlight brightness) to the panel. Takes effect immediately --
// no restart needed. Call after settingsSave().
void display_apply_settings();

// Reads a screen-space touch point, already mapped to the display's
// orientation. Returns false when the panel isn't being pressed.
bool display_read_touch(uint16_t *x, uint16_t *y);

// Pumps LVGL's timer handler. Call every loop() iteration.
void display_loop();
