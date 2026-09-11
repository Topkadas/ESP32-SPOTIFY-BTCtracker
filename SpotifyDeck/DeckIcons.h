// ---------------------------------------------------------------------
//  DeckIcons.h  -  ikony kreslene z primitiv, ne z fontu
//
//  Vsechny funkce dostavaji STRED ikony a barvy uz v RGB565 (to je to,
//  co TFT_eSPI ocekava - RGB888 by se tise vykreslilo jako nesmysl).
//
//  Kresli se na plochu se znamou barvou pozadi, diky cemu jdou pouzit
//  vyhlazene primitivy (drawWideLine, drawSmoothArc, fillSmoothRoundRect)
//  a hrany nejsou zubate.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// Rodiny ikon pocasi (WMO kody se na ne mapuji v DeckWeather).
enum class WxIcon : uint8_t {
  Clear, PartCloud, Cloud, Fog, Drizzle, Rain, Snow, Showers, Thunder, Unknown
};

namespace Icons {

// "size" je polovina sirky ikony v pixelech (22 = velka, 11 = mala).
void weather(int16_t cx, int16_t cy, int16_t size, WxIcon ic, bool day,
             uint16_t fg, uint16_t accent, uint16_t bg);

// Sipka nahoru/dolu pro zmenu kurzu.
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
