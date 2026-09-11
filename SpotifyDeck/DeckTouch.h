// ---------------------------------------------------------------------
//  DeckTouch.h  -  resistive XPT2046 touch + calibration
//
//  The panel is resistive, so the raw ADC values differ from unit to
//  unit, and on top of that they depend on how the display is rotated.
//  Instead of hard-wired constants there is a two-point calibration that
//  also spots swapped axes by itself, and the result is saved to NVS.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

struct TouchEvent {
  bool    down      = false;   // the finger is on the display right now
  bool    pressed   = false;   // it just went down
  bool    released  = false;   // it just lifted
  bool    tap       = false;   // lifted without dragging
  bool    longPress = false;   // held longer than TOUCH_LONGPRESS_MS (once)
  bool    dragging  = false;   // moved more than TOUCH_DRAG_PX since the press
  int16_t x = -1, y = -1;      // current position in pixels
  int16_t startX = -1, startY = -1;
};

namespace Touch {

void begin();

// Called on every pass through loop(). It paces its own sampling.
TouchEvent poll();

// Interactive calibration - draws two targets and waits for a touch.
// Returns true when it worked out, and saves it straight to NVS.
bool calibrate();

bool hasCalibration();
void forgetCalibration();

// Display rotation (1 or 3). Saved to NVS, so it survives a restart.
uint8_t  rotation();
void     setRotation(uint8_t r);

// When the user last did something - for dimming the backlight.
uint32_t lastActivityMs();

}  // namespace Touch
