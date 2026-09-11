// ---------------------------------------------------------------------
//  DeckClock.h  -  analog clock
//
//  It is drawn into an 8-bit color depth sprite (190x190 = 36 kB) and
//  pushed to the display in a single pass. That way the hands do not
//  flicker and there is no need to erase their previous position. The
//  sprite is allocated only on entering the page and freed on leaving.
//
//  Tapping switches between the analog and the digital face.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckTouch.h"

namespace Clock {

void begin();

void enter();      // allocates the sprite, draws the background
void leave();      // frees the sprite

void draw(bool full);
void tick();       // moves the hands when the second changed
bool handleTouch(const TouchEvent& ev);

}  // namespace Clock
