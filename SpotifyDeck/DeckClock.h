// ---------------------------------------------------------------------
//  DeckClock.h  -  analogove hodiny
//
//  Kresli se do spritu s 8bitovou barevnou hloubkou (190x190 = 36 kB) a
//  na displej se posilaji jednim pruchodem. Diky tomu rucicky neblikaji
//  a nemusi se resit mazani jejich predchozi pozice. Sprite se alokuje
//  az pri vstupu na stranku a pri odchodu se zase uvolni.
//
//  Klepnuti prepina mezi analogovym a digitalnim zobrazenim.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckTouch.h"

namespace Clock {

void begin();

void enter();      // alokuje sprite, nakresli pozadi
void leave();      // uvolni sprite

void draw(bool full);
void tick();       // posune rucicky, kdyz se zmenila vterina
bool handleTouch(const TouchEvent& ev);

}  // namespace Clock
