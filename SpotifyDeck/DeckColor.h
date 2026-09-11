// ---------------------------------------------------------------------
//  DeckColor.h  -  color math for the 16-bit display
//
//  All inline, because it runs in loops over tens of thousands of pixels.
//  The internal representation is RGB888 in a uint32_t (0x00RRGGBB);
//  only the result goes to the display as RGB565.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Col {

inline uint16_t to565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

inline uint16_t to565(uint32_t rgb) {
  return to565((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

inline uint32_t to888(uint16_t c) {
  // The top bits are copied down so that 0xFFFF yields exactly 0xFFFFFF.
  uint8_t r = (uint8_t)((c >> 11) & 0x1F);
  uint8_t g = (uint8_t)((c >> 5) & 0x3F);
  uint8_t b = (uint8_t)(c & 0x1F);
  r = (uint8_t)((r << 3) | (r >> 2));
  g = (uint8_t)((g << 2) | (g >> 4));
  b = (uint8_t)((b << 3) | (b >> 2));
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

inline uint8_t R(uint32_t c) { return (uint8_t)(c >> 16); }
inline uint8_t G(uint32_t c) { return (uint8_t)(c >> 8); }
inline uint8_t B(uint32_t c) { return (uint8_t)c; }

// Linear blend between two colors. t = 0..255 (0 = a, 255 = b).
inline uint32_t mix(uint32_t a, uint32_t b, uint8_t t) {
  uint16_t it = 255 - t;
  return rgb((uint8_t)((R(a) * it + R(b) * t) / 255),
             (uint8_t)((G(a) * it + G(b) * t) / 255),
             (uint8_t)((B(a) * it + B(b) * t) / 255));
}

// Darken / brighten. f = 0..255, where 255 changes nothing.
inline uint32_t scale(uint32_t c, uint16_t f) {
  uint16_t r = (uint16_t)R(c) * f / 255;
  uint16_t g = (uint16_t)G(c) * f / 255;
  uint16_t b = (uint16_t)B(c) * f / 255;
  return rgb((uint8_t)min<uint16_t>(r, 255),
             (uint8_t)min<uint16_t>(g, 255),
             (uint8_t)min<uint16_t>(b, 255));
}

// Perceived lightness 0..255 (ITU-R BT.601 weights, good enough and all integer).
inline uint8_t luma(uint32_t c) {
  return (uint8_t)(((uint32_t)R(c) * 77 + (uint32_t)G(c) * 150 +
                    (uint32_t)B(c) * 29) >> 8);
}

// --- HSV -------------------------------------------------------------
//  h = 0..359, s = 0..255, v = 0..255
struct Hsv { uint16_t h; uint8_t s; uint8_t v; };

inline Hsv toHsv(uint32_t c) {
  int r = R(c), g = G(c), b = B(c);
  int mx = max(r, max(g, b));
  int mn = min(r, min(g, b));
  int d  = mx - mn;

  Hsv out;
  out.v = (uint8_t)mx;
  out.s = mx ? (uint8_t)(255 * d / mx) : 0;

  if (d == 0) { out.h = 0; return out; }

  int h;
  if (mx == r)      h = 60 * (g - b) / d;
  else if (mx == g) h = 120 + 60 * (b - r) / d;
  else              h = 240 + 60 * (r - g) / d;
  if (h < 0) h += 360;
  out.h = (uint16_t)h;
  return out;
}

inline uint32_t fromHsv(uint16_t h, uint8_t s, uint8_t v) {
  if (s == 0) return rgb(v, v, v);
  h %= 360;
  int sector = h / 60;
  int f      = h % 60;
  int p = v * (255 - s) / 255;
  int q = v * (255 - (s * f) / 60) / 255;
  int t = v * (255 - (s * (60 - f)) / 60) / 255;
  switch (sector) {
    case 0:  return rgb(v, (uint8_t)t, (uint8_t)p);
    case 1:  return rgb((uint8_t)q, v, (uint8_t)p);
    case 2:  return rgb((uint8_t)p, v, (uint8_t)t);
    case 3:  return rgb((uint8_t)p, (uint8_t)q, v);
    case 4:  return rgb((uint8_t)t, (uint8_t)p, v);
    default: return rgb(v, (uint8_t)p, (uint8_t)q);
  }
}

// Accent color: keeps the hue of the artwork but pushes saturation and
// value up so it stays readable on a dark background. Gray art stays gray.
inline uint32_t accentFrom(uint32_t base) {
  Hsv h = toHsv(base);
  if (h.s < 40) return rgb(0xE6, 0xEC, 0xF5);            // monochrome art
  uint8_t s = (uint8_t)max<int>(h.s, 150);
  return fromHsv(h.h, s, 245);
}

// Darkened shade of the artwork for the background (used when blur is
// off or when the artwork could not be downloaded).
inline uint32_t shadeFrom(uint32_t base, uint8_t value) {
  Hsv h = toHsv(base);
  uint8_t s = (uint8_t)min<int>(h.s, 170);
  return fromHsv(h.h, s, value);
}

}  // namespace Col
