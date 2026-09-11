// ---------------------------------------------------------------------
//  DeckDraw.h  -  sdilene kreslici naradi pro vsechny stranky
//
//  Drzi jeden sprite, pres ktery se skladaji radky textu (pozadi + text
//  mimo obrazovku, teprve pak jedno poslani na displej = zadne blikani),
//  paletu odvozenou z aktualniho pozadi a "sklenene" panely.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace Draw {

// Paleta - prepocitava se pri kazde zmene pozadi.
extern uint16_t cText;    // hlavni text (bily)
extern uint16_t cText2;   // druhotny
extern uint16_t cText3;   // tercialni / popisky
extern uint16_t cAccent;  // barva odvozena z obalu alba nebo ze stranky
extern uint16_t cPanel;   // barva sklenene listy
extern uint16_t cPanelHi; // zvyraznena varianta
extern uint16_t cGood;    // zelena (rust, ok)
extern uint16_t cBad;     // cervena (pokles, chyba)

bool begin();             // vytvori sprite; false = nedostatek pameti
void refreshPalette();    // dopocita barvy z aktualniho pozadi

// Jeden radek textu vykresleny pres pozadi z DeckArt.
//   builtin = 0 -> pouzije se freeFont, jinak vestaveny font cislo N
//   offset     -> posun textu doleva (bezici nazev)
void text(int16_t x, int16_t y, int16_t w, int16_t h, const char* s,
          const GFXfont* freeFont, uint8_t builtin, uint16_t color,
          int16_t offset = 0, uint8_t datum = ML_DATUM);

// Sirka textu v danem fontu.
int16_t measure(const char* s, const GFXfont* freeFont, uint8_t builtin);

// "Sklo" - pozadi pod obdelnikem se ztmavi misto prekryti barvou,
// takze pod listou je porad videt obal alba.
void glass(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
           uint8_t darken);

uint16_t blend(uint16_t a, uint16_t b, uint8_t t);

// Text primo na displej (pro velke nadpisy, kde sprite nestaci).
void bigText(int16_t x, int16_t y, const char* s, const GFXfont* f,
             uint16_t color, uint8_t datum);

// Krouzek stupnu za teplotou (znak ° ve fontech neni).
void degree(int16_t x, int16_t y, int16_t r, uint16_t color, uint16_t bg);

}  // namespace Draw
