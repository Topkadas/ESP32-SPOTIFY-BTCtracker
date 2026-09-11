// ---------------------------------------------------------------------
//  DeckCrypto.h  -  stranka s kurzem bitcoinu
//
//  Data z verejneho Binance API (bez klice, bez registrace, stedry limit).
//  Kurz CZK Binance nema, takze se pocita pres dennni kurz CNB.
//  Klepnutim na cenu se prepina mena USD -> EUR -> CZK.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

#include "DeckTouch.h"

namespace Crypto {

void     begin();
bool     poll();
uint32_t nextPollDelay();

void draw(bool full);
void tick();
bool handleTouch(const TouchEvent& ev);

}  // namespace Crypto
