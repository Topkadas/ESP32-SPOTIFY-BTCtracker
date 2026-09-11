#include "DeckUi.h"

#include <WiFi.h>

#include "DeckArt.h"
#include "DeckDraw.h"
#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckIcons.h"
#include "DeckNet.h"
#include "DeckTft.h"
#include "DeckTouch.h"

// Jedina instance displeje pro cely projekt.
TFT_eSPI tft = TFT_eSPI();

namespace {

// Kreslici naradi (sprite, paleta, sklenene panely) je sdilene se
// vsemi strankami - viz DeckDraw. Tady jsou jen kratke aliasy, aby
// zbytek souboru zustal citelny.
using Draw::cAccent;
using Draw::cPanel;
using Draw::cPanelHi;
using Draw::cText;
using Draw::cText2;
using Draw::cText3;

inline uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  return Draw::blend(a, b, t);
}
inline void refreshPalette() { Draw::refreshPalette(); }

inline void textLine(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* s, const GFXfont* f, uint8_t builtin,
                     uint16_t color, int16_t offset = 0,
                     uint8_t datum = ML_DATUM) {
  Draw::text(x, y, w, h, s, f, builtin, color, offset, datum);
}
inline int16_t measure(const char* s, const GFXfont* f, uint8_t builtin) {
  return Draw::measure(s, f, builtin);
}
inline void glassPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t r, uint8_t darken) {
  Draw::glass(x, y, w, h, r, darken);
}

// --- co uz je vykreslene (aby se neprekreslovalo zbytecne) ------------
char       lastTitle[160]  = {0};
char       lastArtist[160] = {0};
char       lastAlbum[96]   = {0};
char       lastDevice[64]  = {0};
char       lastDeviceLine[64] = {0};
char       lastClock[8]    = {0};
int        lastVolume      = -2;
int32_t    lastProgSec     = -1;
int32_t    lastDurSec      = -1;
int16_t    lastFillW       = -1;
bool       lastPlaying     = false;
bool       lastShuffle     = false;
RepeatMode lastRepeat      = RepeatMode::Off;
uint8_t    lastBars        = 255;

// --- bezici nazev ----------------------------------------------------
int16_t  titleW    = 0;
int16_t  titleOff  = 0;
int8_t   titleDir  = 1;
uint32_t titleNext = 0;

// --- toast ve stavovem radku -----------------------------------------
char     toastText[40] = {0};
uint32_t toastUntil    = 0;

// --- tecky stranek ve stavovem radku ----------------------------------
uint8_t pageCount  = 1;
uint8_t pageActive = 0;

// --- podsviceni ------------------------------------------------------
uint16_t blDuty   = BL_MAX;
uint16_t blTarget = BL_MAX;
bool     blReady  = false;
bool     ledOk    = false;
bool     blDipped = false;

void formatTime(uint32_t ms, char* out, size_t cap) {
  uint32_t total = ms / 1000;
  uint32_t m = total / 60;
  uint32_t s = total % 60;
  if (m >= 60) snprintf(out, cap, "%lu:%02lu:%02lu",
                        (unsigned long)(m / 60), (unsigned long)(m % 60),
                        (unsigned long)s);
  else         snprintf(out, cap, "%lu:%02lu", (unsigned long)m,
                        (unsigned long)s);
}

// =====================================================================
//  Casti obrazovky
// =====================================================================
void drawButton(uint8_t index, const PlayerState& st, bool pressed) {
  int16_t cx = CTRL_CX(index);
  int16_t bw = CTRL_W / CTRL_COUNT - 4;

  // plocha tlacitka zpatky na "sklo"
  glassPanel(cx - bw / 2, CTRL_Y + 1, bw, CTRL_H - 2, 0, 108);

  uint16_t bg = cPanel;
  if (pressed) {
    tft.fillSmoothCircle(cx, CTRL_CY, 18, cPanelHi, cPanel);
    bg = cPanelHi;
  }

  switch (index) {
    case 0: {
      uint16_t c = st.shuffle ? cAccent : cText2;
      Icons::shuffle(cx, CTRL_CY, c, bg);
      if (st.shuffle) Icons::activeDot(cx, CTRL_Y + CTRL_H - 5, cAccent, bg);
      break;
    }
    case 1:
      Icons::prev(cx, CTRL_CY, st.canPrev ? cText : cText3, bg);
      break;
    case 2: {
      // Jedine tlacitko, ktere ma vlastni plochu - je to kotva cele
      // obrazovky a na prvni pohled rekne, jestli hudba bezi.
      uint16_t disc = st.isPlaying ? cAccent : blend565(cPanel, cText, 44);
      tft.fillSmoothCircle(cx, CTRL_CY, 16, disc, bg);
      // Na svetlem akcentu (napr. brat-zelena) musi byt ikona tmava.
      uint16_t ink = (Col::luma(Col::to888(disc)) > 150)
                         ? Col::to565(Col::rgb(0x0A, 0x0D, 0x12))
                         : cText;
      if (st.isPlaying) Icons::pause(cx, CTRL_CY, ink, disc);
      else              Icons::play(cx, CTRL_CY, ink, disc);
      break;
    }
    case 3:
      Icons::next(cx, CTRL_CY, st.canNext ? cText : cText3, bg);
      break;
    case 4: {
      bool on = (st.repeat != RepeatMode::Off);
      uint16_t c = on ? cAccent : cText2;
      Icons::repeat(cx, CTRL_CY, c, bg, st.repeat == RepeatMode::Track);
      if (on) Icons::activeDot(cx, CTRL_Y + CTRL_H - 5, cAccent, bg);
      break;
    }
  }
}

void drawControlBar(const PlayerState& st) {
  glassPanel(CTRL_X, CTRL_Y, CTRL_W, CTRL_H, CTRL_R, 108);
  for (uint8_t i = 0; i < CTRL_COUNT; i++) drawButton(i, st, false);
}

void drawVolume(int volume, bool supported) {
  Art::paintRect(VOL_X, VOL_Y - 2, VOL_W, VOL_H + 4);
  uint16_t bg = Art::pixelAt(VOL_X + VOL_W / 2, VOL_Y + VOL_H / 2);

  if (!supported || volume < 0) {
    Icons::volume(VOL_X + 12, VOL_Y + VOL_H / 2, cText3, bg, 1);
    textLine(VOL_BAR_X, VOL_Y + 2, VOL_BAR_W, 18, "nelze menit",
             nullptr, 2, cText3);
    return;
  }

  uint8_t level = (volume == 0) ? 0 : (volume < 55 ? 1 : 2);
  Icons::volume(VOL_X + 12, VOL_Y + VOL_H / 2, cText2, bg, level);

  int16_t by = VOL_Y + VOL_H / 2 - 2;
  uint16_t track = blend565(bg, cText, 58);
  tft.fillSmoothRoundRect(VOL_BAR_X, by, VOL_BAR_W, 4, 2, track, bg);

  int16_t fw = (int16_t)((int32_t)VOL_BAR_W * volume / 100);
  if (fw > 3) tft.fillSmoothRoundRect(VOL_BAR_X, by, fw, 4, 2, cText, track);
  tft.fillSmoothCircle(VOL_BAR_X + fw, by + 2, 4, cText, bg);
}

void drawProgressBar(uint32_t progressMs, uint32_t durationMs) {
  int16_t fillW = 0;
  if (durationMs > 0) {
    uint32_t p = min(progressMs, durationMs);
    fillW = (int16_t)((uint64_t)PROG_BAR_W * p / durationMs);
  }
  fillW = constrain(fillW, (int16_t)0, (int16_t)PROG_BAR_W);
  if (fillW == lastFillW) return;
  lastFillW = fillW;

  Art::paintRect(PROG_BAR_X - 6, PROG_BAR_Y - 6, PROG_BAR_W + 12,
                 PROG_BAR_H + 12);
  uint16_t bg = Art::pixelAt(PROG_BAR_X + PROG_BAR_W / 2, PROG_BAR_Y + 2);

  uint16_t track = blend565(bg, cText, 58);
  tft.fillSmoothRoundRect(PROG_BAR_X, PROG_BAR_Y, PROG_BAR_W, PROG_BAR_H,
                          PROG_BAR_H / 2, track, bg);
  if (fillW >= PROG_BAR_H)
    tft.fillSmoothRoundRect(PROG_BAR_X, PROG_BAR_Y, fillW, PROG_BAR_H,
                            PROG_BAR_H / 2, cAccent, track);
  tft.fillSmoothCircle(PROG_BAR_X + fillW, PROG_BAR_Y + PROG_BAR_H / 2, 5,
                       cText, bg);
}

}  // namespace

// =====================================================================
//  Verejne API
// =====================================================================
void Ui::begin() {
  tft.init();
  tft.setRotation(Touch::rotation());
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  // LEDC az PO tft.init() - init() umi sahnout na TFT_BL a prepsal by to.
  blReady = ledcAttach(PIN_BACKLIGHT, BL_PWM_FREQ, BL_PWM_BITS);
  if (blReady) ledcWrite(PIN_BACKLIGHT, BL_MAX);
  else { pinMode(PIN_BACKLIGHT, OUTPUT); digitalWrite(PIN_BACKLIGHT, HIGH); }

  Draw::begin();

#if FEAT_RGB_LED
  // RGB LED na desce je ACTIVE LOW - strida se tedy obraci.
  ledOk = ledcAttach(PIN_LED_R, LED_PWM_FREQ, LED_PWM_BITS) &&
          ledcAttach(PIN_LED_G, LED_PWM_FREQ, LED_PWM_BITS) &&
          ledcAttach(PIN_LED_B, LED_PWM_FREQ, LED_PWM_BITS);
  if (ledOk) {
    ledcWrite(PIN_LED_R, 255);
    ledcWrite(PIN_LED_G, 255);
    ledcWrite(PIN_LED_B, 255);
  } else {
    pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);
    pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, HIGH);
    pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);
  }
#endif

  refreshPalette();
}

void Ui::splash() {
  Art::unload();
  refreshPalette();
  Art::paintBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setTextColor(cText);
  tft.drawString("SpotifyDeck", SCREEN_W / 2, SCREEN_H / 2 - 16);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText3);
  tft.drawString("startuji...", SCREEN_W / 2, SCREEN_H / 2 + 18);
  tft.setFreeFont(nullptr);
}

void Ui::bootStatus(const char* title, const char* detail) {
  Art::paintRect(0, SCREEN_H / 2 - 6, SCREEN_W, 70);

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText);
  tft.drawString(title ? title : "", SCREEN_W / 2, SCREEN_H / 2 + 14);
  tft.setTextColor(cText3);
  tft.setTextFont(2);
  tft.drawString(detail ? detail : "", SCREEN_W / 2, SCREEN_H / 2 + 40);
  tft.setFreeFont(nullptr);
}

void Ui::nowPlaying(const PlayerState& st, bool full) {
  if (full) {
    dipBegin();
    refreshPalette();
    Art::paintBackground();
    Art::drawCover(ART_X, ART_Y, ART_SIZE);

    lastTitle[0] = lastArtist[0] = lastAlbum[0] = lastDevice[0] = '\0';
    lastClock[0] = '\0';
    lastVolume = -2;
    lastProgSec = lastDurSec = -1;
    lastFillW = -1;
    lastBars = 255;
    titleOff = 0; titleDir = 1; titleNext = 0;
  }

  if (strcmp(lastTitle, st.title) != 0) {
    strncpy(lastTitle, st.title, sizeof(lastTitle) - 1);
    titleW = measure(st.title, &FreeSansBold12pt7b, 0);
    titleOff = 0; titleDir = 1;
    titleNext = millis() + 1400;
    textLine(TXT_X, TITLE_Y, TXT_W, TITLE_H, st.title,
             &FreeSansBold12pt7b, 0, cText, 0);
  }
  if (strcmp(lastArtist, st.artist) != 0) {
    strncpy(lastArtist, st.artist, sizeof(lastArtist) - 1);
    textLine(TXT_X, ARTIST_Y, TXT_W, ARTIST_H, st.artist,
             &FreeSans9pt7b, 0, cText2);
  }
  if (strcmp(lastAlbum, st.album) != 0) {
    strncpy(lastAlbum, st.album, sizeof(lastAlbum) - 1);
    textLine(TXT_X, ALBUM_Y, TXT_W, ALBUM_H, st.album, nullptr, 2, cText3);
  }

  if (strcmp(lastDeviceLine, st.deviceName) != 0) {
    strncpy(lastDeviceLine, st.deviceName, sizeof(lastDeviceLine) - 1);
    lastDeviceLine[sizeof(lastDeviceLine) - 1] = '\0';
    char line[64];
    snprintf(line, sizeof(line), "%s%s",
             st.deviceName[0] ? "> " : "", st.deviceName);
    textLine(TXT_X, DEVICE_Y, TXT_W, DEVICE_H, line, nullptr, 2, cText3);
  }

  updateStatus();
  updateVolume(st.volume, st.supportsVolume);
  updateProgress(st.progressMs, st.durationMs);

  if (full || st.isPlaying != lastPlaying || st.shuffle != lastShuffle ||
      st.repeat != lastRepeat) {
    lastPlaying = st.isPlaying;
    lastShuffle = st.shuffle;
    lastRepeat  = st.repeat;
    drawControlBar(st);
  }

  if (full) dipEnd();
}

void Ui::updateProgress(uint32_t progressMs, uint32_t durationMs) {
  int32_t ps = (int32_t)(progressMs / 1000);
  int32_t ds = (int32_t)(durationMs / 1000);

  if (ps != lastProgSec || ds != lastDurSec) {
    char buf[12];
    if (ps != lastProgSec) {
      formatTime(progressMs, buf, sizeof(buf));
      textLine(TIME_L_X, PROG_ROW_Y, 40, PROG_ROW_H, buf, nullptr, 2, cText2);
    }
    if (ds != lastDurSec) {
      formatTime(durationMs, buf, sizeof(buf));
      textLine(TIME_R_X - 40, PROG_ROW_Y, 40, PROG_ROW_H, buf, nullptr, 2,
               cText3, 0, MR_DATUM);
    }
    lastProgSec = ps;
    lastDurSec  = ds;
  }
  drawProgressBar(progressMs, durationMs);
}

void Ui::updateControls(const PlayerState& st) {
  lastPlaying = st.isPlaying;
  lastShuffle = st.shuffle;
  lastRepeat  = st.repeat;
  for (uint8_t i = 0; i < CTRL_COUNT; i++) drawButton(i, st, false);
}

void Ui::updateVolume(int volume, bool supported) {
  if (volume == lastVolume) return;
  lastVolume = volume;
  drawVolume(volume, supported);
}

void Ui::setPageDots(uint8_t count, uint8_t active) {
  if (count == pageCount && active == pageActive) return;
  pageCount  = count;
  pageActive = active;
  lastClock[0] = '\0';           // vynutit prekresleni stavoveho radku
}

void Ui::updateStatus() {
  char clock[8] = {0};
#if FEAT_NTP_CLOCK
  Net::formatClock(clock, sizeof(clock));
#endif
  uint8_t bars = Net::bars();

  bool toastActive = toastText[0] && (int32_t)(millis() - toastUntil) < 0;
  const char* line = toastActive ? toastText : "";
  if (!toastActive && toastText[0]) toastText[0] = '\0';

  bool changed = (strcmp(lastDevice, line) != 0) ||
                 (strcmp(lastClock, clock) != 0) || (bars != lastBars);
  if (!changed) return;

  strncpy(lastDevice, line, sizeof(lastDevice) - 1);
  lastDevice[sizeof(lastDevice) - 1] = '\0';
  strncpy(lastClock, clock, sizeof(lastClock) - 1);
  lastClock[sizeof(lastClock) - 1] = '\0';
  lastBars = bars;

  Art::paintRect(0, STATUS_Y, SCREEN_W, STATUS_H);
  uint16_t bg = Art::pixelAt(SCREEN_W / 2, STATUS_H / 2);

  Icons::wifi(18, STATUS_Y + STATUS_H / 2 - 1, bars ? cText2 : cText3,
              blend565(bg, cText3, 90), bars);

  if (line[0])
    textLine(32, STATUS_Y + 2, 118, 18, line, nullptr, 2, cAccent);

  // Tecky stranek uprostred
  if (pageCount > 1) {
    int16_t step = 11;
    int16_t x0 = SCREEN_W / 2 - (pageCount - 1) * step / 2;
    for (uint8_t i = 0; i < pageCount; i++) {
      uint16_t c = (i == pageActive) ? cText : blend565(bg, cText, 70);
      int16_t r = (i == pageActive) ? 3 : 2;
      tft.fillSmoothCircle(x0 + i * step, STATUS_Y + STATUS_H / 2 - 1, r, c, bg);
    }
  }

  if (clock[0])
    textLine(SCREEN_W - 54, STATUS_Y + 2, 46, 18, clock, nullptr, 2, cText2,
             0, MR_DATUM);
}

void Ui::tickMarquee() {
  if (titleW <= TXT_W) return;
  uint32_t now = millis();
  if ((int32_t)(now - titleNext) < 0) return;

  int16_t maxOff = titleW - TXT_W + 6;
  titleOff += titleDir * 2;

  if (titleOff >= maxOff) { titleOff = maxOff; titleDir = -1; titleNext = now + 1400; }
  else if (titleOff <= 0) { titleOff = 0;      titleDir =  1; titleNext = now + 1400; }
  else                    { titleNext = now + 45; }

  textLine(TXT_X, TITLE_Y, TXT_W, TITLE_H, lastTitle,
           &FreeSansBold12pt7b, 0, cText, titleOff);
}

void Ui::idle(const char* title, const char* detail) {
  Art::unload();
  refreshPalette();
  Art::paintBackground();

  uint16_t bg = Art::pixelAt(SCREEN_W / 2, SCREEN_H / 2);
  tft.fillSmoothCircle(SCREEN_W / 2, 92, 34, blend565(bg, cText, 26), bg);
  Icons::play(SCREEN_W / 2 + 2, 92, blend565(bg, cText, 150), blend565(bg, cText, 26));

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(cText);
  tft.drawString(title ? title : "Nic nehraje", SCREEN_W / 2, 152);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText3);
  tft.drawString(detail ? detail : "", SCREEN_W / 2, 180);

#if FEAT_NTP_CLOCK
  char clock[8];
  if (Net::formatClock(clock, sizeof(clock))) {
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.setTextColor(cText2);
    tft.drawString(clock, SCREEN_W / 2, 214);
  }
#endif
  tft.setFreeFont(nullptr);

  lastTitle[0] = lastArtist[0] = lastAlbum[0] = lastDevice[0] = '\0';
  lastProgSec = lastDurSec = -1;
  lastFillW = -1;
  lastVolume = -2;
  lastBars = 255;
}

void Ui::error(const char* title, const char* detail) {
  Art::paintBackground();
  uint16_t bg = Art::pixelAt(SCREEN_W / 2, SCREEN_H / 2);

  tft.fillSmoothCircle(SCREEN_W / 2, 84, 26,
                       Col::to565(Col::rgb(0xC2, 0x3B, 0x3B)), bg);
  Icons::cross(SCREEN_W / 2, 84, cText,
               Col::to565(Col::rgb(0xC2, 0x3B, 0x3B)));

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(cText);
  tft.drawString(title ? title : "Chyba", SCREEN_W / 2, 140);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText3);
  tft.drawString(detail ? detail : "", SCREEN_W / 2, 170);
  tft.setFreeFont(nullptr);

  lastTitle[0] = '\0';
  lastDevice[0] = '\0';
  lastBars = 255;
}

void Ui::reauth() {
  Art::unload();
  refreshPalette();
  Art::paintBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(cText);
  tft.drawString("Spotify se odhlasilo", SCREEN_W / 2, 78);

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText2);
  tft.drawString("Refresh token uz neplati.", SCREEN_W / 2, 118);
  tft.drawString("Spust znovu tools/get_token.py", SCREEN_W / 2, 144);
  tft.setTextColor(cText3);
  tft.drawString("a nahraj sketch s novym tokenem.", SCREEN_W / 2, 168);
  tft.setFreeFont(nullptr);
}

void Ui::fullArt(const PlayerState& st) {
  Art::paintBackground();
  if (!Art::drawCoverFull()) {
    Art::drawCover((SCREEN_W - ART_SIZE) / 2, (SCREEN_H - ART_SIZE) / 2,
                   ART_SIZE);
  }

  // Ztmaveny pruh dole, aby byl nazev citelny i na svetlem obalu.
  glassPanel(0, SCREEN_H - 46, SCREEN_W, 46, 0, 92);

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(cText);
  tft.drawString(st.title, SCREEN_W / 2, SCREEN_H - 30);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText2);
  tft.drawString(st.artist, SCREEN_W / 2, SCREEN_H - 10);
  tft.setFreeFont(nullptr);

  lastTitle[0] = lastArtist[0] = lastAlbum[0] = lastDevice[0] = '\0';
  lastProgSec = lastDurSec = -1;
  lastFillW = -1;
  lastVolume = -2;
  lastBars = 255;
}

// ---------------------------------------------------------------------
//  Dotyk
// ---------------------------------------------------------------------
Hit Ui::hitTest(int16_t x, int16_t y) {
  if (x < 0 || y < 0) return Hit::None;

  if (y >= CTRL_HIT_Y && y < CTRL_HIT_Y + CTRL_HIT_H &&
      x >= CTRL_X && x < CTRL_X + CTRL_W) {
    int idx = (int)((int32_t)(x - CTRL_X) * CTRL_COUNT / CTRL_W);
    idx = constrain(idx, 0, CTRL_COUNT - 1);
    switch (idx) {
      case 0: return Hit::Shuffle;
      case 1: return Hit::Prev;
      case 2: return Hit::PlayPause;
      case 3: return Hit::Next;
      default: return Hit::Repeat;
    }
  }

#if FEAT_SEEK_BAR
  if (y >= PROG_HIT_Y && y < PROG_HIT_Y + PROG_HIT_H) return Hit::Progress;
#endif

#if FEAT_VOLUME_SLIDER
  if (y >= VOL_HIT_Y && y < VOL_HIT_Y + VOL_HIT_H && x >= VOL_X - 8)
    return Hit::Volume;
#endif

#if FEAT_FULLSCREEN_ART
  if (x >= ART_X && x < ART_X + ART_SIZE &&
      y >= ART_Y && y < ART_Y + ART_SIZE) return Hit::Art;
#endif

  if (y < STATUS_H) return Hit::Status;
  return Hit::None;
}

void Ui::pressFeedback(Hit h, bool down) {
  int idx = -1;
  switch (h) {
    case Hit::Shuffle:   idx = 0; break;
    case Hit::Prev:      idx = 1; break;
    case Hit::PlayPause: idx = 2; break;
    case Hit::Next:      idx = 3; break;
    case Hit::Repeat:    idx = 4; break;
    default: return;
  }
  PlayerState tmp;
  tmp.isPlaying = lastPlaying;
  tmp.shuffle   = lastShuffle;
  tmp.repeat    = lastRepeat;
  drawButton((uint8_t)idx, tmp, down);
}

int Ui::valueFromX(Hit h, int16_t x) {
  int16_t x0 = 0, w = 1;
  if (h == Hit::Progress) { x0 = PROG_BAR_X; w = PROG_BAR_W; }
  else if (h == Hit::Volume) { x0 = VOL_BAR_X; w = VOL_BAR_W; }
  else return -1;

  int v = (int)((int32_t)(x - x0) * 100 / w);
  return constrain(v, 0, 100);
}

// ---------------------------------------------------------------------
//  Podsviceni
// ---------------------------------------------------------------------
void Ui::dipBegin() {
  if (!blReady || blDipped) return;
  blDipped = true;
  // Rychly sjezd na ~55 % - dost na to, aby se prekresleni "schovalo",
  // ale ne tak, aby to vypadalo jako vypnuty displej.
  for (uint16_t d = blDuty; d > BL_MAX / 2; d -= 220) {
    ledcWrite(PIN_BACKLIGHT, d);
    delayMicroseconds(900);
  }
  ledcWrite(PIN_BACKLIGHT, BL_MAX / 2);
}

void Ui::dipEnd() {
  if (!blReady || !blDipped) return;
  blDipped = false;
  for (uint16_t d = BL_MAX / 2; d < blTarget; d += 150) {
    ledcWrite(PIN_BACKLIGHT, d);
    delayMicroseconds(700);
  }
  ledcWrite(PIN_BACKLIGHT, blTarget);
  blDuty = blTarget;
}

void Ui::setBacklight(uint16_t duty) {
  blTarget = constrain(duty, (uint16_t)BL_MIN, (uint16_t)BL_MAX);
}

void Ui::wakeBacklight() { blTarget = BL_MAX; }

void Ui::tickBacklight(uint32_t lastActivity) {
#if FEAT_AUTO_DIM
  uint16_t want = BL_MAX;
  if (millis() - lastActivity > BL_DIM_AFTER_MS) want = BL_DIM_LEVEL;

  #if FEAT_LDR_BRIGHTNESS
  // LDR: cim vic svetla, tim vetsi napeti -> mensi hodnota z deliče.
  // Ve tme stahneme jas, at to v noci nesviti do oci.
  static uint32_t lastLdr = 0;
  static uint16_t ldrScale = 255;
  if (millis() - lastLdr > 1500) {
    lastLdr = millis();
    int raw = analogRead(PIN_LDR);          // 0..4095
    int lux = 4095 - raw;                   // vetsi = svetleji
    ldrScale = (uint16_t)constrain(80 + lux * 175 / 4095, 80, 255);
  }
  want = (uint16_t)((uint32_t)want * ldrScale / 255);
  #endif

  blTarget = constrain(want, (uint16_t)BL_MIN, (uint16_t)BL_MAX);
#endif

  if (!blReady) return;
  if (blDuty == blTarget) return;
  if (blDuty < blTarget) blDuty = min<uint16_t>(blTarget, blDuty + BL_FADE_STEP);
  else                   blDuty = max<uint16_t>(blTarget, blDuty - BL_FADE_STEP);
  ledcWrite(PIN_BACKLIGHT, blDuty);
}

// ---------------------------------------------------------------------
void Ui::toast(const char* text) {
  strncpy(toastText, text ? text : "", sizeof(toastText) - 1);
  toastText[sizeof(toastText) - 1] = '\0';
  toastUntil = millis() + 2200;
  lastDevice[0] = '\0';        // vynutit prekresleni stavoveho radku
}

void Ui::clearToast() {
  toastText[0] = '\0';
  lastDevice[0] = '\0';
}


// ---------------------------------------------------------------------
//  RGB LED
// ---------------------------------------------------------------------
void Ui::setLed(uint32_t rgb888) {
#if FEAT_RGB_LED
  if (!ledOk) return;
  // Plne rozsvicena LED je pri pohledu ze predu oslnujici - stahneme ji
  // na petinu a jeste ji normalizujeme, aby byla poznat barva a ne jas.
  Col::Hsv h = Col::toHsv(rgb888);
  uint32_t c = Col::fromHsv(h.h, (uint8_t)max<int>(h.s, 120), 60);
  ledcWrite(PIN_LED_R, 255 - Col::R(c));    // ACTIVE LOW
  ledcWrite(PIN_LED_G, 255 - Col::G(c));
  ledcWrite(PIN_LED_B, 255 - Col::B(c));
#else
  (void)rgb888;
#endif
}

// ---------------------------------------------------------------------
//  Nastaveni  -  jednoducha modalni obrazovka
// ---------------------------------------------------------------------
namespace {

struct MenuItem { const char* label; const char* hint; Ui::SettingsAction action; };

const MenuItem MENU[] = {
  {"Kalibrace dotyku", "klepni na dva terce",        Ui::SettingsAction::Calibrate},
  {"Otocit displej",   "kdyz je obraz vzhuru nohama", Ui::SettingsAction::Rotate},
  {"Nastavit WiFi",    "otevre portal SpotifyDeck",   Ui::SettingsAction::WifiPortal},
  {"Smazat cache obalu", "uvolni misto v pameti",     Ui::SettingsAction::ClearCache},
  {"Restartovat",      "",                            Ui::SettingsAction::Restart},
};
constexpr int MENU_N = sizeof(MENU) / sizeof(MENU[0]);

constexpr int16_t ROW_H  = 38;
constexpr int16_t ROW_Y0 = 30;

void drawMenuRow(int i, bool pressed) {
  int16_t y = ROW_Y0 + i * ROW_H;
  uint16_t bg  = pressed ? cPanelHi : cPanel;
  tft.fillSmoothRoundRect(10, y, SCREEN_W - 20, ROW_H - 4, 8, bg, TFT_BLACK);

  tft.setTextDatum(ML_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextColor(cText);
  tft.drawString(MENU[i].label, 24, y + 13);
  tft.setFreeFont(nullptr);
  tft.setTextFont(2);
  tft.setTextColor(cText3);
  tft.drawString(MENU[i].hint, 24, y + 26);
}

}  // namespace

Ui::SettingsAction Ui::runSettings() {
  tft.fillScreen(TFT_BLACK);
  cPanel   = Col::to565(Col::rgb(0x16, 0x1C, 0x28));
  cPanelHi = Col::to565(Col::rgb(0x27, 0x31, 0x44));

  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(cText);
  tft.drawString("Nastaveni", 14, 4);
  tft.setFreeFont(nullptr);

  tft.setTextDatum(TR_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(cText3);
  tft.drawString("klepni mimo = zpet", SCREEN_W - 14, 8);

  for (int i = 0; i < MENU_N; i++) drawMenuRow(i, false);

  int pressedRow = -1;
  uint32_t deadline = millis() + 45000UL;

  while ((int32_t)(millis() - deadline) < 0) {
    TouchEvent ev = Touch::poll();

    if (ev.pressed) {
      deadline = millis() + 45000UL;
      int row = (ev.y - ROW_Y0) / ROW_H;
      if (ev.y >= ROW_Y0 && row >= 0 && row < MENU_N) {
        pressedRow = row;
        drawMenuRow(row, true);
      } else {
        pressedRow = -1;
      }
    }

    if (ev.released) {
      if (pressedRow >= 0) {
        drawMenuRow(pressedRow, false);
        int row = (ev.y - ROW_Y0) / ROW_H;
        if (ev.tap && row == pressedRow) return MENU[pressedRow].action;
        pressedRow = -1;
      } else if (ev.tap) {
        return SettingsAction::Back;
      }
    }

    delay(8);
  }
  return SettingsAction::Back;
}
