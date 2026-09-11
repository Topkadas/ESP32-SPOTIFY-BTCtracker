#include "DeckTouch.h"

#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "DeckConfig.h"
#include "DeckLang.h"
#include "DeckTft.h"

namespace {

// ---------------------------------------------------------------------
//  Two completely different touch wirings, depending on the board model:
//
//  2432S024R - the XPT2046 SHARES SPI with the display (CS 33). TFT_eSPI
//              drives it directly and takes care of interleaving the bus
//              transactions - it needs "#define TOUCH_CS 33" in
//              User_Setup.h.
//
//  2432S028R - the XPT2046 has its OWN bus (SCK 25, MOSI 32, MISO 39,
//              CS 33). The XPT2046_Touchscreen library drives that one,
//              and it has to run on a different SPI peripheral than the
//              display - otherwise the second spi.begin() reroutes the
//              MISO input through the GPIO matrix onto the display pins
//              and touch ends up reading someone else's pin.
// ---------------------------------------------------------------------
#if DECK_BOARD == 24
  #if !defined(TOUCH_CS)
    #error "For the 2432S024, copy User_Setup.h from the project root into TFT_eSPI (it needs TOUCH_CS 33)."
  #endif
#else
  #if defined(USE_HSPI_PORT)
SPIClass            touchSpi(VSPI);   // the display is on HSPI
  #else
SPIClass            touchSpi(HSPI);   // the display is on VSPI
  #endif
XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
#endif

Preferences         prefs;

// One way to read the raw values, whatever the wiring is.
// Returns true when a finger is on the display.
bool readRawTouch(int16_t& rx, int16_t& ry, int16_t& rz) {
#if DECK_BOARD == 24
  rz = (int16_t)tft.getTouchRawZ();
  if (rz < TOUCH_Z_MIN) return false;
  uint16_t x = 0, y = 0;
  tft.getTouchRaw(&x, &y);
  rx = (int16_t)x;
  ry = (int16_t)y;
  return true;
#else
  TS_Point p = ts.getPoint();
  rx = p.x; ry = p.y; rz = p.z;
  return rz >= TOUCH_Z_MIN;
#endif
}

// Calibration: two reference points. "swap" says whether the raw X axis
// matches X on the display, or whether the panel is turned 90 degrees.
struct Calib {
  bool    swap = false;
  int16_t rawX0 = TOUCH_RAW_X_MIN, rawX1 = TOUCH_RAW_X_MAX;
  int16_t rawY0 = TOUCH_RAW_Y_MIN, rawY1 = TOUCH_RAW_Y_MAX;
  bool    valid = false;
};

Calib    g_cal;
uint8_t  g_rotation = DEFAULT_ROTATION;
uint32_t g_lastActivity = 0;

// The points you tap during calibration.
constexpr int16_t CAL_INSET = 26;

// --- running gesture state ---------------------------------------------
bool     g_down       = false;
bool     g_dragging   = false;
bool     g_longFired  = false;
int16_t  g_startX = -1, g_startY = -1;
int16_t  g_lastX  = -1, g_lastY  = -1;
uint32_t g_downAt     = 0;
uint32_t g_lastSample = 0;
uint8_t  g_missCount  = 0;      // how many reads in a row saw no finger

int16_t mapAxis(int16_t raw, int16_t a0, int16_t a1, int16_t span) {
  if (a1 == a0) return 0;
  long v = (long)(raw - a0) * (span - 1) / (a1 - a0);
  return (int16_t)constrain(v, 0L, (long)span - 1);
}

// Turns a raw sample into pixels.
//
// A calibration is always taken at the current rotation and setRotation()
// throws it away, so on a calibrated board the rotation is already baked
// into it. Only the fallback (uncalibrated) values assume rotation 1 - at
// rotation 3 the display is turned 180 degrees, so both axes have to be
// flipped.
void rawToScreen(int16_t rx, int16_t ry, int16_t& sx, int16_t& sy) {
  int16_t ax = g_cal.swap ? ry : rx;
  int16_t ay = g_cal.swap ? rx : ry;

  sx = mapAxis(ax, g_cal.rawX0, g_cal.rawX1, SCREEN_W);
  sy = mapAxis(ay, g_cal.rawY0, g_cal.rawY1, SCREEN_H);

  if (!g_cal.valid && g_rotation == 3) {
    sx = SCREEN_W - 1 - sx;
    sy = SCREEN_H - 1 - sy;
  }
}

void loadCalib() {
  prefs.begin("deck", true);
  g_rotation = prefs.getUChar("rot", DEFAULT_ROTATION);
  if (g_rotation != 1 && g_rotation != 3) g_rotation = DEFAULT_ROTATION;

  if (prefs.isKey("cal_ok")) {
    g_cal.swap  = prefs.getBool("cal_sw", false);
    g_cal.rawX0 = prefs.getShort("cal_x0", TOUCH_RAW_X_MIN);
    g_cal.rawX1 = prefs.getShort("cal_x1", TOUCH_RAW_X_MAX);
    g_cal.rawY0 = prefs.getShort("cal_y0", TOUCH_RAW_Y_MIN);
    g_cal.rawY1 = prefs.getShort("cal_y1", TOUCH_RAW_Y_MAX);
    g_cal.valid = true;
  }
  prefs.end();

  if (!g_cal.valid) {
    // A fallback guess from community values - good enough to hit the
    // buttons and start a proper calibration.
    g_cal.swap  = false;
    g_cal.rawX0 = TOUCH_RAW_X_MIN;
    g_cal.rawX1 = TOUCH_RAW_X_MAX;
    g_cal.rawY0 = TOUCH_RAW_Y_MIN;
    g_cal.rawY1 = TOUCH_RAW_Y_MAX;
  }
}

void saveCalib() {
  prefs.begin("deck", false);
  prefs.putBool("cal_ok", true);
  prefs.putBool("cal_sw", g_cal.swap);
  prefs.putShort("cal_x0", g_cal.rawX0);
  prefs.putShort("cal_x1", g_cal.rawX1);
  prefs.putShort("cal_y0", g_cal.rawY0);
  prefs.putShort("cal_y1", g_cal.rawY1);
  prefs.end();
  g_cal.valid = true;
}

// Reads one settled touch: waits for the finger to land, collects
// samples and returns their median. Only used during calibration.
bool readSettled(int16_t& rx, int16_t& ry, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;

  int16_t px = 0, py = 0, pz = 0;

  while ((int32_t)(millis() - deadline) < 0) {
    if (!readRawTouch(px, py, pz)) { delay(10); continue; }

    delay(80);                                  // let the finger settle
    int16_t xs[9], ys[9];
    int n = 0;
    for (int i = 0; i < 9; i++) {
      if (readRawTouch(px, py, pz)) { xs[n] = px; ys[n] = py; n++; }
      delay(12);
    }
    if (n < 5) continue;

    for (int i = 1; i < n; i++) {               // insertion sort, n <= 9
      int16_t kx = xs[i], ky = ys[i];
      int j = i - 1;
      while (j >= 0 && xs[j] > kx) { xs[j + 1] = xs[j]; j--; }
      xs[j + 1] = kx;
      j = i - 1;
      while (j >= 0 && ys[j] > ky) { ys[j + 1] = ys[j]; j--; }
      ys[j + 1] = ky;
    }
    rx = xs[n / 2];
    ry = ys[n / 2];

    while (readRawTouch(px, py, pz)) delay(20);         // wait for the finger to lift
    delay(150);
    return true;
  }
  return false;
}

void drawTarget(int16_t x, int16_t y, uint16_t color) {
  tft.drawCircle(x, y, 12, color);
  tft.drawCircle(x, y, 4, color);
  tft.drawFastHLine(x - 18, y, 36, color);
  tft.drawFastVLine(x, y - 18, 36, color);
}

}  // namespace

// =====================================================================
void Touch::begin() {
  loadCalib();

#if DECK_BOARD == 24
  // Nothing to initialise - touch rides the display bus and TFT_eSPI,
  // which is already running, looks after it.
  LOGLN("[touch] XPT2046 on the display bus (CS 33)");
#else
  touchSpi.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  ts.begin(touchSpi);
  // Rotation is handled later in rawToScreen(), so a calibration holds
  // no matter how the display is turned. 1 = identity, no transform.
  ts.setRotation(1);
  #if defined(USE_HSPI_PORT)
  LOGLN("[touch] XPT2046 on its own VSPI bus");
  #else
  LOGLN("[touch] XPT2046 on its own HSPI bus");
  #endif
#endif

  g_lastActivity = millis();
  LOGF("[touch] calibration %s, rotation %u\n",
       g_cal.valid ? "from NVS" : "default", g_rotation);
}

TouchEvent Touch::poll() {
  TouchEvent ev;

  uint32_t now = millis();

  // A long press has to be evaluated outside the sampling window too. If
  // a blocking network request (easily a second) runs across it, the
  // threshold would otherwise pass unnoticed and the gesture would land
  // as a tap.
  if (g_down && !g_longFired && !g_dragging &&
      (now - g_downAt) > TOUCH_LONGPRESS_MS) {
    g_longFired  = true;
    ev.longPress = true;
    ev.down      = true;
    ev.x = g_lastX; ev.y = g_lastY;
    ev.startX = g_startX; ev.startY = g_startY;
    return ev;
  }

  if (now - g_lastSample < 12) {
    // Between samples we just repeat the current state, so the caller
    // does not have to care how often it calls poll().
    ev.down     = g_down;
    ev.dragging = g_dragging;
    ev.x = g_lastX; ev.y = g_lastY;
    ev.startX = g_startX; ev.startY = g_startY;
    return ev;
  }
  g_lastSample = now;

  int16_t rx = 0, ry = 0, rz = 0;
  bool touching = readRawTouch(rx, ry, rz);

  int16_t sx = g_lastX, sy = g_lastY;
  if (touching) rawToScreen(rx, ry, sx, sy);

  if (touching) {
    g_missCount = 0;
    if (!g_down) {
      g_down      = true;
      g_dragging  = false;
      g_longFired = false;
      g_downAt    = now;
      g_startX = sx; g_startY = sy;
      ev.pressed = true;
    } else {
      if (abs(sx - g_startX) > TOUCH_DRAG_PX || abs(sy - g_startY) > TOUCH_DRAG_PX)
        g_dragging = true;
      if (!g_longFired && !g_dragging && (now - g_downAt) > TOUCH_LONGPRESS_MS) {
        g_longFired  = true;
        ev.longPress = true;
      }
    }
    g_lastX = sx; g_lastY = sy;
    g_lastActivity = now;
  } else if (g_down) {
    // A resistive panel occasionally "drops" a sample - we count up to
    // three in a row before declaring that the finger has lifted.
    if (++g_missCount < 3) {
      ev.down     = true;
      ev.dragging = g_dragging;
      ev.x = g_lastX; ev.y = g_lastY;
      ev.startX = g_startX; ev.startY = g_startY;
      return ev;
    }
    g_down      = false;
    g_missCount = 0;
    ev.released = true;
    ev.tap      = !g_dragging && !g_longFired &&
                  (now - g_downAt) >= TOUCH_DEBOUNCE_MS;
    g_lastActivity = now;
  }

  ev.down     = g_down;
  ev.dragging = g_dragging;
  ev.x = g_lastX; ev.y = g_lastY;
  ev.startX = g_startX; ev.startY = g_startY;
  return ev;
}

bool Touch::calibrate() {
  const int16_t px[2] = {CAL_INSET, (int16_t)(SCREEN_W - CAL_INSET)};
  const int16_t py[2] = {CAL_INSET, (int16_t)(SCREEN_H - CAL_INSET)};
  int16_t rx[2], ry[2];

  tft.setSwapBytes(false);
  for (int i = 0; i < 2; i++) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(T(S_CAL_PROMPT),
                   SCREEN_W / 2, SCREEN_H / 2 - 12);
    tft.drawString(i == 0 ? "1 / 2" : "2 / 2", SCREEN_W / 2, SCREEN_H / 2 + 12);
    drawTarget(px[i], py[i], TFT_GREENYELLOW);

    if (!readSettled(rx[i], ry[i], 30000)) {
      LOGLN("[touch] calibration timed out");
      return false;
    }
    LOGF("[touch] point %d: raw %d,%d\n", i + 1, rx[i], ry[i]);
  }

  int dx = abs(rx[1] - rx[0]);
  int dy = abs(ry[1] - ry[0]);
  if (dx < 200 && dy < 200) {
    LOGLN("[touch] the points are too close, calibration rejected");
    return false;
  }

  // If Y changed more than X between the top-left and bottom-right
  // corners, the panel is turned 90 degrees relative to the display.
  g_cal.swap = (dy > dx);

  int16_t ax0 = g_cal.swap ? ry[0] : rx[0];
  int16_t ax1 = g_cal.swap ? ry[1] : rx[1];
  int16_t ay0 = g_cal.swap ? rx[0] : ry[0];
  int16_t ay1 = g_cal.swap ? rx[1] : ry[1];

  // From the two points, work out which raw values sit at the edges (0 and max).
  long spanX = (long)ax1 - ax0;
  long spanY = (long)ay1 - ay0;
  if (spanX == 0 || spanY == 0) return false;

  long pxSpan = px[1] - px[0];
  long pySpan = py[1] - py[0];

  g_cal.rawX0 = (int16_t)(ax0 - spanX * px[0] / pxSpan);
  g_cal.rawX1 = (int16_t)(ax0 + spanX * (SCREEN_W - 1 - px[0]) / pxSpan);
  g_cal.rawY0 = (int16_t)(ay0 - spanY * py[0] / pySpan);
  g_cal.rawY1 = (int16_t)(ay0 + spanY * (SCREEN_H - 1 - py[0]) / pySpan);

  saveCalib();
  LOGF("[touch] saved: swap=%d X %d..%d  Y %d..%d\n",
       (int)g_cal.swap, g_cal.rawX0, g_cal.rawX1, g_cal.rawY0, g_cal.rawY1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.drawString(T(S_CAL_SAVED), SCREEN_W / 2, SCREEN_H / 2);
  delay(900);
  return true;
}

bool Touch::hasCalibration() { return g_cal.valid; }

void Touch::forgetCalibration() {
  prefs.begin("deck", false);
  prefs.remove("cal_ok");
  prefs.end();
  g_cal.valid = false;
}

uint8_t Touch::rotation() { return g_rotation; }

void Touch::setRotation(uint8_t r) {
  if (r != 1 && r != 3) return;
  g_rotation = r;
  prefs.begin("deck", false);
  prefs.putUChar("rot", r);
  prefs.end();
  // Rotating changes what a calibration means - the old one is void.
  forgetCalibration();
}

uint32_t Touch::lastActivityMs() { return g_lastActivity; }
