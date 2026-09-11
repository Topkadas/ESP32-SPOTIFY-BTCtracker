// ---------------------------------------------------------------------
//  DeckHttp.h  -  jeden sdileny HTTP/HTTPS klient pro cele zarizeni
//
//  ESP32 bez PSRAM ma po nabehnuti WiFi kolem 250 kB volne haldy a jedna
//  TLS relace z toho ukousne 40-50 kB. Kdyby mel kazdy modul (Spotify,
//  pocasi, kurzy) vlastni WiFiClientSecure, dojde pamet. Proto je tu
//  jeden klient pro vsechny - a protoze HTTPClient pri setReuse(true)
//  nekontroluje, jestli otevrene spojeni vede na stejny host, hlida se
//  prepnuti hostitele tady.
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Http {

struct Result {
  int  code       = 0;    // HTTP status, nebo zaporna hodnota pri chybe site
  int  retryAfter = 0;    // sekundy z hlavicky Retry-After (0 = nebyla)
  bool ok() const { return code >= 200 && code < 300; }
};

// GET + rozparsovani JSON. "filter" muze byt nullptr.
Result getJson(const String& url, JsonDocument& doc,
               const JsonDocument* filter = nullptr,
               const char* authHeader = nullptr);

// GET, ktery vrati telo jako text (pro odpovedi, co nejsou JSON).
Result getText(const String& url, String& out, size_t maxLen = 8192);

// PUT / POST / DELETE bez tela - typicky ovladaci prikazy Spotify.
Result command(const char* verb, const String& url, const char* authHeader);

// POST application/x-www-form-urlencoded + JSON odpoved.
Result postForm(const String& url, const String& body, const char* authHeader,
                JsonDocument& doc, const JsonDocument* filter = nullptr);

// Stazeni binarnich dat do cerstve alokovaneho bufferu (volajici uvolni).
// Vraci pocet bajtu, 0 pri chybe.
size_t download(const String& url, uint8_t** buf, size_t maxLen);

// Zavre otevrene TLS spojeni (uvolni ~45 kB haldy).
void releaseTls();

const char* lastError();

}  // namespace Http
