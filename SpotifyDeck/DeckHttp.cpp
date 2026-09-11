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

// Pulls the host out of a URL (everything between "//" and the first "/").
String hostOf(const String& url) {
  int a = url.indexOf("//");
  if (a < 0) return String();
  a += 2;
  int b = url.indexOf('/', a);
  return (b < 0) ? url.substring(a) : url.substring(a, b);
}

bool isHttps(const String& url) { return url.startsWith("https://"); }

// Throws away the open TLS session, so the next request starts from a
// fresh handshake. Cheap insurance whenever a connection is suspect.
void dropTls() {
  if (tlsReady) {
    tls.stop();
    tlsHost = "";
  }
}

// Every request MUST end here, never with a bare http.end().
//
// The keep-alive connection is shared by every module, so whatever one
// request leaves unread in the socket becomes the next request's
// response. That really happens: an error reply (403, 429) carries a
// JSON body, and code that only looked at the status code used to walk
// away without reading it. The next poll then parsed those leftovers,
// saw "HTTP 200" with an empty body, and the display quietly stopped
// updating while playback control - which ignores the reply - kept
// working. So: drain first, end second.
void finish(HTTPClient& http, bool poisoned = false) {
  auto* s = http.getStreamPtr();   // NetworkClient* on core 3.x
  if (s) {
    uint32_t guard = millis() + 300;
    while (s->available() && (int32_t)(millis() - guard) < 0) s->read();
  }
  http.end();
  if (poisoned) dropTls();
}

bool prepare(HTTPClient& http, const String& url) {
  if (WiFi.status() != WL_CONNECTED) {
    setError("no WiFi");
    return false;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("SpotifyDeck/1.0 (ESP32)");

  if (isHttps(url)) {
    if (!tlsReady) {
      // No certificate validation - see the Security section of the
      // README. A root CA would cost more flash and need maintaining.
      tls.setInsecure();
      tls.setHandshakeTimeout(TLS_HANDSHAKE_S);
      tlsReady = true;
    }
    String h = hostOf(url);
    if (tlsHost != h) {      // different host - the connection cannot be reused
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
    if (r.code < 0) setError("network: %s", http.errorToString(r.code).c_str());
    // An error reply still has a body; a transport error means the
    // socket state is unknown. Both make the connection unsafe to reuse.
    finish(http, r.code < 0);
    return r;
  }

  String payload = http.getString();

  if (payload.isEmpty()) {
    // An empty body on a 200 is not "nothing to show". Either the read
    // failed (usually the heap ran out inside getString's reserve), or
    // we just parsed leftovers from a previous reply. Treating it as
    // valid is what made the screen freeze while the buttons still
    // worked, so: report it, and throw the connection away.
    finish(http, true);
    setError("empty body on HTTP 200");
    r.code = -102;
    return r;
  }

  finish(http);

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

  bool poisoned = (r.code < 0);

  if (r.code == 200) {
    int size = http.getSize();
    if (size > (int)maxLen) {
      setError("response too large (%d B)", size);
      r.code = -101;
      poisoned = true;             // we are not going to read it all
    } else {
      out = http.getString();
      // With chunked transfer the server does not announce a length
      // (size == -1), so the cap has to hold afterwards as well -
      // otherwise it could simply be bypassed.
      if (out.length() > maxLen) {
        setError("response too large (%u B)", (unsigned)out.length());
        out = "";
        r.code = -101;
      } else if (out.isEmpty()) {
        setError("empty body on HTTP 200");
        r.code = -102;
        poisoned = true;
      }
    }
  } else if (r.code < 0) {
    setError("network: %s", http.errorToString(r.code).c_str());
  }

  finish(http, poisoned);
  return r;
}

Http::Result Http::command(const char* verb, const String& url,
                           const char* authHeader) {
  Result r;
  HTTPClient http;
  if (!prepare(http, url)) { r.code = -1; return r; }

  collectRetryAfter(http);
  if (authHeader) http.addHeader("Authorization", authHeader);
  // HTTPClient sends no Content-Length when the body is empty, and
  // Spotify (behind its envoy proxy) sometimes never answers such a PUT.
  http.addHeader("Content-Length", "0");

  r.code = http.sendRequest(verb);
  if (r.code == 429) r.retryAfter = headerRetryAfter(http);
  if (r.code < 0) setError("network: %s", http.errorToString(r.code).c_str());

  // A 204 has no body, but 403 and 429 do - and leaving it unread is
  // exactly what poisons the next request on the shared connection.
  finish(http, r.code < 0);
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
  if (r.code < 0) setError("network: %s", http.errorToString(r.code).c_str());

  String payload = (r.code > 0) ? http.getString() : String();
  finish(http, r.code < 0);

  if (payload.length()) {
    DeserializationError err =
        filter ? deserializeJson(doc, payload,
                                 DeserializationOption::Filter(*filter))
               : deserializeJson(doc, payload);
    if (err && r.ok()) {
      setError("JSON: %s", err.c_str());
      r.code = -100;
    }
    // On an error code the body stays in doc on purpose - the caller
    // reads "error" / "error_description" out of it.
  }
  return r;
}

namespace {

// A Stream for HTTPClient::writeToStream to pour the response body into.
// It handles chunked transfer for us, with no manual parsing.
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
    finish(http, code < 0);
    setError("download: HTTP %d", code);
    return 0;
  }

  int declared = http.getSize();
  size_t cap = (declared > 0) ? (size_t)declared : maxLen;
  if (cap > maxLen) {
    finish(http, true);
    setError("file too large (%d B)", declared);
    return 0;
  }
  if (ESP.getMaxAllocHeap() < cap + 8192) {
    finish(http, true);
    setError("not enough memory (%u B)", (unsigned)ESP.getMaxAllocHeap());
    return 0;
  }

  uint8_t* p = (uint8_t*)malloc(cap);
  if (!p) { finish(http, true); setError("malloc failed"); return 0; }

  MemSink sink(p, cap);
  int written = http.writeToStream(&sink);
  finish(http);

  if (written <= 0 || sink.written() == 0 || sink.overflowed()) {
    free(p);
    setError("only %d B downloaded", written);
    return 0;
  }

  *buf = p;
  return sink.written();
}

void Http::releaseTls() { dropTls(); }

const char* Http::lastError() { return errorMsg; }
