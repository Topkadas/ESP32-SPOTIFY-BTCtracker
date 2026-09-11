#include "DeckIcons.h"

#include "DeckColor.h"
#include "DeckTft.h"

namespace {

// A triangle with smoothed edges: fill it first, then run a thin "wide
// line" around the outline, which blends the edges into the background.
void smoothTriangle(float x1, float y1, float x2, float y2,
                    float x3, float y3, uint16_t fg, uint16_t bg) {
  tft.fillTriangle((int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2,
                   (int32_t)x3, (int32_t)y3, fg);
  tft.drawWideLine(x1, y1, x2, y2, 1.1f, fg, bg);
  tft.drawWideLine(x2, y2, x3, y3, 1.1f, fg, bg);
  tft.drawWideLine(x3, y3, x1, y1, 1.1f, fg, bg);
}

inline uint16_t blend565(uint16_t a, uint16_t b, uint8_t t) {
  return Col::to565(Col::mix(Col::to888(a), Col::to888(b), t));
}

constexpr float STROKE = 2.6f;

}  // namespace

void Icons::play(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  smoothTriangle(cx - 6.0f, cy - 9.5f,
                 cx - 6.0f, cy + 9.5f,
                 cx + 9.5f, cy, fg, bg);
}

void Icons::pause(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  tft.fillSmoothRoundRect(cx - 8, cy - 9, 5, 19, 2, fg, bg);
  tft.fillSmoothRoundRect(cx + 3, cy - 9, 5, 19, 2, fg, bg);
}

void Icons::next(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  smoothTriangle(cx - 9.0f, cy - 8.5f,
                 cx - 9.0f, cy + 8.5f,
                 cx + 2.5f, cy, fg, bg);
  tft.fillSmoothRoundRect(cx + 5, cy - 8, 3, 17, 1, fg, bg);
}

void Icons::prev(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  smoothTriangle(cx + 9.0f, cy - 8.5f,
                 cx + 9.0f, cy + 8.5f,
                 cx - 2.5f, cy, fg, bg);
  tft.fillSmoothRoundRect(cx - 8, cy - 8, 3, 17, 1, fg, bg);
}

void Icons::shuffle(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  // lower path: straight in from the left, then up to the right
  tft.drawWideLine(cx - 11.0f, cy + 5.5f, cx - 4.0f, cy + 5.5f, STROKE, fg, bg);
  tft.drawWideLine(cx - 4.0f,  cy + 5.5f, cx + 5.0f, cy - 4.5f, STROKE, fg, bg);
  smoothTriangle(cx + 10.0f, cy - 8.5f,
                 cx + 2.5f,  cy - 7.0f,
                 cx + 9.0f,  cy - 1.0f, fg, bg);

  // upper path: straight in from the left, then down to the right (they
  // cross in the middle)
  tft.drawWideLine(cx - 11.0f, cy - 5.5f, cx - 5.0f, cy - 5.5f, STROKE, fg, bg);
  tft.drawWideLine(cx - 5.0f,  cy - 5.5f, cx - 0.5f, cy - 1.0f, STROKE, fg, bg);
  tft.drawWideLine(cx + 1.0f,  cy + 1.0f, cx + 5.0f, cy + 4.5f, STROKE, fg, bg);
  smoothTriangle(cx + 10.0f, cy + 8.5f,
                 cx + 2.5f,  cy + 7.0f,
                 cx + 9.0f,  cy + 1.0f, fg, bg);
}

void Icons::repeat(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg, bool single) {
  // top arrow, pointing right
  tft.drawWideLine(cx - 8.0f, cy - 6.5f, cx + 4.0f, cy - 6.5f, STROKE, fg, bg);
  smoothTriangle(cx + 9.5f, cy - 6.5f,
                 cx + 3.5f, cy - 10.5f,
                 cx + 3.5f, cy - 2.5f, fg, bg);
  tft.drawWideLine(cx - 8.0f, cy - 6.5f, cx - 8.0f, cy - 2.0f, STROKE, fg, bg);

  // bottom arrow, pointing left
  tft.drawWideLine(cx + 8.0f, cy + 6.5f, cx - 4.0f, cy + 6.5f, STROKE, fg, bg);
  smoothTriangle(cx - 9.5f, cy + 6.5f,
                 cx - 3.5f, cy + 2.5f,
                 cx - 3.5f, cy + 10.5f, fg, bg);
  tft.drawWideLine(cx + 8.0f, cy + 6.5f, cx + 8.0f, cy + 2.0f, STROKE, fg, bg);

  if (single) {
    // a small "1" in the middle of the loop
    tft.drawWideLine(cx + 0.5f, cy - 2.5f, cx + 0.5f, cy + 2.5f, 2.2f, fg, bg);
    tft.drawWideLine(cx - 1.8f, cy - 1.0f, cx + 0.5f, cy - 2.8f, 2.0f, fg, bg);
  }
}

void Icons::volume(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg,
                   uint8_t level) {
  tft.fillSmoothRoundRect(cx - 10, cy - 3, 4, 7, 1, fg, bg);
  smoothTriangle(cx - 6.0f, cy - 3.0f, cx - 6.0f, cy + 4.0f,
                 cx - 1.0f, cy + 8.0f, fg, bg);
  smoothTriangle(cx - 6.0f, cy - 3.0f, cx - 1.0f, cy + 8.0f,
                 cx - 1.0f, cy - 8.0f, fg, bg);

  if (level == 0) {                     // muted - the slash
    tft.drawWideLine(cx + 2.0f, cy - 5.0f, cx + 10.0f, cy + 5.0f, 2.2f, fg, bg);
    tft.drawWideLine(cx + 10.0f, cy - 5.0f, cx + 2.0f, cy + 5.0f, 2.2f, fg, bg);
    return;
  }

  // Mind the TFT_eSPI convention: angle 0 is at the bottom (6 o'clock)
  // and grows clockwise, so "to the right" is 270 degrees.
  uint16_t weak = blend565(bg, fg, 70);
  tft.drawSmoothArc(cx - 1, cy, 7, 5, 235, 305,
                    (level >= 1) ? fg : weak, bg, true);
  tft.drawSmoothArc(cx - 1, cy, 12, 10, 228, 312,
                    (level >= 2) ? fg : weak, bg, true);
}

void Icons::wifi(int16_t cx, int16_t cy, uint16_t fg, uint16_t dim,
                 uint8_t bars) {
  // Four bars growing to the right - at this size more legible than
  // arcs, and it needs no antialiasing.
  for (uint8_t i = 0; i < 4; i++) {
    int16_t h = 3 + i * 3;
    int16_t x = cx - 7 + i * 4;
    tft.fillRect(x, cy + 5 - h, 3, h, (i < bars) ? fg : dim);
  }
}

void Icons::spinner(int16_t cx, int16_t cy, int16_t r, uint16_t fg, uint16_t bg,
                    uint8_t phase) {
  uint32_t start = (uint32_t)phase * 30;
  tft.drawSmoothArc(cx, cy, r, r - 3, 0, 359, blend565(bg, fg, 46), bg, false);
  tft.drawSmoothArc(cx, cy, r, r - 3, start % 360, (start + 100) % 360,
                    fg, bg, true);
}

void Icons::tick(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  tft.drawWideLine(cx - 7.0f, cy + 0.5f, cx - 2.0f, cy + 5.5f, 3.0f, fg, bg);
  tft.drawWideLine(cx - 2.0f, cy + 5.5f, cx + 7.5f, cy - 5.5f, 3.0f, fg, bg);
}

void Icons::cross(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  tft.drawWideLine(cx - 6.0f, cy - 6.0f, cx + 6.0f, cy + 6.0f, 3.0f, fg, bg);
  tft.drawWideLine(cx + 6.0f, cy - 6.0f, cx - 6.0f, cy + 6.0f, 3.0f, fg, bg);
}

void Icons::activeDot(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  tft.fillSmoothCircle(cx, cy, 2, fg, bg);
}

// =====================================================================
//  Weather icons
// =====================================================================
namespace {

// Every shape is computed relative to "size" (half the icon width), so
// one definition serves both the large and the small variant.
struct Wx {
  float cx, cy, k;
  uint16_t fg, accent, bg;
  float X(float v) const { return cx + v * k; }
  float Y(float v) const { return cy + v * k; }
  int32_t Xi(float v) const { return (int32_t)lroundf(X(v)); }
  int32_t Yi(float v) const { return (int32_t)lroundf(Y(v)); }
  int32_t R(float v)  const { return (int32_t)max(1L, lroundf(v * k)); }
};

void sunDisc(const Wx& w, float r, float rayIn, float rayOut, float thick) {
  tft.fillSmoothCircle(w.Xi(0), w.Yi(0), w.R(r), w.accent, w.bg);
  for (int i = 0; i < 8; i++) {
    float a = i * (float)PI / 4.0f;
    float sx = w.X(cosf(a) * rayIn),  sy = w.Y(sinf(a) * rayIn);
    float ex = w.X(cosf(a) * rayOut), ey = w.Y(sinf(a) * rayOut);
    tft.drawWideLine(sx, sy, ex, ey, thick * w.k, w.accent, w.bg);
  }
}

void moonDisc(const Wx& w, float r) {
  tft.fillSmoothCircle(w.Xi(0), w.Yi(0), w.R(r), w.accent, w.bg);
  // Cutting a second circle out in the background color makes a crescent.
  tft.fillSmoothCircle(w.Xi(r * 0.55f), w.Yi(-r * 0.45f), w.R(r * 0.92f),
                       w.bg, w.accent);
}

void cloudShape(const Wx& w, float dy, float scale, uint16_t color) {
  Wx c = w;
  c.cy += dy * w.k;
  c.k  *= scale;
  tft.fillSmoothCircle(c.Xi(-7), c.Yi(2),  c.R(7), color, w.bg);
  tft.fillSmoothCircle(c.Xi(1),  c.Yi(-3), c.R(9), color, w.bg);
  tft.fillSmoothCircle(c.Xi(9),  c.Yi(2),  c.R(6), color, w.bg);
  tft.fillSmoothRoundRect(c.Xi(-13), c.Yi(1), c.R(26), c.R(8), c.R(4),
                          color, w.bg);
}

void drops(const Wx& w, int count, float len, float startY, uint16_t color) {
  for (int i = 0; i < count; i++) {
    float x = -7.0f + i * 7.0f;
    tft.drawWideLine(w.X(x), w.Y(startY), w.X(x - 2.5f), w.Y(startY + len),
                     2.0f * w.k, color, w.bg);
  }
}

void flakes(const Wx& w, int count, float startY, uint16_t color) {
  for (int i = 0; i < count; i++) {
    float x = -7.0f + i * 7.0f;
    float y = startY + (i & 1 ? 3.0f : 0.0f);
    float r = 3.0f;
    for (int a = 0; a < 3; a++) {
      float ang = a * (float)PI / 3.0f;
      tft.drawWideLine(w.X(x - cosf(ang) * r), w.Y(y - sinf(ang) * r),
                       w.X(x + cosf(ang) * r), w.Y(y + sinf(ang) * r),
                       1.6f * w.k, color, w.bg);
    }
  }
}

void bolt(const Wx& w, uint16_t color) {
  tft.fillTriangle(w.Xi(2), w.Yi(6), w.Xi(-5), w.Yi(17), w.Xi(1), w.Yi(17),
                   color);
  tft.fillTriangle(w.Xi(1), w.Yi(17), w.Xi(-2), w.Yi(25), w.Xi(7), w.Yi(13),
                   color);
  tft.fillTriangle(w.Xi(7), w.Yi(13), w.Xi(1), w.Yi(13), w.Xi(1), w.Yi(17),
                   color);
}

}  // namespace

void Icons::weather(int16_t cx, int16_t cy, int16_t size, WxIcon ic, bool day,
                    uint16_t fg, uint16_t accent, uint16_t bg) {
  Wx w{(float)cx, (float)cy, (float)size / 22.0f, fg, accent, bg};

  switch (ic) {
    case WxIcon::Clear:
      if (day) sunDisc(w, 9.0f, 12.5f, 17.5f, 2.6f);
      else     moonDisc(w, 11.0f);
      break;

    case WxIcon::PartCloud: {
      Wx s = w;
      s.cx -= 7.0f * w.k;
      s.cy -= 8.0f * w.k;
      if (day) sunDisc(s, 6.5f, 9.0f, 12.5f, 2.2f);
      else     moonDisc(s, 7.5f);
      cloudShape(w, 4.0f, 0.9f, fg);
      break;
    }

    case WxIcon::Cloud:
      cloudShape(w, 3.0f, 0.78f, blend565(fg, bg, 96));   // the back, darker layer
      cloudShape(w, 0.0f, 1.0f, fg);
      break;

    case WxIcon::Fog:
      cloudShape(w, -4.0f, 0.9f, fg);
      for (int i = 0; i < 3; i++) {
        float y = 9.0f + i * 5.0f;
        float half = 11.0f - i * 2.0f;
        tft.drawWideLine(w.X(-half), w.Y(y), w.X(half), w.Y(y), 2.2f * w.k,
                         fg, bg);
      }
      break;

    case WxIcon::Drizzle:
      cloudShape(w, -4.0f, 1.0f, fg);
      drops(w, 3, 4.0f, 9.0f, accent);
      break;

    case WxIcon::Rain:
      cloudShape(w, -4.0f, 1.0f, fg);
      drops(w, 3, 9.0f, 8.0f, accent);
      break;

    case WxIcon::Showers: {
      Wx s = w;
      s.cx -= 8.0f * w.k;
      s.cy -= 10.0f * w.k;
      if (day) sunDisc(s, 5.5f, 8.0f, 11.0f, 2.0f);
      cloudShape(w, -2.0f, 0.92f, fg);
      drops(w, 2, 7.0f, 9.0f, accent);
      break;
    }

    case WxIcon::Snow:
      cloudShape(w, -4.0f, 1.0f, fg);
      flakes(w, 3, 12.0f, accent);
      break;

    case WxIcon::Thunder:
      cloudShape(w, -6.0f, 1.0f, fg);
      bolt(w, accent);
      break;

    default:
      tft.drawSmoothCircle(cx, cy, size, fg, bg);
      break;
  }
}

void Icons::trend(int16_t cx, int16_t cy, int16_t size, bool up, uint16_t fg,
                  uint16_t bg) {
  float h = size, wd = size * 0.85f;
  if (up) {
    tft.fillTriangle(cx, cy - h, cx - wd, cy + h * 0.4f, cx + wd,
                     cy + h * 0.4f, fg);
    tft.drawWideLine(cx, cy - h, cx - wd, cy + h * 0.4f, 1.1f, fg, bg);
    tft.drawWideLine(cx, cy - h, cx + wd, cy + h * 0.4f, 1.1f, fg, bg);
  } else {
    tft.fillTriangle(cx, cy + h, cx - wd, cy - h * 0.4f, cx + wd,
                     cy - h * 0.4f, fg);
    tft.drawWideLine(cx, cy + h, cx - wd, cy - h * 0.4f, 1.1f, fg, bg);
    tft.drawWideLine(cx, cy + h, cx + wd, cy - h * 0.4f, 1.1f, fg, bg);
  }
}
