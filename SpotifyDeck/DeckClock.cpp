#include "DeckClock.h"

#include <Preferences.h>
#include <time.h>

#include "DeckArt.h"
#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckDraw.h"
#include "DeckNet.h"
#include "DeckTft.h"

namespace {

constexpr int16_t R      = 94;                 // polomer cifernika
constexpr int16_t SPR_D  = 2 * R + 2;          // 190 px
constexpr int16_t CX     = SCREEN_W / 2;
constexpr int16_t CY     = 130;
constexpr int16_t SPR_X  = CX - R - 1;
constexpr int16_t SPR_Y  = CY - R - 1;

// Barva, ktera se pri poslani spritu bere jako pruhledna - diky ni
// zustanou rohy ctvercoveho spritu prekryte pozadim stranky.
constexpr uint16_t MAGIC = TFT_MAGENTA;

TFT_eSprite face = TFT_eSprite(&tft);
bool        ready = false;
bool        digital = false;

int  lastSec = -1;
int  lastMin = -1;
int  lastDay = -1;
char lastDigital[8] = {0};

uint16_t cFace, cRing, cTick, cHand, cSecond, cText;

Preferences prefs;

const char* DAYS[7]   = {"Ne", "Po", "Ut", "St", "Ct", "Pa", "So"};
const char* MONTHS[12] = {"led", "uno", "bre", "dub", "kve", "cvn",
                          "cvc", "srp", "zar", "rij", "lis", "pro"};

void applyPalette() {
  Art::useArtBackground(false);
  Art::setFlatPalette(Col::rgb(0x14, 0x1A, 0x2C),
                      Col::rgb(0x04, 0x05, 0x09),
                      Col::rgb(0x5E, 0xC8, 0xFF));
  Draw::refreshPalette();

  cFace   = Col::to565(Col::rgb(0x0C, 0x11, 0x1C));
  cRing   = Col::to565(Col::rgb(0x24, 0x30, 0x48));
  cTick   = Col::to565(Col::rgb(0x7E, 0x8B, 0xA4));
  cHand   = Col::to565(Col::rgb(0xF2, 0xF6, 0xFF));
  cSecond = Draw::cAccent;
  cText   = Col::to565(Col::rgb(0xB6, 0xC2, 0xD6));
}

// Uhel 0 = 12 hodin, roste po smeru hodinovych rucicek.
void handPoint(float deg, float len, float& x, float& y) {
  float rad = (deg - 90.0f) * (float)DEG_TO_RAD;
  x = R + 1 + cosf(rad) * len;
  y = R + 1 + sinf(rad) * len;
}

void drawFace(const struct tm& t) {
  face.fillSprite(MAGIC);
  face.fillSmoothCircle(R + 1, R + 1, R, cFace, MAGIC);
  face.drawSmoothCircle(R + 1, R + 1, R, cRing, cFace);
  face.drawSmoothCircle(R + 1, R + 1, R - 1, cRing, cFace);

  // Minutove a hodinove risky
  for (int i = 0; i < 60; i++) {
    bool big = (i % 5 == 0);
    float x1, y1, x2, y2;
    handPoint(i * 6.0f, R - (big ? 14 : 7), x1, y1);
    handPoint(i * 6.0f, R - 4, x2, y2);
    face.drawWideLine(x1, y1, x2, y2, big ? 3.0f : 1.4f,
                      big ? cText : cTick, cFace);
  }

  // Cisla 12 / 3 / 6 / 9
  face.setTextDatum(MC_DATUM);
  face.setFreeFont(&FreeSansBold12pt7b);
  face.setTextColor(cText);
  const char* nums[4] = {"12", "3", "6", "9"};
  for (int i = 0; i < 4; i++) {
    float x, y;
    handPoint(i * 90.0f, R - 32, x, y);
    face.drawString(nums[i], (int32_t)x, (int32_t)y);
  }
  face.setFreeFont(nullptr);

  // Datum pod stredem
  char date[24];
  snprintf(date, sizeof(date), "%s %d. %s", DAYS[t.tm_wday % 7], t.tm_mday,
           MONTHS[t.tm_mon % 12]);
  face.setTextFont(2);
  face.setTextColor(Draw::cAccent);
  face.setTextDatum(MC_DATUM);
  face.drawString(date, R + 1, R + 1 + 34);
}

void drawHands(const struct tm& t) {
  float hourDeg = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
  float minDeg  = t.tm_min * 6.0f + t.tm_sec * 0.1f;
  float secDeg  = t.tm_sec * 6.0f;

  float x, y, bx, by;

  // hodinova
  handPoint(hourDeg, R * 0.52f, x, y);
  handPoint(hourDeg + 180.0f, 14.0f, bx, by);
  face.drawWideLine(bx, by, x, y, 6.5f, cHand, cFace);

  // minutova
  handPoint(minDeg, R * 0.76f, x, y);
  handPoint(minDeg + 180.0f, 18.0f, bx, by);
  face.drawWideLine(bx, by, x, y, 4.5f, cHand, cFace);

  // vterinova
  handPoint(secDeg, R * 0.84f, x, y);
  handPoint(secDeg + 180.0f, 22.0f, bx, by);
  face.drawWideLine(bx, by, x, y, 2.0f, cSecond, cFace);
  face.fillSmoothCircle((int32_t)x, (int32_t)y, 3, cSecond, cFace);

  // stredovy cep
  face.fillSmoothCircle(R + 1, R + 1, 6, cSecond, cFace);
  face.fillSmoothCircle(R + 1, R + 1, 2, cFace, cSecond);
}

void pushFace() { face.pushSprite(SPR_X, SPR_Y, MAGIC); }

void drawDigital(const struct tm& t, bool full) {
  char now[8];
  strftime(now, sizeof(now), "%H:%M", &t);

  if (full) {
    Art::paintBackground();
    lastDigital[0] = '\0';
    lastSec = -1;
  }

  if (strcmp(now, lastDigital) != 0) {
    strncpy(lastDigital, now, sizeof(lastDigital) - 1);
    Art::paintRect(0, 76, SCREEN_W, 76);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(Draw::cText);
    // Font 7 je sedmisegmentovy - na velke hodiny sedi nejlip.
    tft.drawString(now, SCREEN_W / 2, 112, 7);
  }

  // Vteriny mensim pismem a v akcentu
  char sec[4];
  snprintf(sec, sizeof(sec), "%02d", t.tm_sec);
  Art::paintRect(SCREEN_W / 2 + 84, 116, 40, 26);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(Draw::cAccent);
  tft.drawString(sec, SCREEN_W / 2 + 88, 128, 4);

  char date[32];
  snprintf(date, sizeof(date), "%s %d. %s %d", DAYS[t.tm_wday % 7],
           t.tm_mday, MONTHS[t.tm_mon % 12], 1900 + t.tm_year);
  Draw::text(40, 168, 240, 20, date, &FreeSans9pt7b, 0, Draw::cText2, 0,
             TC_DATUM);
}

}  // namespace

// =====================================================================
void Clock::begin() {
  prefs.begin("deck", true);
  digital = prefs.getBool("clkdig", false);
  prefs.end();
}

void Clock::enter() {
  applyPalette();
  if (!digital && !ready) {
    face.setColorDepth(8);
    ready = (face.createSprite(SPR_D, SPR_D) != nullptr);
    if (!ready) LOGLN("[clock] sprite se nevesel do pameti, jedu digitalne");
  }
  lastSec = lastMin = lastDay = -1;
  lastDigital[0] = '\0';
}

void Clock::leave() {
  if (ready) { face.deleteSprite(); ready = false; }
}

void Clock::draw(bool full) {
  struct tm t;
  if (!getLocalTime(&t, 5)) {
    if (full) {
      applyPalette();
      Art::paintBackground();
      Draw::bigText(SCREEN_W / 2, 120, "cekam na cas z internetu",
                    &FreeSans9pt7b, Draw::cText2, MC_DATUM);
    }
    return;
  }

  if (digital || !ready) { drawDigital(t, full); return; }

  if (full) Art::paintBackground();
  drawFace(t);
  drawHands(t);
  pushFace();

  lastSec = t.tm_sec;
  lastMin = t.tm_min;
  lastDay = t.tm_mday;
}

void Clock::tick() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return;
  if (t.tm_sec == lastSec) return;

  if (digital || !ready) { drawDigital(t, false); lastSec = t.tm_sec; return; }

  // Cely cifernik i rucicky se skladaji ve spritu a na displej jdou
  // jednim pruchodem - proto se muze prekreslovat vsechno kazdou vterinu,
  // aniz by to blikalo.
  drawFace(t);
  drawHands(t);
  lastMin = t.tm_min;
  lastDay = t.tm_mday;
  pushFace();
  lastSec = t.tm_sec;
}

bool Clock::handleTouch(const TouchEvent& ev) {
  if (!ev.released || !ev.tap) return false;

  digital = !digital;
  prefs.begin("deck", false);
  prefs.putBool("clkdig", digital);
  prefs.end();

  if (digital) leave();
  else         enter();
  draw(true);
  return true;
}
