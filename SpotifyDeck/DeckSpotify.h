// ---------------------------------------------------------------------
//  DeckSpotify.h  -  vlastni klient Spotify Web API
//
//  Zadna externi Spotify knihovna. Duvody:
//    * plna kontrola nad tim, co se parsuje (JSON filtr -> pametova stopa
//      kolem 2 kB misto desitek kB),
//    * spravne osetreni 204 / 401 / 403 / 429 vcetne Retry-After,
//    * rotace refresh tokenu (Spotify ho obcas vymeni) s ulozenim do NVS,
//    * stahovani obalu po obycejnem HTTP, cimz se usetri cely TLS buffer.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// Vysledek jednoho dotazu na stav prehravace.
enum class PollResult : uint8_t {
  Ok,            // 200 + platna data
  Idle,          // 204 - nic nehraje / zadne aktivni zarizeni
  NoNetwork,     // WiFi je dole nebo se nepodarilo pripojit
  AuthFailed,    // refresh token je mrtvy -> nutne nove prihlaseni
  Forbidden,     // 403 - typicky "Premium required"
  RateLimited,   // 429 - cekame do retryAfter
  Error          // cokoliv jineho
};

enum class RepeatMode : uint8_t { Off = 0, Context = 1, Track = 2 };

struct PlayerState {
  bool       hasTrack      = false;
  bool       isPlaying     = false;
  bool       shuffle       = false;
  RepeatMode repeat        = RepeatMode::Off;

  int16_t    volume        = -1;      // -1 = neznama / zarizeni ji neumi
  bool       supportsVolume = false;

  uint32_t   progressMs    = 0;
  uint32_t   durationMs    = 0;

  // Co server v tuhle chvili dovoli (z "actions.disallows").
  bool canNext = true, canPrev = true, canSeek = true;
  bool canPause = true, canResume = true;
  bool canShuffle = true, canRepeat = true;

  char trackId[40]    = {0};
  char title[160]     = {0};
  char artist[160]    = {0};
  char album[96]      = {0};
  char deviceName[48] = {0};
  char artUrl[160]    = {0};
  char artId[48]      = {0};   // posledni segment URL = stabilni klic do cache
  bool isEpisode      = false;
};

namespace Spotify {

// Nacte ulozeny refresh token z NVS (kdyz tam neni, vezme ten ze secrets.h).
void begin();

// Vrati true, pokud mame platny access token (pripadne ho obnovi).
bool ensureToken();

// Jeden dotaz na GET /v1/me/player. Pri PollResult::Ok naplni "out".
PollResult poll(PlayerState& out);

// Ovladaci prikazy. Vraci true pri 2xx.
bool play();
bool pause();
bool next();
bool previous();
bool seek(uint32_t positionMs);
bool setVolume(int percent);
bool setShuffle(bool on);
bool setRepeat(RepeatMode mode);

// Stahne JPEG obalu do cerstve alokovaneho bufferu (volajici dela free()).
// Vraci delku v bajtech, 0 pri chybe.
size_t fetchArtwork(const char* url, uint8_t** outBuffer);

// Do kdy (millis) nesmime posilat dalsi dotaz - vyplnuje se pri 429.
uint32_t backoffUntil();

// Posledni chybova hlaska pro zobrazeni na displeji.
const char* lastError();

// True, kdyz je refresh token neplatny a je nutne projit prihlasenim znovu.
bool needsReauth();

}  // namespace Spotify
