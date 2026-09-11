// ---------------------------------------------------------------------
//  DeckArt.h  -  obal alba: stazeni, cache, dekodovani, barvy, pozadi
//
//  Jak vznika rozmazane pozadi:
//    JPEG se dekoduje dvakrat z tehoz bufferu. Jednou v meritku 1/8
//    (300x300 -> ~38x38 px), z ceho se udela mikro-nahled, dvakrat se
//    rozmaze boxfiltrem a pak se bilinearne roztahne pres celou
//    obrazovku. Rozmazani tedy nic nestoji - vznikne zdarma tim, ze se
//    zahazuji detaily uz pri dekodovani. Podruhe se dekoduje v meritku
//    1/2 na ostry ctverec obalu.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

namespace Art {

void begin();

// Zajisti, ze je v pameti obal s danym id (z LittleFS cache nebo ze site).
// Vraci true, kdyz je obal pripraveny k vykresleni.
bool load(const char* artId, const char* url);

// Uvolni JPEG z pameti a prepne se na nahradni paletu.
void unload();

bool        hasArt();
const char* currentId();

// Dominantni barva obalu a z ni odvozena barva pro zvyrazneni.
uint32_t dominant();
uint32_t accent();

// --- rezim pozadi ----------------------------------------------------
//  Ostatni stranky (pocasi, kurzy, hodiny) obal nemaji, ale chteji stejne
//  kresleni pozadi. Vypnutim "art" rezimu se misto rozmazaneho obalu
//  pouzije hladky svisly prechod mezi dvema zadanymi barvami.
void useArtBackground(bool on);
void setFlatPalette(uint32_t top, uint32_t bottom, uint32_t accent);

// --- pozadi ----------------------------------------------------------
void paintBackground();                                       // cela plocha
void paintRect(int16_t x, int16_t y, int16_t w, int16_t h);   // jen vyrez
void rowInto(uint16_t* dst, int16_t x, int16_t y, int16_t w); // radek do bufferu
uint16_t pixelAt(int16_t x, int16_t y);                       // jeden pixel

// --- obal ------------------------------------------------------------
bool drawCover(int16_t x, int16_t y, int16_t size);
bool drawCoverFull();      // pres celou obrazovku (orezane na vysku)

}  // namespace Art
