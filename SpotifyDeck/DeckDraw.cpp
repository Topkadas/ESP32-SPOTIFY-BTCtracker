#include "DeckDraw.h"

#include "DeckArt.h"
#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckTft.h"

namespace Draw {

uint16_t cText    = 0xFFFF;
uint16_t cText2   = 0xCE79;
uint16_t cText3   = 0x8410;
uint16_t cAccent  = 0x07E0;
uint16_t cPanel   = 0x0000;
uint16_t cPanelHi = 0x2124;
uint16_t cGood    = 0x2648;
uint16_t cBad     = 0xE986;

}  // namespace Draw

namespace {

// Sprite musi byt siroky jako cely displej. Draw::text() si sirku orezava
// na tuhle hodnotu, takze kazde uzsi reseni tise poskodi vsechny volajici,
// kteri kresli sirsi radek - text se vycentruje vedle a konec predchoziho
// retezce se nikdy nesmaze. 320 x 36 x 2 B = 23 kB haldy.
constexpr int16_t SPR_W = SCREEN_W;
constexpr int16_t SPR_H = 36;

TFT_eSprite spr   = TFT_eSprite(&tft);
bool        sprOk = false;
uint16_t    rowTmp[SCREEN_W];

}  // namespace

bool Draw::begin() {
  if (sprOk) return true;
  spr.setColorDepth(16);
  sprOk = (spr.createSprite(SPR_W, SPR_H) != nullptr);
  if (!sprOk) LOGLN("[draw] POZOR: sprite se nevytvoril, text bude blikat");
  return sprOk;
}

uint16_t Draw::blend(uint16_t a, uint16_t b, uint8_t t) {
  return Col::to565(Col::mix(Col::to888(a), Col::to888(b), t));
}

void Draw::refreshPalette() {
  cAccent = Col::to565(Art::accent());
  cText   = Col::to565(Col::rgb(0xFF, 0xFF, 0xFF));
  cText2  = Col::to565(Col::rgb(0xCB, 0xD4, 0xE3));
  cText3  = Col::to565(Col::rgb(0x93, 0x9F, 0xB4));
  cGood   = Col::to565(Col::rgb(0x3F, 0xD6, 0x7A));
  cBad    = Col::to565(Col::rgb(0xFF, 0x6B, 0x6B));

  // Barva sklenene listy - vezme se pozadi pod ni a ztmavi se.
  uint32_t under = Col::to888(Art::pixelAt(SCREEN_W / 2, CTRL_CY));
  cPanel   = Col::to565(Col::scale(under, 108));
  cPanelHi = blend(cPanel, cText, 34);
}

namespace {

// Naplni oblast spritu pozadim z DeckArt. Sprite si barvy drzi
// s prohozenymi bajty, proto ten bswap.
void spriteBackground(int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t solid, bool useSolid) {
  uint16_t* buf = (uint16_t*)spr.getPointer();
  if (!buf) return;

  if (useSolid) {
    uint16_t v = __builtin_bswap16(solid);
    for (int16_t row = 0; row < h; row++) {
      uint16_t* dst = buf + (size_t)row * SPR_W;
      for (int16_t i = 0; i < w; i++) dst[i] = v;
    }
    return;
  }

  for (int16_t row = 0; row < h; row++) {
    Art::rowInto(rowTmp, x, y + row, w);
    uint16_t* dst = buf + (size_t)row * SPR_W;
    for (int16_t i = 0; i < w; i++) dst[i] = __builtin_bswap16(rowTmp[i]);
  }
}

}  // namespace

void Draw::text(int16_t x, int16_t y, int16_t w, int16_t h, const char* s,
                const GFXfont* freeFont, uint8_t builtin, uint16_t color,
                int16_t offset, uint8_t datum, uint16_t bgColor) {
  if (!sprOk) return;
  if (w > SPR_W) w = SPR_W;
  if (h > SPR_H) h = SPR_H;
  if (w <= 0 || h <= 0) return;

  spriteBackground(x, y, w, h, bgColor, bgColor != 0);

  if (builtin) spr.setTextFont(builtin);
  else         spr.setFreeFont(freeFont);
  spr.setTextColor(color);
  spr.setTextDatum(datum);

  // Text se vzdy sazi svisle na stred boxu (drawString dostava y = h/2),
  // takze horni a dolni datum nedava smysl - glyfy by se kreslily pod
  // spodni hranu spritu a orizly by se. Prevedeme je na stredove.
  if      (datum == TL_DATUM || datum == BL_DATUM) datum = ML_DATUM;
  else if (datum == TC_DATUM || datum == BC_DATUM) datum = MC_DATUM;
  else if (datum == TR_DATUM || datum == BR_DATUM) datum = MR_DATUM;

  int16_t tx = -offset;
  if      (datum == MC_DATUM) tx = w / 2;
  else if (datum == MR_DATUM) tx = w;

  spr.drawString(s && *s ? s : "", tx, h / 2);
  spr.pushSprite(x, y, 0, 0, w, h);
}

int16_t Draw::measure(const char* s, const GFXfont* freeFont, uint8_t builtin) {
  if (!sprOk || !s) return 0;
  if (builtin) spr.setTextFont(builtin);
  else         spr.setFreeFont(freeFont);
  return spr.textWidth(s);
}

void Draw::glass(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint8_t darken) {
  if (w <= 0 || h <= 0 || w > SCREEN_W) return;
  int32_t rr = (int32_t)r * r;
  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);

  for (int16_t dy = 0; dy < h; dy++) {
    Art::rowInto(rowTmp, x, y + dy, w);
    for (int16_t dx = 0; dx < w; dx++) {
      int16_t ex = 0, ey = 0;
      if (dx < r)           ex = r - dx;
      else if (dx >= w - r) ex = dx - (w - r) + 1;
      if (dy < r)           ey = r - dy;
      else if (dy >= h - r) ey = dy - (h - r) + 1;

      if (ex && ey && (int32_t)ex * ex + (int32_t)ey * ey > rr) continue;

      uint32_t c = Col::scale(Col::to888(rowTmp[dx]), darken);
      if (dy == 0 || (dy == 1 && dx > r && dx < w - r))
        c = Col::mix(c, Col::rgb(255, 255, 255), 26);   // jemny horni odlesk
      rowTmp[dx] = Col::to565(c);
    }
    tft.pushImage(x, y + dy, w, 1, rowTmp);
  }
  tft.setSwapBytes(swap);
}

void Draw::bigText(int16_t x, int16_t y, const char* s, const GFXfont* f,
                   uint16_t color, uint8_t datum) {
  tft.setTextDatum(datum);
  if (f) tft.setFreeFont(f);
  else   tft.setTextFont(2);
  tft.setTextColor(color);
  tft.drawString(s ? s : "", x, y);
  tft.setFreeFont(nullptr);
}

void Draw::degree(int16_t x, int16_t y, int16_t r, uint16_t color,
                  uint16_t bg) {
  tft.drawSmoothCircle(x, y, r, color, bg);
  if (r > 3) tft.drawSmoothCircle(x, y, r - 1, color, bg);
}
