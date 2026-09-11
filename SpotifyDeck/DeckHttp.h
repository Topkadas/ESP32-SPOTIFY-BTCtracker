// ---------------------------------------------------------------------
//  DeckHttp.h  -  one shared HTTP/HTTPS client for the whole device
//
//  An ESP32 without PSRAM has about 250 kB of free heap once WiFi is up,
//  and a single TLS session eats 40-50 kB of that. If every module
//  (Spotify, weather, prices) had its own WiFiClientSecure, memory would
//  run out. So there is one client for all of them - and because
//  HTTPClient with setReuse(true) does not check that the open
//  connection leads to the same host, the host switch is tracked here.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Http {

struct Result {
  int  code       = 0;    // HTTP status, or a negative value on a network error
  int  retryAfter = 0;    // seconds from the Retry-After header (0 = absent)
  bool ok() const { return code >= 200 && code < 300; }
};

// GET + JSON parsing. "filter" may be nullptr.
Result getJson(const String& url, JsonDocument& doc,
               const JsonDocument* filter = nullptr,
               const char* authHeader = nullptr);

// GET that returns the body as text (for responses that are not JSON).
Result getText(const String& url, String& out, size_t maxLen = 8192);

// PUT / POST / DELETE with no body - typically Spotify control commands.
Result command(const char* verb, const String& url, const char* authHeader);

// POST application/x-www-form-urlencoded + JSON response.
Result postForm(const String& url, const String& body, const char* authHeader,
                JsonDocument& doc, const JsonDocument* filter = nullptr);

// Downloads binary data into a freshly allocated buffer (caller frees).
// Returns the number of bytes, 0 on error.
size_t download(const String& url, uint8_t** buf, size_t maxLen);

// Closes the open TLS connection (frees ~45 kB of heap).
void releaseTls();

const char* lastError();

}  // namespace Http
