// ---------------------------------------------------------------------
//  DeckText.h  -  UTF-8 to ASCII folding
//
//  The built-in TFT_eSPI fonts (Font2/Font4) and the Adafruit FreeFonts
//  only carry characters 0x20-0x7E. Hand them an accented name the way
//  Spotify really spells it - Dvorak with a hacek, Beyonce with an acute
//  - and you get two random rectangles instead of the letter. So every
//  string that comes off the network is folded here first: accents are
//  stripped, unknown characters become a question mark.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Text {

// U+00C0 .. U+00FF  (two-byte UTF-8 starting with 0xC3)
static const char LATIN1[65] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPs"
    "aaaaaaaceeeeiiiidnooooo/ouuuuypy";

// U+0100 .. U+013F  (0xC4)
static const char LATIN_A[65] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlL";

// U+0140 .. U+017F  (0xC5)
static const char LATIN_B[65] =
    "lLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

// Folds a UTF-8 string down to ASCII. Always NUL-terminated.
inline void fold(const char* src, char* dst, size_t cap) {
  if (!cap) return;
  if (!src) { dst[0] = '\0'; return; }

  size_t o = 0;
  const uint8_t* p = (const uint8_t*)src;

  while (*p && o + 1 < cap) {
    uint8_t c = *p;

    if (c < 0x80) {                       // plain ASCII
      dst[o++] = (char)c;
      p++;
      continue;
    }

    if ((c & 0xE0) == 0xC0 && p[1]) {     // two-byte sequence
      uint16_t cp = ((uint16_t)(c & 0x1F) << 6) | (p[1] & 0x3F);
      char out = '?';
      if (cp >= 0x00C0 && cp <= 0x00FF)      out = LATIN1[cp - 0x00C0];
      else if (cp >= 0x0100 && cp <= 0x013F) out = LATIN_A[cp - 0x0100];
      else if (cp >= 0x0140 && cp <= 0x017F) out = LATIN_B[cp - 0x0140];
      else if (cp < 0x00C0)                  out = ' ';
      dst[o++] = out;
      p += 2;
      continue;
    }

    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {   // three-byte sequence
      uint32_t cp = ((uint32_t)(c & 0x0F) << 12) |
                    ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      const char* rep = "?";
      switch (cp) {
        case 0x2018: case 0x2019: case 0x02BC: rep = "'";   break;
        case 0x201C: case 0x201D:              rep = "\""; break;
        case 0x2013: case 0x2014: case 0x2212: rep = "-";   break;
        case 0x2026:                           rep = "..."; break;
        case 0x00A0: case 0x2009: case 0x202F: rep = " ";   break;
        case 0x20AC:                           rep = "EUR"; break;
        case 0x2022:                           rep = "*";   break;
        default: break;
      }
      while (*rep && o + 1 < cap) dst[o++] = *rep++;
      p += 3;
      continue;
    }

    // Four-byte sequences (emoji) and everything else get dropped.
    dst[o++] = '?';
    while (*p && (*p & 0xC0) == 0x80) p++;
    if (*p && *p >= 0x80) p++;
  }

  dst[o] = '\0';
}

// Variant that folds "in place", back into the same buffer.
inline void foldInPlace(char* buf, size_t cap) {
  if (!buf || !cap) return;
  char tmp[256];
  size_t n = min(cap, sizeof(tmp));
  fold(buf, tmp, n);
  strncpy(buf, tmp, cap - 1);
  buf[cap - 1] = '\0';
}

}  // namespace Text
