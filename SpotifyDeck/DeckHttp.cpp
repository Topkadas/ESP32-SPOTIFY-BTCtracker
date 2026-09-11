#include "DeckHttp.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "DeckConfig.h"

namespace {

WiFiClientSecure tls;
WiFiClient       plain;
bool             tlsReady = false;
String           tlsHost;
char             errorMsg[96] = {0};

void setError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(errorMsg, sizeof(errorMsg), fmt, ap);
  va_end(ap);
  LOGF("[http] %s\n", errorMsg);
}

// Vytahne hostitele z URL (vse mezi "//" a prvnim "/").
String hostOf(const String& url) {
  int a = url.indexOf("//");
  if (a < 0) return String();
  a += 2;
  int b = url.indexOf('/', a);
  return (b < 0) ? url.substring(a) : url.substring(a, b);
}

bool isHttps(const String& url) { return url.startsWith("https://"); }

bool prepare(HTTPClient& http, const String& url) {
  if (WiFi.status() != WL_CONNECTED) {
    setError("bez WiFi");
    return false;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("SpotifyDeck/1.0 (ESP32)");

  if (isHttps(url)) {
    if (!tlsReady) {
      // Bez overovani certifikatu - viz README, kapitola Bezpecnost.
      // Root CA by zabral dalsi kB flash a musel by se udrzovat.
      tls.setInsecure();
      tls.setHandshakeTimeout(TLS_HANDSHAKE_S);
      tlsReady = true;
    }
    String h = hostOf(url);
    if (tlsHost != h) {      // jiny host -> spojeni znovu pouzit nelze
      tls.stop();
      tlsHost = h;
    }
    http.setReuse(true);
    return http.begin(tls, url);
  }

  http.setReuse(false);
  return http.begin(plain, url);
}

int headerRetryAfter(HTTPClient& http) {
  String v = http.header("Retry-After");
  v.trim();
  long s = v.length() ? v.toInt() : 0;
  if (s < 0) s = 0;
  if (s > 3600) s = 3600;
  return (int)s;
}

void collectRetryAfter(HTTPClient& http) {
  static const char* keys[] = {"Retry-After"};
  http.collectHeaders(keys, 1);
}

}  // namespace

// =====================================================================
Http::Result Http::getJson(const String& url, JsonDocument& doc,
                           const JsonDocument* filter,
                           const char* authHeader) {
  Result r;
  HTTPClient http;
  if (!prepare(http, url)) { r.code = -1; return r; }

  collectRetryAfter(http);
  if (authHeader) http.addHeader("Authorization", authHeader);

  r.code = http.GET();
  if (r.code == 429) r.retryAfter = headerRetryAfter(http);

  if (r.code != 200) {
    if (r.code < 0) setError("sit: %s", http.errorToString(r.code).c_str());
    http.end();
    return r;
  }

  String payload = http.getString();
  http.end();

  if (payload.isEmpty()) {
    // Prazdne telo u kodu 200 neni "nic k zobrazeni" - je to selhane
    // cteni (nejcasteji dosla halda pri reserve() v getString).
    // Rozlisit to je dulezite: jinak by se to tvarilo jako "nic nehraje".
    setError("prazdna odpoved u HTTP 200");
    r.code = -102;
    return r;
  }

  DeserializationError err =
      filter ? deserializeJson(doc, payload, DeserializationOption::Filter(*filter))
             : deserializeJson(doc, payload);
  if (err) {
    setError("JSON: %s", err.c_str());
    r.code = -100;
  }
  return r;
}

Http::Result Http::getText(const String& url, String& out, size_t maxLen) {
  Result r;
  out = "";
  HTTPClient http;
  if (!prepare(http, url)) { r.code = -1; return r; }

  collectRetryAfter(http);
  r.code = http.GET();
  if (r.code == 429) r.retryAfter = headerRetryAfter(http);

  if (r.code == 200) {
    int size = http.getSize();
    if (size > (int)maxLen) {
      setError("odpoved je moc velka (%d B)", size);
      r.code = -101;
    } else {
      out = http.getString();
      // Pri chunked prenosu server delku nehlasi (size == -1), takze
      // strop musi platit i zpetne - jinak by sel obejit.
      if (out.length() > maxLen) {
        setError("odpoved je moc velka (%u B)", (unsigned)out.length());
        out = "";
        r.code = -101;
      }
    }
  } else if (r.code < 0) {
    setError("sit: %s", http.errorToString(r.code).c_str());
  }
  http.end();
  return r;
}

Http::Result Http::command(const char* verb, const String& url,
                           const char* authHeader) {
  Result r;
  HTTPClient http;
  if (!prepare(http, url)) { r.code = -1; return r; }

  collectRetryAfter(http);
  if (authHeader) http.addHeader("Authorization", authHeader);
  // HTTPClient neposle Content-Length, kdyz je telo prazdne, a Spotify
  // (za proxy envoy) na takovy PUT obcas neodpovi vubec.
  http.addHeader("Content-Length", "0");

  r.code = http.sendRequest(verb);
  if (r.code == 429) r.retryAfter = headerRetryAfter(http);
  if (r.code < 0) setError("sit: %s", http.errorToString(r.code).c_str());

  http.end();
  return r;
}

Http::Result Http::postForm(const String& url, const String& body,
                            const char* authHeader, JsonDocument& doc,
                            const JsonDocument* filter) {
  Result r;
  HTTPClient http;
  if (!prepare(http, url)) { r.code = -1; return r; }

  collectRetryAfter(http);
  if (authHeader) http.addHeader("Authorization", authHeader);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  r.code = http.POST(body);
  if (r.code == 429) r.retryAfter = headerRetryAfter(http);
  if (r.code < 0) setError("sit: %s", http.errorToString(r.code).c_str());

  String payload = (http.getSize() != 0) ? http.getString() : String();
  http.end();

  if (payload.length()) {
    DeserializationError err =
        filter ? deserializeJson(doc, payload,
                                 DeserializationOption::Filter(*filter))
               : deserializeJson(doc, payload);
    if (err && r.ok()) {
      setError("JSON: %s", err.c_str());
      r.code = -100;
    }
    // Pri chybovem kodu necháme telo v doc - volajici si z nej precte
    // "error" / "error_description".
  }
  return r;
}

namespace {

// Stream, do ktereho HTTPClient::writeToStream sype telo odpovedi.
// Diky nemu zvladneme i chunked prenos bez rucniho parsovani.
class MemSink : public Stream {
 public:
  MemSink(uint8_t* buf, size_t cap) : _buf(buf), _cap(cap) {}
  size_t written() const   { return _len; }
  bool   overflowed() const { return _over; }

  size_t write(uint8_t b) override {
    if (_len >= _cap) { _over = true; return 0; }
    _buf[_len++] = b;
    return 1;
  }
  size_t write(const uint8_t* data, size_t size) override {
    if (_len + size > _cap) { size = _cap - _len; _over = true; }
    if (size) { memcpy(_buf + _len, data, size); _len += size; }
    return size;
  }
  int  available() override { return 0; }
  int  read() override      { return -1; }
  int  peek() override      { return -1; }
  void flush() override     {}

 private:
  uint8_t* _buf;
  size_t   _cap;
  size_t   _len  = 0;
  bool     _over = false;
};

}  // namespace

size_t Http::download(const String& url, uint8_t** buf, size_t maxLen) {
  *buf = nullptr;
  HTTPClient http;
  if (!prepare(http, url)) return 0;

  int code = http.GET();
  if (code != 200) {
    http.end();
    setError("stahovani: HTTP %d", code);
    return 0;
  }

  int declared = http.getSize();
  size_t cap = (declared > 0) ? (size_t)declared : maxLen;
  if (cap > maxLen) {
    http.end();
    setError("soubor je moc velky (%d B)", declared);
    return 0;
  }
  if (ESP.getMaxAllocHeap() < cap + 8192) {
    http.end();
    setError("malo pameti (%u B)", (unsigned)ESP.getMaxAllocHeap());
    return 0;
  }

  uint8_t* p = (uint8_t*)malloc(cap);
  if (!p) { http.end(); setError("malloc selhal"); return 0; }

  MemSink sink(p, cap);
  int written = http.writeToStream(&sink);
  http.end();

  if (written <= 0 || sink.written() == 0 || sink.overflowed()) {
    free(p);
    setError("stazeno jen %d B", written);
    return 0;
  }

  *buf = p;
  return sink.written();
}

void Http::releaseTls() {
  if (tlsReady) { tls.stop(); tlsHost = ""; }
}

const char* Http::lastError() { return errorMsg; }
