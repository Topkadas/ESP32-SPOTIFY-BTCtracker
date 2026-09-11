// ---------------------------------------------------------------------
//  DeckDraw.h  -  shared drawing tools for every page
//
//  Holds the one sprite that text lines are composed through (background
//  + text off-screen, then a single push to the display = no flicker),
//  the palette derived from the current background, and the "glass"
//  panels.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace Draw {

// The palette - recomputed on every background change.
extern uint16_t cText;    // main text (white)
extern uint16_t cText2;   // secondary
extern uint16_t cText3;   // tertiary / labels
extern uint16_t cAccent;  // color taken from the album art or the page
extern uint16_t cPanel;   // color of the glass bar
extern uint16_t cPanelHi; // highlighted variant
extern uint16_t cGood;    // green (rising, ok)
extern uint16_t cBad;     // red (falling, error)

bool begin();             // creates the sprite; false = out of memory
void refreshPalette();    // recomputes the colors from the current background

// One line of text drawn over the background from DeckArt.
//   builtin = 0 -> freeFont is used, otherwise built-in font number N
//   offset     -> shifts the text to the left (marquee title)
//   bgColor    -> 0 = take the page background from DeckArt (the default).
//                 Otherwise this solid color is used. That is needed
//                 everywhere the text sits on a glass panel or inside a
//                 chart - otherwise it would repaint the untouched page
//                 background underneath itself and leave a light
//                 rectangle in the panel.
void text(int16_t x, int16_t y, int16_t w, int16_t h, const char* s,
          const GFXfont* freeFont, uint8_t builtin, uint16_t color,
          int16_t offset = 0, uint8_t datum = ML_DATUM,
          uint16_t bgColor = 0);

// Width of the text in the given font.
int16_t measure(const char* s, const GFXfont* freeFont, uint8_t builtin);

// "Glass" - the background under the rectangle is darkened instead of
// being covered with a color, so the album art shows through the bar.
void glass(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
           uint8_t darken);

uint16_t blend(uint16_t a, uint16_t b, uint8_t t);

// Text straight to the display (for big headings the sprite cannot hold).
void bigText(int16_t x, int16_t y, const char* s, const GFXfont* f,
             uint16_t color, uint8_t datum);

// The degree ring after a temperature (the fonts have no degree sign).
void degree(int16_t x, int16_t y, int16_t r, uint16_t color, uint16_t bg);

}  // namespace Draw
