// ---------------------------------------------------------------------
//  DeckLang.h  -  cestina / anglictina pro cele prostredi displeje
//
//  Vsechny texty, ktere uzivatel uvidi, jsou na jednom miste v tabulce
//  o dvou sloupcich. Prepina se v Nastaveni -> Jazyk a volba se uklada
//  do NVS, takze plati i po restartu.
//
//  Pozor: displej umi jen ASCII (viz DeckText.h), takze ani ceske,
//  ani anglicke retezce nesmi obsahovat diakritiku.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

enum StrId : uint16_t {
  // --- start a stav site ---
  S_BOOTING,
  S_WIFI_CONNECTING,
  S_WIFI_SEARCHING,
  S_WIFI_SETUP,
  S_WIFI_CREDS,          // "WiFi: %s / heslo: %s"
  S_WIFI_FAILED,
  S_RESTARTING,
  S_NO_WIFI,
  S_RECONNECTING,
  S_SPOTIFY_SIGNIN,
  S_PORTAL_OPENING,
  S_PORTAL_JOIN,
  S_CITY_PARAM,          // popisek pole ve WiFi portalu

  // --- Spotify ---
  S_NOTHING_PLAYING,
  S_START_MUSIC,
  S_AD_OR_UNKNOWN,
  S_PREMIUM_TITLE,
  S_PREMIUM_DETAIL,
  S_SPOTIFY_DOWN,
  S_RATE_LIMITED,
  S_NEXT_TRACK,
  S_PREV_TRACK,
  S_VOLUME_FMT,          // "hlasitost %d %%"
  S_NO_VOLUME,
  S_VOL_UNAVAILABLE,     // vedle ikony reproduktoru
  S_ERROR,

  // --- nove prihlaseni ---
  S_REAUTH_TITLE,
  S_REAUTH_L1,
  S_REAUTH_L2,
  S_REAUTH_L3,

  // --- pocasi ---
  S_WX_LOADING,
  S_WX_FEELS_FMT,        // "pocitove %d C   vlhkost %d%%"
  S_WX_WIND_FMT,         // "vitr %d km/h"
  S_WX_GEOCODE_FMT,      // "geokodovani: HTTP %d"
  S_WX_NOTFOUND_FMT,     // "misto '%s' nenalezeno"
  S_WX_HTTP_FMT,         // "pocasi: HTTP %d"

  // --- kurzy ---
  S_BTC_LOADING,
  S_BTC_NOCHART,
  S_BTC_HIGH,
  S_BTC_LOW,
  S_BTC_CNB_FAIL,
  S_BTC_HTTP_FMT,        // "Binance: HTTP %d"

  // --- hodiny ---
  S_CLOCK_WAITING,

  // --- kalibrace ---
  S_CAL_PROMPT,
  S_CAL_SAVED,
  S_CAL_TITLE,
  S_CAL_SOON,

  // --- nastaveni ---
  S_SETTINGS,
  S_BACK_HINT,
  S_MENU_CAL,       S_MENU_CAL_HINT,
  S_MENU_ROTATE,    S_MENU_ROTATE_HINT,
  S_MENU_WIFI,      S_MENU_WIFI_HINT,
  S_MENU_LANG,      S_MENU_LANG_HINT,
  S_MENU_CACHE,     S_MENU_CACHE_HINT,
  S_MENU_RESTART,   S_MENU_RESTART_HINT,
  S_CACHE_CLEARING,

  STR_COUNT
};

namespace Lang {

void begin();
bool isEnglish();
void set(bool english);
void toggle();

// Preklad podle aktualne zvoleneho jazyka.
const char* t(StrId id);

// Zkratky dne v tydnu (0 = nedele) a mesice (0 = leden).
const char* dayShort(int dow);
const char* monthShort(int month);

// Slovni popis pocasi podle kodu WMO.
const char* weather(int wmoCode);

}  // namespace Lang

// Zkratka, aby volani v kodu nebyla dlouha.
#define T(id) Lang::t(id)
