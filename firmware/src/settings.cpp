#include "settings.h"

#include <Preferences.h>

static const char *NS = "display";

static DisplaySettings g_settings;

const DisplaySettings &getSettings() {
    return g_settings;
}

void settingsLoad() {
    Preferences p;
    p.begin(NS, true);
    g_settings.brightness = p.getUChar("bright", 100);
    g_settings.swapRedBlue = p.getBool("swaprb", false);
    g_settings.rotate90 = p.getBool("rot90", false);
    g_settings.flip180 = p.getBool("flip180", false);
    g_settings.mirror = p.getBool("mirror", false);
    g_settings.invertColours = p.getBool("invert", false);
    g_settings.swapNav = p.getBool("swapnav", false);
    g_settings.autoCycle = p.getBool("autocyc", false);
    g_settings.autoCycleSeconds = p.getUShort("autocycs", 30);
    p.end();
}

void settingsSave(const DisplaySettings &s) {
    g_settings = s;

    Preferences p;
    p.begin(NS, false);
    p.putUChar("bright", s.brightness);
    p.putBool("swaprb", s.swapRedBlue);
    p.putBool("rot90", s.rotate90);
    p.putBool("flip180", s.flip180);
    p.putBool("mirror", s.mirror);
    p.putBool("invert", s.invertColours);
    p.putBool("swapnav", s.swapNav);
    p.putBool("autocyc", s.autoCycle);
    p.putUShort("autocycs", s.autoCycleSeconds);
    p.end();
}
