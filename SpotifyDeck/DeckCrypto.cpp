#include "DeckCrypto.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "DeckArt.h"
#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckLang.h"
#include "DeckDraw.h"
#include "DeckHttp.h"
#include "DeckIcons.h"
#include "DeckTft.h"

namespace {

constexpr int SERIES_MAX = 48;          // 48 hourly candles = 2 days

enum Fiat : uint8_t { FIAT_USD = 0, FIAT_EUR = 1, FIAT_CZK = 2 };

const char* FIAT_NAME[3] = {"USD", "EUR", "CZK"};

Fiat     g_fiat = FIAT_USD;
bool     g_valid = false;
float    g_price = 0;
float    g_change = 0;                  // % over 24 h
float    g_high = 0, g_low = 0;
float    g_series[SERIES_MAX];
int      g_seriesLen = 0;
float    g_usdCzk = 0;                  // the CNB rate
uint32_t g_czkFetchedAt = 0;
char     g_error[64] = {0};
char     g_updated[8] = {0};
uint32_t g_nextDelay = 1000;

Preferences prefs;

constexpr const char* BINANCE = "https://api.binance.com";
constexpr const char* CNB_URL =
    "https://www.cnb.cz/cs/financni-trhy/devizovy-trh/kurzy-devizoveho-trhu/"
    "kurzy-devizoveho-trhu/denni_kurz.txt";

// Binance has a BTCEUR pair but no BTCCZK - so CZK is derived from USD.
const char* symbolFor(Fiat f) { return (f == FIAT_EUR) ? "BTCEUR" : "BTCUSDT"; }

float conversion() {
  if (g_fiat == FIAT_CZK) return g_usdCzk > 0 ? g_usdCzk : 0;
  return 1.0f;
}

// The daily CNB rate is a plain text file:
//   USA|dolar|1|USD|21,456
bool fetchCzkRate() {
  String txt;
  Http::Result r = Http::getText(String(CNB_URL), txt, 12000);
  if (!r.ok()) return false;

  int pos = txt.indexOf("|USD|");
  if (pos < 0) return false;

  int lineEnd = txt.indexOf('\n', pos);
  String tail = txt.substring(pos + 5, lineEnd < 0 ? txt.length() : lineEnd);
  tail.trim();
  tail.replace(",", ".");              // the CNB uses a decimal comma
  float v = tail.toFloat();
  if (v <= 0) return false;

  g_usdCzk = v;
  g_czkFetchedAt = millis();
  LOGF("[btc] CNB rate: 1 USD = %.3f CZK\n", g_usdCzk);
  return true;
}

// The /api/v3/klines response is an array of arrays:
//   [[time,"open","high","low","close","volume",...], ...]
// Running that through ArduinoJson would cost tens of kB, so the fifth
// field ("close") of each candle is pulled straight out of the text.
int parseCloses(const String& json, float* out, int maxOut) {
  int n = 0, depth = 0, field = 0;
  bool inStr = false;
  String num;

  for (size_t i = 0; i < json.length() && n < maxOut; i++) {
    char c = json[i];

    if (inStr) {
      if (c == '"') {
        inStr = false;
        if (depth == 2 && field == 4) {
          out[n++] = num.toFloat();
          field = 99;                   // the rest of the candle is of no interest
        }
      } else {
        num += c;
      }
      continue;
    }

    switch (c) {
      case '[': depth++; if (depth == 2) field = 0; break;
      case ']': if (depth == 2) field = 0; depth--; break;
      case ',': if (depth == 2 && field != 99) field++; break;
      case '"': if (depth == 2) { inStr = true; num = ""; } break;
      default: break;
    }
  }
  return n;
}

void formatPrice(float v, char* out, size_t cap) {
  if (v <= 0) { snprintf(out, cap, "-"); return; }

  // Space as the thousands separator, so it stays readable even at half
  // the screen wide.
  long whole = (long)v;
  char raw[16];
  snprintf(raw, sizeof(raw), "%ld", whole);
  int len = strlen(raw);

  size_t o = 0;
  for (int i = 0; i < len && o + 2 < cap; i++) {
    out[o++] = raw[i];
    int rest = len - 1 - i;
    if (rest > 0 && rest % 3 == 0) out[o++] = ' ';
  }
  out[o] = '\0';
}

}  // namespace

// =====================================================================
void Crypto::begin() {
  prefs.begin("deck", true);
  g_fiat = (Fiat)prefs.getUChar("cfiat", FIAT_USD);
  prefs.end();
  if (g_fiat > FIAT_CZK) g_fiat = FIAT_USD;
}

bool Crypto::poll() {
  g_error[0] = '\0';

  if (g_fiat == FIAT_CZK &&
      (g_usdCzk <= 0 || millis() - g_czkFetchedAt > 6UL * 3600UL * 1000UL)) {
    if (!fetchCzkRate() && g_usdCzk <= 0) {
      snprintf(g_error, sizeof(g_error), "%s", T(S_BTC_CNB_FAIL));
      g_nextDelay = 60000;
      return false;
    }
  }

  const char* sym = symbolFor(g_fiat);

  // --- price and 24h statistics ---
  JsonDocument filter;
  filter["lastPrice"]         = true;
  filter["priceChangePercent"] = true;
  filter["highPrice"]         = true;
  filter["lowPrice"]          = true;

  JsonDocument doc;
  String url = String(BINANCE) + "/api/v3/ticker/24hr?symbol=" + sym;
  Http::Result r = Http::getJson(url, doc, &filter);
  if (!r.ok()) {
    snprintf(g_error, sizeof(g_error), T(S_BTC_HTTP_FMT), r.code);
    g_nextDelay = 60000;
    return false;
  }

  float k = conversion();
  g_price  = atof(doc["lastPrice"] | "0") * k;
  g_change = atof(doc["priceChangePercent"] | "0");
  g_high   = atof(doc["highPrice"] | "0") * k;
  g_low    = atof(doc["lowPrice"] | "0") * k;

  // --- the 48 hour chart ---
  String klines;
  String kurl = String(BINANCE) + "/api/v3/klines?symbol=" + sym +
                "&interval=1h&limit=" + String(SERIES_MAX);
  Http::Result kr = Http::getText(kurl, klines, 20000);
  if (kr.ok()) {
    g_seriesLen = parseCloses(klines, g_series, SERIES_MAX);
    for (int i = 0; i < g_seriesLen; i++) g_series[i] *= k;
  }

  struct tm tmNow;
  if (getLocalTime(&tmNow, 5)) strftime(g_updated, sizeof(g_updated), "%H:%M", &tmNow);

  g_valid = (g_price > 0);
  g_nextDelay = 60000;                  // a minute is plenty, the Binance limit is generous
  LOGF("[btc] %.2f %s (%.2f %%), %d chart points\n", g_price, FIAT_NAME[g_fiat],
       g_change, g_seriesLen);
  return g_valid;
}

uint32_t Crypto::nextPollDelay() { return g_nextDelay; }

// ---------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------
namespace {

constexpr int16_t CH_X = 14;
constexpr int16_t CH_Y = 130;
constexpr int16_t CH_W = SCREEN_W - 2 * CH_X;
constexpr int16_t CH_H = 68;

void applyPalette() {
  bool up = (g_change >= 0);
  uint32_t top = up ? Col::rgb(0x0D, 0x3A, 0x2A) : Col::rgb(0x3A, 0x12, 0x18);
  uint32_t bottom = Col::rgb(0x04, 0x06, 0x0A);
  uint32_t accent = up ? Col::rgb(0x3F, 0xD6, 0x7A) : Col::rgb(0xFF, 0x6B, 0x6B);

  Art::useArtBackground(false);
  Art::setFlatPalette(top, bottom, accent);
  Draw::refreshPalette();
}

void drawChart() {
  Draw::glass(CH_X - 4, CH_Y - 8, CH_W + 8, CH_H + 16, 12, 120);
  uint16_t bg = Draw::cPanel;

  if (g_seriesLen < 2) {
    Draw::text(CH_X, CH_Y + CH_H / 2 - 8, CH_W, 16, T(S_BTC_NOCHART),
               nullptr, 2, Draw::cText3, 0, TC_DATUM);
    return;
  }

  float lo = g_series[0], hi = g_series[0];
  for (int i = 1; i < g_seriesLen; i++) {
    lo = min(lo, g_series[i]);
    hi = max(hi, g_series[i]);
  }
  float span = hi - lo;
  if (span <= 0) span = 1;

  // The filled area under the curve - one vertical bar per column.
  uint16_t fill = Draw::blend(bg, Draw::cAccent, 58);
  int16_t prevX = -1, prevY = -1;

  for (int16_t px = 0; px < CH_W; px++) {
    float t = (float)px * (g_seriesLen - 1) / (float)(CH_W - 1);
    int i0 = (int)t;
    int i1 = min(i0 + 1, g_seriesLen - 1);
    float f = t - i0;
    float v = g_series[i0] + (g_series[i1] - g_series[i0]) * f;

    int16_t y = CH_Y + CH_H - (int16_t)((v - lo) * CH_H / span);
    y = constrain(y, CH_Y, (int16_t)(CH_Y + CH_H));

    tft.drawFastVLine(CH_X + px, y, CH_Y + CH_H - y, fill);

    if (prevX >= 0)
      tft.drawWideLine(prevX, prevY, CH_X + px, y, 2.0f, Draw::cAccent, bg);
    prevX = CH_X + px;
    prevY = y;
  }

  // Mark the last point with a dot.
  if (prevX >= 0) tft.fillSmoothCircle(prevX, prevY, 3, Draw::cText, bg);

  char loTxt[20], hiTxt[20];
  formatPrice(lo, loTxt, sizeof(loTxt));
  formatPrice(hi, hiTxt, sizeof(hiTxt));
  Draw::text(CH_X + 2, CH_Y - 6, 90, 14, hiTxt, nullptr, 1, Draw::cText3);
  Draw::text(CH_X + 2, CH_Y + CH_H - 6, 90, 14, loTxt, nullptr, 1, Draw::cText3);
}

void drawFooter() {
  const int16_t y = 204;
  Draw::glass(10, y, SCREEN_W - 20, 30, 10, 118);

  char hi[24], lo[24];
  formatPrice(g_high, hi, sizeof(hi));
  formatPrice(g_low, lo, sizeof(lo));

  char left[40], mid[40];
  snprintf(left, sizeof(left), "%s  %s", T(S_BTC_HIGH), hi);
  snprintf(mid, sizeof(mid), "%s  %s", T(S_BTC_LOW), lo);

  Draw::text(20, y + 7, 140, 16, left, nullptr, 2, Draw::cText2);
  Draw::text(166, y + 7, 100, 16, mid, nullptr, 2, Draw::cText2);
  Draw::text(SCREEN_W - 62, y + 7, 52, 16, g_updated, nullptr, 2,
             Draw::cText3, 0, MR_DATUM);
}

}  // namespace

void Crypto::draw(bool full) {
  if (full) {
    applyPalette();
    Art::paintBackground();
  }

  uint16_t bg = Art::pixelAt(SCREEN_W / 2, 60);

  Draw::text(14, 30, 120, 18, "BITCOIN", nullptr, 2, Draw::cText3);
  char pair[16];
  snprintf(pair, sizeof(pair), "BTC / %s", FIAT_NAME[g_fiat]);
  Draw::text(SCREEN_W - 110, 30, 96, 18, pair, nullptr, 2, Draw::cAccent, 0,
             MR_DATUM);

  if (!g_valid) {
    Draw::bigText(SCREEN_W / 2, 120,
                  g_error[0] ? g_error : T(S_BTC_LOADING), &FreeSans9pt7b,
                  Draw::cText2, MC_DATUM);
    return;
  }

  char price[24];
  formatPrice(g_price, price, sizeof(price));
  Draw::bigText(14, 52, price, &FreeSansBold24pt7b, Draw::cText, TL_DATUM);

  // The 24 h change as a colored chip
  bool up = (g_change >= 0);
  uint16_t cc = up ? Draw::cGood : Draw::cBad;
  char chg[16];
  snprintf(chg, sizeof(chg), "%.2f %%", fabsf(g_change));

  int16_t chipW = Draw::measure(chg, nullptr, 2) + 34;
  int16_t chipX = 14, chipY = 100;
  tft.fillSmoothRoundRect(chipX, chipY, chipW, 24, 12,
                          Draw::blend(bg, cc, 60), bg);
  Icons::trend(chipX + 14, chipY + 12, 6, up, cc, Draw::blend(bg, cc, 60));
  Draw::text(chipX + 24, chipY + 4, chipW - 30, 16, chg, nullptr, 2, cc);

  drawChart();
  drawFooter();
}

void Crypto::tick() {}

bool Crypto::handleTouch(const TouchEvent& ev) {
  if (!ev.released || !ev.tap) return false;

  // Top half = switch the currency, bottom half = force a refresh.
  if (ev.y < 128) {
    g_fiat = (Fiat)((g_fiat + 1) % 3);
    prefs.begin("deck", false);
    prefs.putUChar("cfiat", (uint8_t)g_fiat);
    prefs.end();
    g_valid = false;
  }
  g_nextDelay = 0;
  return true;
}
