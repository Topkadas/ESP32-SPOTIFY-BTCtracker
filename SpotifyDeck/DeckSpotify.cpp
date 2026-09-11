#include "DeckSpotify.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <base64.h>

#include "DeckConfig.h"
#include "DeckHttp.h"
#include "DeckText.h"
#include "secrets.h"

namespace {

Preferences prefs;

String   accessToken;
String   refreshToken;
String   authHeader;              // "Bearer ..." - rebuilt only when the token changes
uint32_t tokenExpiresAt = 0;      // millis()
uint32_t rateLimitUntil = 0;      // millis()
bool     rateLimited    = false;  // see timeReached() below
bool     reauthNeeded   = false;
char     errorMsg[80]   = {0};

constexpr const char* API      = "https://api.spotify.com";
constexpr const char* ACCOUNTS = "https://accounts.spotify.com";

void setError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(errorMsg, sizeof(errorMsg), fmt, ap);
  va_end(ap);
  LOGF("[spotify] %s\n", errorMsg);
}

// Strings from Spotify arrive as UTF-8, but the display only does ASCII -
// accents would come out as blank rectangles. Hence the folding.
void copyStr(char* dst, size_t cap, const char* src) {
  Text::fold(src, dst, cap);
}

// For identifiers (track id, artwork URL) - those are pure ASCII already
// and folding could only break them.
void copyRaw(char* dst, size_t cap, const char* src) {
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

// A millis()-safe comparison (survives the ~49 day wrap-around).
//
// Careful: it is only safe against a deadline that itself came from a
// recent millis(). Zero as "no deadline" does NOT work here - the
// expression then degenerates into (int32_t)millis() >= 0, which is
// false between day 24.9 and day 49.7 of uptime. So "no deadline" is
// tracked by a separate boolean.
inline bool timeReached(uint32_t deadline) {
  return (int32_t)(millis() - deadline) >= 0;
}

// Is the rate limit in force? If it has expired, clear the flag right away.
inline bool rateLimitActive() {
  if (!rateLimited) return false;
  if (timeReached(rateLimitUntil)) { rateLimited = false; return false; }
  return true;
}

String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

void noteRateLimit(const Http::Result& r) {
  int secs = r.retryAfter > 0 ? r.retryAfter : 30;
  rateLimitUntil = millis() + (uint32_t)secs * 1000UL;
  rateLimited    = true;
  setError("Rate limited, waiting %d s", secs);
}

int sendCommand(const char* verb, const String& path) {
  if (WiFi.status() != WL_CONNECTED)  return -1;
  if (rateLimitActive())              return 429;
  if (!Spotify::ensureToken())        return 401;

  Http::Result r = Http::command(verb, String(API) + path, authHeader.c_str());

  if (r.code == 429) noteRateLimit(r);
  if (r.code == 403) setError("403 - you need Spotify Premium");
  if (r.code == 401) { accessToken = ""; tokenExpiresAt = 0; }
  if (r.code < 0)    setError("%s", Http::lastError());

  LOGF("[spotify] %s %s -> %d\n", verb, path.c_str(), r.code);
  return r.code;
}

bool commandOk(int code) { return code >= 200 && code < 300; }

// Pulls the last segment out of the artwork URL - it is a content hash,
// so it works both as a cache key and as an "is this still the same
// cover?" test.
void extractArtId(const char* url, char* dst, size_t cap) {
  dst[0] = '\0';
  if (!url || !*url) return;
  const char* slash = strrchr(url, '/');
  copyRaw(dst, cap, slash ? slash + 1 : url);
}

// Picks the image closest to the requested width. Spotify sends 640/300/64,
// but for podcasts or old albums there may be fewer entries and width
// may be null.
void pickArtwork(JsonArrayConst images, int preferred, PlayerState& st) {
  const char* best = nullptr;
  int bestScore = INT32_MAX;
  int index = 0;

  for (JsonVariantConst img : images) {
    const char* url = img["url"] | (const char*)nullptr;
    if (url) {
      int w = img["width"] | 0;
      int score = (w > 0) ? abs(w - preferred) : 10000 + index;
      if (score < bestScore) { bestScore = score; best = url; }
    }
    index++;
  }

  if (best) {
    copyRaw(st.artUrl, sizeof(st.artUrl), best);
    extractArtId(best, st.artId, sizeof(st.artId));
  }
}

// "actions" comes over the wire as { "disallows": { "pausing": true } }
// and only carries the keys that are true. The documentation shows a flat
// variant - we handle both, and a missing key means "allowed".
bool allowed(JsonVariantConst actions, const char* key) {
  JsonVariantConst dis = actions["disallows"];
  if (!dis.isNull()) return !(dis[key] | false);
  if (actions[key].is<bool>()) return actions[key].as<bool>();
  return true;
}

}  // namespace

// =====================================================================
//  Public API
// =====================================================================
void Spotify::begin() {
  prefs.begin("deck", false);
  refreshToken = prefs.getString("rtok", "");
  prefs.end();

  if (refreshToken.isEmpty()) {
    refreshToken = SPOTIFY_REFRESH_TOKEN;
    LOGLN("[spotify] refresh token from secrets.h");
  } else {
    LOGLN("[spotify] refresh token from NVS");
  }

  accessToken    = "";
  authHeader     = "";
  tokenExpiresAt = 0;
  reauthNeeded   = false;
  errorMsg[0]    = '\0';
}

bool Spotify::ensureToken() {
  if (reauthNeeded) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!accessToken.isEmpty() && !timeReached(tokenExpiresAt)) return true;

  if (refreshToken.isEmpty() || refreshToken == "paste_refresh_token_here") {
    setError("No refresh token");
    reauthNeeded = true;
    return false;
  }

  String basic = "Basic " + base64::encode(String(SPOTIFY_CLIENT_ID) + ":" +
                                           String(SPOTIFY_CLIENT_SECRET));
  String body = "grant_type=refresh_token&refresh_token=" +
                urlEncode(refreshToken);

  JsonDocument filter;
  filter["access_token"]     = true;
  filter["expires_in"]       = true;
  filter["refresh_token"]    = true;
  filter["error"]            = true;
  filter["error_description"] = true;

  JsonDocument doc;
  Http::Result r = Http::postForm(String(ACCOUNTS) + "/api/token", body,
                                  basic.c_str(), doc, &filter);

  if (!r.ok()) {
    const char* err = doc["error"] | "";
    // invalid_grant = the token was revoked or is older than 6 months
    if (strcmp(err, "invalid_grant") == 0) {
      setError("Refresh token invalid - sign in again");
      reauthNeeded = true;
    } else if (r.code == 429) {
      noteRateLimit(r);
    } else {
      setError("Token HTTP %d %s", r.code, err);
    }
    return false;
  }

  const char* at = doc["access_token"] | (const char*)nullptr;
  if (!at) { setError("Response has no access_token"); return false; }

  accessToken = at;
  authHeader  = "Bearer " + accessToken;

  uint32_t ttl = doc["expires_in"] | 3600UL;
  if (ttl > TOKEN_EARLY_REFRESH_S) ttl -= TOKEN_EARLY_REFRESH_S;
  tokenExpiresAt = millis() + ttl * 1000UL;

  // On a refresh Spotify may hand back a new refresh token. When it does,
  // the old one stops working - so the new one has to be stored for good.
  const char* newRefresh = doc["refresh_token"] | (const char*)nullptr;
  if (newRefresh && *newRefresh && refreshToken != newRefresh) {
    refreshToken = newRefresh;
    prefs.begin("deck", false);
    prefs.putString("rtok", refreshToken);
    prefs.end();
    LOGLN("[spotify] refresh token rotated and saved to NVS");
  }

  LOGF("[spotify] new access token, valid for %lu s\n", (unsigned long)ttl);
  return true;
}

PollResult Spotify::poll(PlayerState& out) {
  if (WiFi.status() != WL_CONNECTED) return PollResult::NoNetwork;
  if (rateLimitActive())             return PollResult::RateLimited;
  if (!ensureToken())
    return reauthNeeded ? PollResult::AuthFailed : PollResult::Error;

  // The "market" parameter makes Spotify drop the list of available
  // markets (~1.5 kB per track) and replace it with a single boolean.
  String url = String(API) + "/v1/me/player?market=" + SPOTIFY_MARKET +
               "&additional_types=track,episode";

  // The filter cuts the response from several kB down to about 1 kB of
  // data we actually use.
  JsonDocument filter;
  filter["is_playing"]             = true;
  filter["progress_ms"]            = true;
  filter["shuffle_state"]          = true;
  filter["repeat_state"]           = true;
  filter["currently_playing_type"] = true;
  filter["actions"]                = true;
  filter["device"]["name"]            = true;
  filter["device"]["volume_percent"] = true;
  filter["device"]["supports_volume"] = true;
  filter["item"]["id"]                 = true;
  filter["item"]["name"]               = true;
  filter["item"]["type"]               = true;
  filter["item"]["duration_ms"]        = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["album"]["name"]      = true;
  filter["item"]["album"]["images"]    = true;
  filter["item"]["images"]             = true;   // podcast episode
  filter["item"]["show"]["name"]       = true;

  JsonDocument doc;
  Http::Result r = Http::getJson(url, doc, &filter, authHeader.c_str());

  if (r.code == 204) return PollResult::Idle;
  if (r.code == 401) {
    accessToken = "";
    tokenExpiresAt = 0;
    return PollResult::Error;        // next time we refresh the token and retry
  }
  if (r.code == 403) {
    setError("403 - Spotify Premium required");
    return PollResult::Forbidden;
  }
  if (r.code == 429) { noteRateLimit(r); return PollResult::RateLimited; }
  if (r.code != 200) {
    setError("%s", Http::lastError());
    return PollResult::Error;
  }

  PlayerState st;
  st.isPlaying = doc["is_playing"] | false;
  st.shuffle   = doc["shuffle_state"] | false;

  const char* rep = doc["repeat_state"] | "off";
  st.repeat = (!strcmp(rep, "track"))   ? RepeatMode::Track
            : (!strcmp(rep, "context")) ? RepeatMode::Context
                                        : RepeatMode::Off;

  JsonVariantConst dev = doc["device"];
  copyStr(st.deviceName, sizeof(st.deviceName), dev["name"] | "");
  st.supportsVolume = dev["supports_volume"] | false;
  st.volume = dev["volume_percent"].is<int>() ? dev["volume_percent"].as<int>() : -1;

  JsonVariantConst actions = doc["actions"];
  if (!actions.isNull()) {
    st.canNext    = allowed(actions, "skipping_next");
    st.canPrev    = allowed(actions, "skipping_prev");
    st.canSeek    = allowed(actions, "seeking");
    st.canPause   = allowed(actions, "pausing");
    st.canResume  = allowed(actions, "resuming");
    st.canShuffle = allowed(actions, "toggling_shuffle");
    st.canRepeat  = allowed(actions, "toggling_repeat_context");
  }

  JsonVariantConst item = doc["item"];
  if (item.isNull()) {          // an ad or an unknown content type
    out = st;
    return PollResult::Ok;
  }

  st.hasTrack   = true;
  st.progressMs = doc["progress_ms"] | 0UL;
  st.durationMs = item["duration_ms"] | 0UL;
  copyRaw(st.trackId, sizeof(st.trackId), item["id"] | "");
  copyStr(st.title,   sizeof(st.title),   item["name"] | "");

  const char* itemType = item["type"] | "track";
  st.isEpisode = (strcmp(itemType, "episode") == 0);

  if (st.isEpisode) {
    copyStr(st.artist, sizeof(st.artist), item["show"]["name"] | "");
    copyStr(st.album,  sizeof(st.album),  "Podcast");
    if (item["images"].is<JsonArrayConst>())
      pickArtwork(item["images"].as<JsonArrayConst>(), 300, st);
  } else {
    // Join up to three artists, so a "feat." does not get cut off.
    String artists;
    if (item["artists"].is<JsonArrayConst>()) {
      int n = 0;
      for (JsonVariantConst a : item["artists"].as<JsonArrayConst>()) {
        const char* nm = a["name"] | (const char*)nullptr;
        if (!nm || !*nm) continue;
        if (n) artists += ", ";
        artists += nm;
        if (++n == 3) break;
      }
    }
    copyStr(st.artist, sizeof(st.artist), artists.c_str());
    copyStr(st.album,  sizeof(st.album),  item["album"]["name"] | "");
    if (item["album"]["images"].is<JsonArrayConst>())
      pickArtwork(item["album"]["images"].as<JsonArrayConst>(), 300, st);
  }

  errorMsg[0] = '\0';
  out = st;
  return PollResult::Ok;
}

// ---------------------------------------------------------------------
//  Controls
// ---------------------------------------------------------------------
bool Spotify::play()     { return commandOk(sendCommand("PUT",  "/v1/me/player/play")); }
bool Spotify::pause()    { return commandOk(sendCommand("PUT",  "/v1/me/player/pause")); }
bool Spotify::next()     { return commandOk(sendCommand("POST", "/v1/me/player/next")); }
bool Spotify::previous() { return commandOk(sendCommand("POST", "/v1/me/player/previous")); }

bool Spotify::seek(uint32_t positionMs) {
  return commandOk(sendCommand(
      "PUT", "/v1/me/player/seek?position_ms=" + String(positionMs)));
}

bool Spotify::setVolume(int percent) {
  percent = constrain(percent, 0, 100);
  return commandOk(sendCommand(
      "PUT", "/v1/me/player/volume?volume_percent=" + String(percent)));
}

bool Spotify::setShuffle(bool on) {
  return commandOk(sendCommand(
      "PUT", String("/v1/me/player/shuffle?state=") + (on ? "true" : "false")));
}

bool Spotify::setRepeat(RepeatMode mode) {
  const char* s = (mode == RepeatMode::Track)   ? "track"
                : (mode == RepeatMode::Context) ? "context"
                                                : "off";
  return commandOk(sendCommand("PUT", String("/v1/me/player/repeat?state=") + s));
}

// ---------------------------------------------------------------------
//  Album art
// ---------------------------------------------------------------------
size_t Spotify::fetchArtwork(const char* url, uint8_t** outBuffer) {
  *outBuffer = nullptr;
  if (!url || !*url || WiFi.status() != WL_CONNECTED) return 0;

  // i.scdn.co serves the artwork over plain HTTP too. It is a public
  // image, and this saves the entire TLS buffer (~45 kB) plus the
  // handshake - on an ESP32 without PSRAM that is the difference between
  // "it works" and "out of memory".
  String plainUrl(url);
  if (plainUrl.startsWith("https://")) plainUrl = "http://" + plainUrl.substring(8);

  size_t len = Http::download(plainUrl, outBuffer, ART_MAX_BYTES);
  if (!len) {
    setError("Artwork: %s", Http::lastError());
    return 0;
  }
  LOGF("[spotify] artwork %u B\n", (unsigned)len);
  return len;
}

uint32_t    Spotify::backoffUntil() { return rateLimitUntil; }
const char* Spotify::lastError()    { return errorMsg; }
bool        Spotify::needsReauth()  { return reauthNeeded; }
