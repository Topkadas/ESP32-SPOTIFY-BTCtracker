// ---------------------------------------------------------------------
//  DeckWeather.h  -  the weather page (Open-Meteo)
//
//  Open-Meteo is free and needs NO API key and no sign-up, which on a
//  device with no keyboard is exactly what we want. The place is entered
//  by name ("Jirny", "Praha") and resolved to coordinates through their
//  geocoding API; the result is stored in NVS, so it only happens when
//  the place changes.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckTouch.h"

namespace Weather {

void begin();

// Fetches the current weather + forecast. Returns true on success.
bool poll();

// How long until it makes sense to ask again (ms).
uint32_t nextPollDelay();

void draw(bool full);
void tick();
bool handleTouch(const TouchEvent& ev);

// Sets the place. The name is resolved to coordinates on the next poll().
void        setPlace(const char* name);
const char* place();

}  // namespace Weather
