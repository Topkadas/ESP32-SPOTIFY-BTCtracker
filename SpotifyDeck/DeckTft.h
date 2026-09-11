// ---------------------------------------------------------------------
//  DeckTft.h  -  the one global display instance
//
//  TFT_eSPI keeps its whole state (rotation, viewport, font) inside the
//  object, so it makes sense to have one and share it. The definition
//  lives in DeckUi.cpp.
// ---------------------------------------------------------------------
#pragma once

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
