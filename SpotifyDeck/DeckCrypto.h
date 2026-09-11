// ---------------------------------------------------------------------
//  DeckCrypto.h  -  the bitcoin price page
//
//  Data from the public Binance API (no key, no sign-up, generous limit).
//  Binance has no CZK pair, so it is computed via the daily CNB rate.
//  Tapping the price cycles the currency USD -> EUR -> CZK.
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
