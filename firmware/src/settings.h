#pragma once
#include <Arduino.h>

// Panel-variant corrections plus user preferences. CYD boards ship with
// several different panel/touch wirings, so these exist to correct a board
// that comes up wrong -- the defaults below are what's correct on the unit
// this was developed against, and every toggle is a deviation from that.
struct DisplaySettings {
    uint8_t brightness = 100;    // percent, clamped to 5..100 when applied
    bool swapRedBlue = false;    // red shows blue / yellow shows cyan
    bool rotate90 = false;       // screen comes up portrait and cropped
    bool flip180 = false;        // screen upside down
    bool mirror = false;         // text/images left-right reversed
    bool invertColours = false;  // colours appear as their negative
    bool swapNav = false;        // left/right taps page the wrong way

    // Unattended page cycling, for wall-mounted use.
    bool autoCycle = false;
    uint16_t autoCycleSeconds = 30;
};

// Bounds applied wherever autoCycleSeconds is accepted.
static const uint16_t AUTO_CYCLE_MIN_S = 5;
static const uint16_t AUTO_CYCLE_MAX_S = 600;

const DisplaySettings &getSettings();

// Loads from NVS into the in-memory copy. Call once, before display init.
void settingsLoad();

// Persists to NVS and updates the in-memory copy. Does not itself re-apply
// them to the panel -- callers use display_apply_settings() for that.
void settingsSave(const DisplaySettings &s);
