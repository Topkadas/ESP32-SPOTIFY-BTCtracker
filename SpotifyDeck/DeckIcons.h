// ---------------------------------------------------------------------
//  DeckIcons.h  -  icons drawn from primitives, not from a font
//
//  Every function takes the CENTER of the icon, and colors already in
//  RGB565 (that is what TFT_eSPI expects - RGB888 would silently draw
//  as nonsense).
//
//  They are drawn onto a surface whose background color is known, which
//  is what lets us use the antialiased primitives (drawWideLine,
//  drawSmoothArc, fillSmoothRoundRect) and keep the edges from jagging.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// Weather icon families (WMO codes are mapped onto them in DeckWeather).
enum class WxIcon : uint8_t {
  Clear, PartCloud, Cloud, Fog, Drizzle, Rain, Snow, Showers, Thunder, Unknown
};

namespace Icons {

// "size" is half the icon width in pixels (22 = large, 11 = small).
void weather(int16_t cx, int16_t cy, int16_t size, WxIcon ic, bool day,
             uint16_t fg, uint16_t accent, uint16_t bg);

// Up/down arrow for the price change.
void trend(int16_t cx, int16_t cy, int16_t size, bool up, uint16_t fg,
           uint16_t bg);


void play   (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void pause  (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void next   (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void prev   (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void shuffle(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void repeat (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg, bool single);
void volume (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg, uint8_t level);
void wifi   (int16_t cx, int16_t cy, uint16_t fg, uint16_t dim, uint8_t bars);
void spinner(int16_t cx, int16_t cy, int16_t r, uint16_t fg, uint16_t bg,
             uint8_t phase);
void tick   (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void cross  (int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);
void activeDot(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg);

}  // namespace Icons
