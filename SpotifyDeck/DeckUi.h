// ---------------------------------------------------------------------
//  DeckUi.h  -  drawing
//
//  The whole screen is repainted only when the track changes. Everything
//  else (the progress bar moving, the marquee title, volume, the clock)
//  is drawn into small rectangles that pull their background from
//  DeckArt - so it does not flicker even over the blurred cover.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckSpotify.h"

// What is under the finger.
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

// --- screens -------------------------------------------------------
void splash();
void bootStatus(const char* title, const char* detail);
void nowPlaying(const PlayerState& st, bool full);
void idle(const char* title, const char* detail);
void error(const char* title, const char* detail);
void reauth();
void fullArt(const PlayerState& st);

// --- partial repaints -------------------------------------------
void updateProgress(uint32_t progressMs, uint32_t durationMs);
void updateControls(const PlayerState& st);
void updateVolume(int volume, bool supported);
void updateStatus();
void invalidateStatus();   // after a full-page repaint
void setPageDots(uint8_t count, uint8_t active);
void tickMarquee();

// --- touch -----------------------------------------------------------
Hit  hitTest(int16_t x, int16_t y);
void pressFeedback(Hit h, bool down);
int  valueFromX(Hit h, int16_t x);       // 0..100 for both Progress and Volume

// --- settings (a blocking modal screen after a long press) -------
enum class SettingsAction : uint8_t {
  Back, Calibrate, Rotate, WifiPortal, Language, ClearCache, Restart
};
SettingsAction runSettings();

// --- on-board RGB LED ------------------------------------------------
void setLed(uint32_t rgb888);

// --- backlight ------------------------------------------------------
// Before a large repaint the backlight is pulled down and brought back
// up afterwards. That turns the jerky reveal of the JPEG into a smooth
// cross-fade.
void dipBegin();
void dipEnd();

void setBacklight(uint16_t duty);
void tickBacklight(uint32_t lastActivity);
void wakeBacklight();

// --- odds and ends -------------------------------------------------------
void toast(const char* text);            // a short message at the bottom
void clearToast();

}  // namespace Ui
