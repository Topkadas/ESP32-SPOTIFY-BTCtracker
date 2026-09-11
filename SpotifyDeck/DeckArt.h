// ---------------------------------------------------------------------
//  DeckArt.h  -  album art: download, cache, decode, colors, background
//
//  How the blurred background is made:
//    The JPEG is decoded twice out of the same buffer. Once at 1/8 scale
//    (300x300 -> ~38x38 px), which gives a micro thumbnail; that is box
//    blurred twice and then stretched bilinearly across the whole
//    screen. The blur therefore costs nothing - it comes for free from
//    the detail already thrown away during decoding. The second decode
//    is at 1/2 scale, for the sharp cover square.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Art {

void begin();

// Makes sure the art with the given id is in memory (from the LittleFS
// cache or off the network). Returns true when it is ready to draw.
bool load(const char* artId, const char* url);

// Frees the JPEG from memory and falls back to the substitute palette.
void unload();

bool        hasArt();
const char* currentId();

// The dominant color of the art and the accent derived from it.
uint32_t dominant();
uint32_t accent();

// --- background mode ----------------------------------------------------
//  The other pages (weather, prices, clock) have no artwork but want the
//  same background drawing. Turning "art" mode off swaps the blurred
//  cover for a smooth vertical gradient between two given colors.
void useArtBackground(bool on);
void setFlatPalette(uint32_t top, uint32_t bottom, uint32_t accent);

// --- background ----------------------------------------------------------
void paintBackground();                                       // the whole screen
void paintRect(int16_t x, int16_t y, int16_t w, int16_t h);   // just a slice
void rowInto(uint16_t* dst, int16_t x, int16_t y, int16_t w); // one row into a buffer
uint16_t pixelAt(int16_t x, int16_t y);                       // a single pixel

// --- cover ------------------------------------------------------------
bool drawCover(int16_t x, int16_t y, int16_t size);
bool drawCoverFull();      // full screen (cropped to height)

}  // namespace Art
