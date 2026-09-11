// ---------------------------------------------------------------------
//  DeckLang.h  -  Czech / English for the whole display UI
//
//  Every text the user gets to see lives in one place, in a two-column
//  table. It is switched in Settings -> Language and the choice is kept
//  in NVS, so it survives a restart.
//
//  Note: the display can only do ASCII (see DeckText.h), so neither the
//  Czech nor the English strings may contain accents.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

enum StrId : uint16_t {
  // --- boot and network state ---
  S_BOOTING,
  S_WIFI_CONNECTING,
  S_WIFI_SEARCHING,
  S_WIFI_SETUP,
  S_WIFI_CREDS,          // "WiFi: %s / password: %s"
  S_WIFI_FAILED,
  S_RESTARTING,
  S_NO_WIFI,
  S_RECONNECTING,
  S_SPOTIFY_SIGNIN,
  S_PORTAL_OPENING,
  S_PORTAL_JOIN,
  S_CITY_PARAM,          // field label in the WiFi portal

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
  S_VOLUME_FMT,          // "volume %d %%"
  S_NO_VOLUME,
  S_VOL_UNAVAILABLE,     // next to the speaker icon
  S_ERROR,

  // --- re-authentication ---
  S_REAUTH_TITLE,
  S_REAUTH_L1,
  S_REAUTH_L2,
  S_REAUTH_L3,

  // --- weather ---
  S_WX_LOADING,
  S_WX_FEELS_FMT,        // "feels %d C   humidity %d%%"
  S_WX_WIND_FMT,         // "wind %d km/h"
  S_WX_GEOCODE_FMT,      // "geocoding: HTTP %d"
  S_WX_NOTFOUND_FMT,     // "place '%s' not found"
  S_WX_HTTP_FMT,         // "weather: HTTP %d"

  // --- exchange rates ---
  S_BTC_LOADING,
  S_BTC_NOCHART,
  S_BTC_HIGH,
  S_BTC_LOW,
  S_BTC_CNB_FAIL,
  S_BTC_HTTP_FMT,        // "Binance: HTTP %d"

  // --- clock ---
  S_CLOCK_WAITING,

  // --- calibration ---
  S_CAL_PROMPT,
  S_CAL_SAVED,
  S_CAL_TITLE,
  S_CAL_SOON,

  // --- settings ---
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

// The translation in the currently selected language.
const char* t(StrId id);

// Short weekday (0 = Sunday) and month (0 = January) names.
const char* dayShort(int dow);
const char* monthShort(int month);

// Weather description for a WMO code.
const char* weather(int wmoCode);

}  // namespace Lang

// Shorthand, to keep the call sites short.
#define T(id) Lang::t(id)
