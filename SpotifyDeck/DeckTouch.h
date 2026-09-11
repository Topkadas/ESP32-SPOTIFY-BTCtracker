// ---------------------------------------------------------------------
//  DeckTouch.h  -  rezistivni dotyk XPT2046 + kalibrace
//
//  Panel je rezistivni, takze surove hodnoty ADC se lisi kus od kusu
//  a jeste k tomu zavisi na tom, jak je displej otoceny. Misto
//  napevno zadrátovanych konstant se proto dela dvoubodova kalibrace,
//  ktera si sama pozna i prohozeni os, a vysledek se ulozi do NVS.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

struct TouchEvent {
  bool    down      = false;   // prst je prave ted na displeji
  bool    pressed   = false;   // prave dosedl
  bool    released  = false;   // prave se zvedl
  bool    tap       = false;   // zvedl se, aniz by tahnul
  bool    longPress = false;   // drzi dyl nez TOUCH_LONGPRESS_MS (jednou)
  bool    dragging  = false;   // od stisku ujel vic nez TOUCH_DRAG_PX
  int16_t x = -1, y = -1;      // aktualni pozice v pixelech
  int16_t startX = -1, startY = -1;
};

namespace Touch {

void begin();

// Vola se v kazdem pruchodu loop(). Sam si hlida vzorkovaci periodu.
TouchEvent poll();

// Interaktivni kalibrace - nakresli dva terce a ceka na dotyk.
// Vraci true, kdyz se povedla, a rovnou ji ulozi do NVS.
bool calibrate();

bool hasCalibration();
void forgetCalibration();

// Otoceni displeje (1 nebo 3). Ulozi se do NVS a plati i po restartu.
uint8_t  rotation();
void     setRotation(uint8_t r);

// Kdy naposledy uzivatel neco udelal - pro ztlumeni podsviceni.
uint32_t lastActivityMs();

}  // namespace Touch
