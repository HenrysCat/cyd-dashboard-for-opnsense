#include "display_driver.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "settings.h"

// On the CYD the XPT2046 touch controller is on its own dedicated pins, NOT
// shared with the display's SPI bus. The display runs on HSPI (12/13/14/15,
// see platformio.ini), so touch gets its own VSPI instance here -- sharing a
// bus, or calling the global SPI.begin() (which would reconfigure VSPI to its
// default pins), corrupts display output.
static const uint8_t TOUCH_SCLK_PIN = 25;
static const uint8_t TOUCH_MOSI_PIN = 32;
static const uint8_t TOUCH_MISO_PIN = 39;
static const uint8_t TOUCH_CS_PIN = 33;
static const uint8_t TOUCH_IRQ_PIN = 36;

static const uint32_t TOUCH_FREQUENCY = 2500000;

// Fixed panel bounds rather than a per-unit crosshair calibration. These are
// the values used by other working projects on this same hardware; resistive
// CYD panels are consistent enough that a calibration step mostly adds a
// failure mode (an unusable calibration screen locks you out of the device)
// without improving accuracy for coarse left/right tap zones.
static const uint16_t TOUCH_MIN = 120;
static const uint16_t TOUCH_MAX = 3975;

static SPIClass touchSpi(VSPI);

static TFT_eSPI tft = TFT_eSPI();

// ILI9341 MADCTL (0x36) bits.
static const uint8_t MADCTL_REG = 0x36;
static const uint8_t MADCTL_MY = 0x80;   // row address order
static const uint8_t MADCTL_MX = 0x40;   // column address order
static const uint8_t MADCTL_MV = 0x20;   // row/column exchange
static const uint8_t MADCTL_BGR = 0x08;  // colour order

// What TFT_eSPI itself writes for rotation 3 (MX|MY|MV|BGR) -- verified
// correct on this board. Rather than calling setRotation() and losing our
// other corrections, the MADCTL byte is composed here so every toggle is an
// XOR against this known-good baseline.
static const uint8_t MADCTL_BASE = MADCTL_MX | MADCTL_MY | MADCTL_MV;

// Backlight PWM. TFT_BL is also handed to TFT_eSPI (which drives it on/off at
// init); attaching LEDC afterwards takes over for dimming.
static const uint8_t BACKLIGHT_PIN = 21;
static const uint8_t BACKLIGHT_CHANNEL = 0;

static const size_t DRAW_BUF_LINES = 40;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_buf[SCREEN_WIDTH * DRAW_BUF_LINES];

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

void display_apply_settings() {
    const DisplaySettings &s = getSettings();

    uint8_t madctl = MADCTL_BASE;
    if (s.rotate90) madctl ^= MADCTL_MV;
    if (s.flip180) madctl ^= (MADCTL_MX | MADCTL_MY);
    // A mirror is a single-axis flip, so it toggles whichever bit currently
    // drives the screen's horizontal axis. MV (set in the baseline) swaps the
    // meaning of the row/column bits, so rotate90 decides which one that is.
    if (s.mirror) madctl ^= (s.rotate90 ? MADCTL_MX : MADCTL_MY);
    if (!s.swapRedBlue) madctl |= MADCTL_BGR;

    tft.startWrite();
    tft.writecommand(MADCTL_REG);
    tft.writedata(madctl);
    tft.endWrite();

    // TFT_INVERSION_ON is set at build time because this panel needs it, so
    // the *default* state is inverted. This toggle flips away from that.
    tft.invertDisplay(!s.invertColours);

    uint8_t pct = constrain(s.brightness, (uint8_t)5, (uint8_t)100);
    ledcWrite(BACKLIGHT_CHANNEL, map(pct, 0, 100, 0, 255));

    // Changing MADCTL re-scans the pixels already in the panel's memory under
    // the new orientation, so whatever was on screen reappears rearranged.
    // LVGL has no idea any of that happened and would only repaint regions it
    // considers dirty, leaving those remnants behind -- so clear the panel and
    // force a full repaint. Guarded because this also runs from
    // display_init_panel() before lv_init(), when there's no display yet.
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) {
        tft.fillScreen(TFT_BLACK);
        lv_obj_t *scr = lv_disp_get_scr_act(disp);
        if (scr) lv_obj_invalidate(scr);
        lv_obj_invalidate(lv_layer_top());
    }
}

void display_init_panel() {
    tft.init();
    // Establishes TFT_eSPI's internal 320x240 landscape dimensions. The MADCTL
    // byte is then overwritten by display_apply_settings() -- the logical
    // resolution stays 320x240 regardless of which corrections are enabled.
    tft.setRotation(3);

    ledcSetup(BACKLIGHT_CHANNEL, 5000, 8);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);

    display_apply_settings();
    tft.fillScreen(TFT_BLACK);

    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf, NULL, SCREEN_WIDTH * DRAW_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void display_init_touch() {
    pinMode(TOUCH_CS_PIN, OUTPUT);
    digitalWrite(TOUCH_CS_PIN, HIGH);
    pinMode(TOUCH_IRQ_PIN, INPUT);
    touchSpi.begin(TOUCH_SCLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
}

static uint16_t readTouchAxis(uint8_t command) {
    touchSpi.transfer(command);
    const uint16_t high = touchSpi.transfer(0x00);
    const uint16_t low = touchSpi.transfer(0x00);
    return ((high << 8) | low) >> 3;
}

static bool readRawTouch(uint16_t &rawX, uint16_t &rawY) {
    // IRQ is pulled low only while the panel is actually being pressed.
    if (digitalRead(TOUCH_IRQ_PIN) == HIGH) return false;

    touchSpi.beginTransaction(SPISettings(TOUCH_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(TOUCH_CS_PIN, LOW);
    delayMicroseconds(2);

    uint32_t xTotal = 0, yTotal = 0;
    const uint8_t samples = 4;  // averaged: single reads off a resistive panel are noisy
    for (uint8_t i = 0; i < samples; i++) {
        xTotal += readTouchAxis(0xD0);
        yTotal += readTouchAxis(0x90);
    }

    digitalWrite(TOUCH_CS_PIN, HIGH);
    touchSpi.endTransaction();

    rawX = xTotal / samples;
    rawY = yTotal / samples;
    return rawX >= TOUCH_MIN && rawX <= TOUCH_MAX && rawY >= TOUCH_MIN && rawY <= TOUCH_MAX;
}

static int16_t scaleTouch(uint16_t value, int16_t size) {
    value = constrain(value, TOUCH_MIN, TOUCH_MAX);
    return (int16_t)(((uint32_t)(value - TOUCH_MIN) * (size - 1)) / (TOUCH_MAX - TOUCH_MIN));
}

bool display_read_touch(uint16_t *x, uint16_t *y) {
    uint16_t rawX, rawY;
    if (!readRawTouch(rawX, rawY)) return false;

    // The touch panel is wired independently of the display, so its axes do
    // not follow the display's rotation -- map them explicitly here. This
    // base mapping corresponds to the un-flipped (rotation 3) orientation.
    int16_t baseX = scaleTouch(rawX, SCREEN_WIDTH);
    int16_t baseY = scaleTouch(rawY, SCREEN_HEIGHT);

    const DisplaySettings &s = getSettings();

    int16_t mappedX, mappedY;
    if (s.rotate90) {
        mappedX = (int16_t)((int32_t)baseX * SCREEN_WIDTH / SCREEN_WIDTH);
        mappedY = (int16_t)((int32_t)baseY * SCREEN_HEIGHT / SCREEN_HEIGHT);
    } else {
        mappedX = SCREEN_WIDTH - 1 - (int16_t)((int32_t)baseY * SCREEN_WIDTH / SCREEN_HEIGHT);
        mappedY = (int16_t)((int32_t)baseX * SCREEN_HEIGHT / SCREEN_WIDTH);
    }

    // Changing MADCTL re-orients the display but not the touch panel, so the
    // same corrections have to be mirrored here to keep taps landing where
    // they look.
    if (s.flip180) {
        mappedX = SCREEN_WIDTH - 1 - mappedX;
        mappedY = SCREEN_HEIGHT - 1 - mappedY;
    }
    if (s.mirror) {
        mappedX = SCREEN_WIDTH - 1 - mappedX;
    }

    *x = (uint16_t)constrain(mappedX, 0, SCREEN_WIDTH - 1);
    *y = (uint16_t)constrain(mappedY, 0, SCREEN_HEIGHT - 1);
    return true;
}

void display_loop() {
    lv_timer_handler();
}
