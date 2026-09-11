// ---------------------------------------------------------------------
//  DeckNet.h  -  WiFi, the config portal and time
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Net {

// The callback this module uses to ask for a status line on the display.
// Thanks to it DeckNet needs to know nothing about DeckUi.
typedef void (*StatusFn)(const char* title, const char* detail);

// Connects to the stored network. If there is none (or it fails), it
// opens the config portal on the "SpotifyDeck" AP.
void begin(StatusFn status);

bool connected();

// Signal strength mapped to 0-4 "bars".
uint8_t bars();

// Opening the portal by hand (long press in settings).
void startPortal(StatusFn status);

// Besides WiFi the portal can also set the place for the weather. The
// form value can only be picked up once the portal has closed.
void        setCityDefault(const char* city);
const char* cityFromPortal();   // empty = the user changed nothing
void        clearCityFromPortal();

// Forgets the stored network.
void forgetWifi();

// Time from NTP. Returns false until the time has synced.
bool timeValid();
bool formatClock(char* out, size_t cap);   // "14:32"

// Called from loop() - watches for dropouts and tries to reconnect.
void tick();

}  // namespace Net
