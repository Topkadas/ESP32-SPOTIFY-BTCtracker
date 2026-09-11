// ---------------------------------------------------------------------
//  DeckWeather.h  -  stranka s pocasim (Open-Meteo)
//
//  Open-Meteo je zdarma a NEPOTREBUJE API klic ani registraci, coz je
//  pro zarizeni bez klavesnice presne to, co chceme. Misto se zadava
//  jmenem ("Jirny", "Praha") a prelozi se na souradnice pres jejich
//  geokodovaci API; vysledek se ulozi do NVS, takze se to dela jen
//  pri zmene.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckTouch.h"

namespace Weather {

void begin();

// Stahne aktualni pocasi + predpoved. Vraci true pri uspechu.
bool poll();

// Za jak dlouho ma smysl se zeptat znovu (ms).
uint32_t nextPollDelay();

void draw(bool full);
void tick();
bool handleTouch(const TouchEvent& ev);

// Nastaveni mista. Jmeno se prelozi na souradnice pri nejblizsim poll().
void        setPlace(const char* name);
const char* place();

}  // namespace Weather
