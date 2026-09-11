// ---------------------------------------------------------------------
//  DeckTft.h  -  jedina globalni instance displeje
//
//  TFT_eSPI drzi cely stav (rotaci, viewport, font) v objektu, takze
//  dava smysl mit ho jeden a sdilet ho. Definice je v DeckUi.cpp.
// ---------------------------------------------------------------------
#pragma once

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
