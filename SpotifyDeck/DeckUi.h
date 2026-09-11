// ---------------------------------------------------------------------
//  DeckUi.h  -  vykreslovani
//
//  Cela obrazovka se prekresluje jen pri zmene skladby. Vsechno ostatni
//  (posun progress baru, bezici nazev, hlasitost, hodiny) se kresli
//  do malych obdelniku, ktere si pozadi doplni z DeckArt - takze i nad
//  rozmazanym obalem to nebliká.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckSpotify.h"

// Co se nachazi pod prstem.
enum class Hit : uint8_t {
  None,
  Shuffle,
  Prev,
  PlayPause,
  Next,
  Repeat,
  Art,
  Progress,
  Volume,
  Status
};

namespace Ui {

void begin();

// --- obrazovky -------------------------------------------------------
void splash();
void bootStatus(const char* title, const char* detail);
void nowPlaying(const PlayerState& st, bool full);
void idle(const char* title, const char* detail);
void error(const char* title, const char* detail);
void reauth();
void fullArt(const PlayerState& st);

// --- castecne prekresleni -------------------------------------------
void updateProgress(uint32_t progressMs, uint32_t durationMs);
void updateControls(const PlayerState& st);
void updateVolume(int volume, bool supported);
void updateStatus();
void setPageDots(uint8_t count, uint8_t active);
void tickMarquee();

// --- dotyk -----------------------------------------------------------
Hit  hitTest(int16_t x, int16_t y);
void pressFeedback(Hit h, bool down);
int  valueFromX(Hit h, int16_t x);       // 0..100 pro Progress i Volume

// --- nastaveni (blokujici modalni obrazovka po dlouhem stisku) -------
enum class SettingsAction : uint8_t {
  Back, Calibrate, Rotate, WifiPortal, ClearCache, Restart
};
SettingsAction runSettings();

// --- RGB LED na desce ------------------------------------------------
void setLed(uint32_t rgb888);

// --- podsviceni ------------------------------------------------------
// Pred velkym prekreslenim se podsviceni stahne a po nem zase nabehne.
// Z trhaneho odkryvani JPEGu se tim stane plynuly prolinacka.
void dipBegin();
void dipEnd();

void setBacklight(uint16_t duty);
void tickBacklight(uint32_t lastActivity);
void wakeBacklight();

// --- drobnosti -------------------------------------------------------
void toast(const char* text);            // kratka hlaska dole
void clearToast();

}  // namespace Ui
