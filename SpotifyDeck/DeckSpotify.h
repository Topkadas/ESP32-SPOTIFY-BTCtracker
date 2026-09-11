// ---------------------------------------------------------------------
//  DeckSpotify.h  -  a hand-written Spotify Web API client
//
//  No external Spotify library. The reasons:
//    * full control over what gets parsed (a JSON filter -> a memory
//      footprint around 2 kB instead of tens of kB),
//    * correct handling of 204 / 401 / 403 / 429 including Retry-After,
//    * refresh token rotation (Spotify swaps it now and then) with the
//      new one saved to NVS,
//    * artwork fetched over plain HTTP, which saves the whole TLS buffer.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// The outcome of a single player-state request.
enum class PollResult : uint8_t {
  Ok,            // 200 + valid data
  Idle,          // 204 - nothing playing / no active device
  NoNetwork,     // WiFi is down or the connection failed
  AuthFailed,    // the refresh token is dead -> sign in again
  Forbidden,     // 403 - typically "Premium required"
  RateLimited,   // 429 - we wait until retryAfter
  Error          // anything else
};

enum class RepeatMode : uint8_t { Off = 0, Context = 1, Track = 2 };

struct PlayerState {
  bool       hasTrack      = false;
  bool       isPlaying     = false;
  bool       shuffle       = false;
  RepeatMode repeat        = RepeatMode::Off;

  int16_t    volume        = -1;      // -1 = unknown / the device has no volume
  bool       supportsVolume = false;

  uint32_t   progressMs    = 0;
  uint32_t   durationMs    = 0;

  // What the server allows right now (from "actions.disallows").
  bool canNext = true, canPrev = true, canSeek = true;
  bool canPause = true, canResume = true;
  bool canShuffle = true, canRepeat = true;

  char trackId[40]    = {0};
  char title[160]     = {0};
  char artist[160]    = {0};
  char album[96]      = {0};
  char deviceName[48] = {0};
  char artUrl[160]    = {0};
  char artId[48]      = {0};   // last URL segment = a stable cache key
  bool isEpisode      = false;
};

namespace Spotify {

// Loads the stored refresh token from NVS (falls back to the one in secrets.h).
void begin();

// Returns true if we hold a valid access token (refreshing it if needed).
bool ensureToken();

// One GET /v1/me/player request. Fills "out" on PollResult::Ok.
PollResult poll(PlayerState& out);

// Control commands. Return true on 2xx.
bool play();
bool pause();
bool next();
bool previous();
bool seek(uint32_t positionMs);
bool setVolume(int percent);
bool setShuffle(bool on);
bool setRepeat(RepeatMode mode);

// Downloads the artwork JPEG into a freshly allocated buffer (the caller
// does the free()). Returns the length in bytes, 0 on error.
size_t fetchArtwork(const char* url, uint8_t** outBuffer);

// Until when (millis) we must not send another request - set on a 429.
uint32_t backoffUntil();

// The last error message, for showing on the display.
const char* lastError();

// True when the refresh token is invalid and you have to sign in again.
bool needsReauth();

}  // namespace Spotify
