// ---------------------------------------------------------------------
//  DeckNet.h  -  WiFi, konfiguracni portal a cas
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Net {

// Callback, kterym si modul rika o vypsani stavu na displej.
// Diky nemu nemusi DeckNet vedet nic o DeckUi.
typedef void (*StatusFn)(const char* title, const char* detail);

// Pripoji se k ulozene siti. Kdyz zadna neni (nebo se to nepovede),
// otevre konfiguracni portal na AP "SpotifyDeck".
void begin(StatusFn status);

bool connected();

// Sila signalu prevedena na 0-4 "cárky".
uint8_t bars();

// Rucni otevreni portalu (dlouhy stisk v nastaveni).
void startPortal(StatusFn status);

// Portal umi krome WiFi nastavit i misto pro pocasi. Hodnota z formulare
// se da vyzvednout az po jeho zavreni.
void        setCityDefault(const char* city);
const char* cityFromPortal();   // prazdne = uzivatel nic nezmenil
void        clearCityFromPortal();

// Zapomene ulozenou sit.
void forgetWifi();

// Cas z NTP. Vraci false, dokud se cas nesynchronizoval.
bool timeValid();
bool formatClock(char* out, size_t cap);   // "14:32"

// Vola se v loop() - hlida vypadky spojeni a zkousi se pripojit zpet.
void tick();

}  // namespace Net
