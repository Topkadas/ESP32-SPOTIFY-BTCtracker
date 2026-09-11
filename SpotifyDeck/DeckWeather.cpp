#include "DeckWeather.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "DeckArt.h"
#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckLang.h"
#include "DeckDraw.h"
#include "DeckHttp.h"
#include "DeckIcons.h"
#include "DeckNet.h"
#include "DeckText.h"
#include "DeckTft.h"

namespace {

struct Day {
  WxIcon  icon = WxIcon::Unknown;
  float   tmin = 0, tmax = 0;
  int     pop  = 0;          // pravdepodobnost srazek v %
  char    label[4] = {0};    // "Po", "Ut", ...
};

char   g_place[40]  = "Jirny";
float  g_lat = 0, g_lon = 0;
bool   g_located = false;

bool    g_valid   = false;
float   g_temp = 0, g_feels = 0;
int     g_humidity = 0;
float   g_wind = 0, g_precip = 0;
int     g_code = -1;
bool    g_isDay = true;
Day     g_days[3];
uint32_t g_fetchedAt = 0;
char    g_error[64] = {0};

uint32_t g_nextDelay = 1000;
bool     g_needGeocode = true;

Preferences prefs;

constexpr const char* GEO_API =
    "https://geocoding-api.open-meteo.com/v1/search";
constexpr const char* WX_API = "https://api.open-meteo.com/v1/forecast";

// --- WMO kody --------------------------------------------------------
WxIcon iconFor(int code) {
  switch (code) {
    case 0:  return WxIcon::Clear;
    case 1:
    case 2:  return WxIcon::PartCloud;
    case 3:  return WxIcon::Cloud;
    case 45:
    case 48: return WxIcon::Fog;
    case 51: case 53: case 55:
    case 56: case 57: return WxIcon::Drizzle;
    case 61: case 63: case 65:
    case 66: case 67: return WxIcon::Rain;
    case 71: case 73: case 75: case 77:
    case 85: case 86: return WxIcon::Snow;
    case 80: case 81: case 82: return WxIcon::Showers;
    case 95: case 96: case 99: return WxIcon::Thunder;
    default: return WxIcon::Unknown;
  }
}

// Slovni popis pocasi je v obou jazycich v DeckLang - viz Lang::weather().

// Z data "2026-09-12" udela zkratku dne v tydnu (Zellerova kongruence).
void weekdayOf(const char* iso, char* out, size_t cap) {
  out[0] = '\0';
  if (!iso || strlen(iso) < 10) return;
  int y = atoi(iso);
  int m = atoi(iso + 5);
  int d = atoi(iso + 8);
  if (m < 3) { m += 12; y--; }
  int k = y % 100, j = y / 100;
  int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
  int dow = (h + 6) % 7;               // 0 = nedele
  strncpy(out, Lang::dayShort(dow), cap - 1);
  out[cap - 1] = '\0';
}

String urlEscape(const char* s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  for (const uint8_t* p = (const uint8_t*)s; *p; p++) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      out += (char)*p;
    } else {
      out += '%';
      out += hex[*p >> 4];
      out += hex[*p & 0x0F];
    }
  }
  return out;
}

bool geocode() {
  String url = String(GEO_API) + "?name=" + urlEscape(g_place) +
               "&count=1&language=cs&format=json";

  JsonDocument filter;
  filter["results"][0]["latitude"]  = true;
  filter["results"][0]["longitude"] = true;
  filter["results"][0]["name"]      = true;
  filter["results"][0]["country_code"] = true;

  JsonDocument doc;
  Http::Result r = Http::getJson(url, doc, &filter);
  if (!r.ok()) {
    snprintf(g_error, sizeof(g_error), T(S_WX_GEOCODE_FMT), r.code);
    return false;
  }

  JsonVariantConst first = doc["results"][0];
  if (first.isNull()) {
    snprintf(g_error, sizeof(g_error), T(S_WX_NOTFOUND_FMT), g_place);
    return false;
  }

  g_lat = first["latitude"]  | 0.0f;
  g_lon = first["longitude"] | 0.0f;
  const char* nm = first["name"] | g_place;
  Text::fold(nm, g_place, sizeof(g_place));

  prefs.begin("deck", false);
  prefs.putFloat("wlat", g_lat);
  prefs.putFloat("wlon", g_lon);
  prefs.putString("wcity", g_place);
  prefs.end();

  g_located = true;
  g_needGeocode = false;
  LOGF("[wx] %s -> %.4f, %.4f\n", g_place, g_lat, g_lon);
  return true;
}

}  // namespace

// =====================================================================
void Weather::begin() {
  prefs.begin("deck", true);
  String city = prefs.getString("wcity", "");
  g_lat = prefs.getFloat("wlat", 0.0f);
  g_lon = prefs.getFloat("wlon", 0.0f);
  prefs.end();

  if (city.length()) {
    strncpy(g_place, city.c_str(), sizeof(g_place) - 1);
    g_place[sizeof(g_place) - 1] = '\0';
  }
  // Souradnice mame ulozene -> geokodovat znovu neni potreba.
  g_located = (g_lat != 0.0f || g_lon != 0.0f);
  g_needGeocode = !g_located;

  LOGF("[wx] misto: %s (%.4f, %.4f)\n", g_place, g_lat, g_lon);
}

void Weather::setPlace(const char* name) {
  if (!name || !*name) return;
  Text::fold(name, g_place, sizeof(g_place));
  g_needGeocode = true;
  g_located     = false;
  g_valid       = false;
  g_nextDelay   = 0;

  prefs.begin("deck", false);
  prefs.putString("wcity", g_place);
  prefs.remove("wlat");
  prefs.remove("wlon");
  prefs.end();
}

const char* Weather::place() { return g_place; }

bool Weather::poll() {
  g_error[0] = '\0';

  if (g_needGeocode && !geocode()) {
    g_nextDelay = 60000;
    return false;
  }
  if (!g_located) { g_nextDelay = 60000; return false; }

  char url[420];
  snprintf(url, sizeof(url),
           "%s?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
           "precipitation,weather_code,is_day,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
           "precipitation_probability_max"
           "&timezone=auto&forecast_days=4",
           WX_API, g_lat, g_lon);

  JsonDocument filter;
  filter["current"] = true;
  filter["daily"]   = true;

  JsonDocument doc;
  Http::Result r = Http::getJson(String(url), doc, &filter);
  if (!r.ok()) {
    snprintf(g_error, sizeof(g_error), T(S_WX_HTTP_FMT), r.code);
    g_nextDelay = 60000;
    return false;
  }

  JsonVariantConst cur = doc["current"];
  g_temp     = cur["temperature_2m"]      | 0.0f;
  g_feels    = cur["apparent_temperature"] | g_temp;
  g_humidity = cur["relative_humidity_2m"] | 0;
  g_wind     = cur["wind_speed_10m"]       | 0.0f;
  g_precip   = cur["precipitation"]        | 0.0f;
  g_code     = cur["weather_code"]         | -1;
  g_isDay    = (cur["is_day"] | 1) != 0;

  JsonVariantConst daily = doc["daily"];
  for (int i = 0; i < 3; i++) {
    int idx = i + 1;                        // 0 je dnesek, ukazujeme dalsi
    g_days[i].icon = iconFor(daily["weather_code"][idx] | -1);
    g_days[i].tmax = daily["temperature_2m_max"][idx] | 0.0f;
    g_days[i].tmin = daily["temperature_2m_min"][idx] | 0.0f;
    g_days[i].pop  = daily["precipitation_probability_max"][idx] | 0;
    weekdayOf(daily["time"][idx] | "", g_days[i].label,
              sizeof(g_days[i].label));
    if (!g_days[i].label[0]) snprintf(g_days[i].label, 4, "+%d", idx);
  }

  g_valid     = true;
  g_fetchedAt = millis();
  g_nextDelay = 10UL * 60UL * 1000UL;       // predpoved se meni pomalu
  LOGF("[wx] %.1f C, kod %d, %s\n", g_temp, g_code, Lang::weather(g_code));
  return true;
}

uint32_t Weather::nextPollDelay() { return g_nextDelay; }

// ---------------------------------------------------------------------
//  Vykresleni
// ---------------------------------------------------------------------
namespace {

// Pozadi stranky se ladi podle pocasi a denni doby.
void applyPalette() {
  uint32_t top, bottom, accent;

  if (!g_isDay) {
    top = Col::rgb(0x16, 0x1F, 0x3D); bottom = Col::rgb(0x05, 0x07, 0x11);
    accent = Col::rgb(0x9F, 0xB6, 0xFF);
  } else {
    switch (iconFor(g_code)) {
      case WxIcon::Clear:
        top = Col::rgb(0x1B, 0x5E, 0xA8); bottom = Col::rgb(0x07, 0x12, 0x22);
        accent = Col::rgb(0xFF, 0xC7, 0x3A); break;
      case WxIcon::PartCloud:
        top = Col::rgb(0x2A, 0x55, 0x86); bottom = Col::rgb(0x08, 0x11, 0x1E);
        accent = Col::rgb(0xFF, 0xD0, 0x66); break;
      case WxIcon::Rain:
      case WxIcon::Drizzle:
      case WxIcon::Showers:
        top = Col::rgb(0x25, 0x3B, 0x55); bottom = Col::rgb(0x06, 0x0C, 0x14);
        accent = Col::rgb(0x6E, 0xC6, 0xFF); break;
      case WxIcon::Snow:
        top = Col::rgb(0x3A, 0x4C, 0x62); bottom = Col::rgb(0x0A, 0x0E, 0x16);
        accent = Col::rgb(0xDC, 0xEC, 0xFF); break;
      case WxIcon::Thunder:
        top = Col::rgb(0x35, 0x2E, 0x52); bottom = Col::rgb(0x08, 0x06, 0x12);
        accent = Col::rgb(0xFF, 0xD8, 0x3A); break;
      case WxIcon::Fog:
        top = Col::rgb(0x3E, 0x45, 0x4E); bottom = Col::rgb(0x0C, 0x0E, 0x12);
        accent = Col::rgb(0xC8, 0xD2, 0xE0); break;
      default:
        top = Col::rgb(0x2C, 0x3C, 0x52); bottom = Col::rgb(0x07, 0x0A, 0x10);
        accent = Col::rgb(0x9F, 0xC3, 0xE8); break;
    }
  }

  Art::useArtBackground(false);
  Art::setFlatPalette(top, bottom, accent);
  Draw::refreshPalette();
}

void fmtTemp(float t, char* out, size_t cap) {
  snprintf(out, cap, "%d", (int)lroundf(t));
}

void drawForecast() {
  const int16_t y = 156;
  const int16_t h = 78;
  for (int i = 0; i < 3; i++) {
    int16_t x = 10 + i * 100;
    int16_t w = 96;
    Draw::glass(x, y, w, h, 12, 118);

    uint16_t bg = Draw::cPanel;
    // Den nahore pres celou sirku, pod nim ikona vlevo a cisla vpravo.
    // Ikona musi zustat mala (12 px) - pri vetsi uz se "21 / 12" v pisme 2
    // do zbytku karty nevejde a konec se orizne.
    Draw::text(x + 4, y + 3, w - 8, 15, g_days[i].label, nullptr, 2,
               Draw::cText2, 0, MC_DATUM);

    Icons::weather(x + 21, y + 44, 12, g_days[i].icon, true,
                   Draw::cText, Draw::cAccent, bg);

    char hi[8], lo[8];
    fmtTemp(g_days[i].tmax, hi, sizeof(hi));
    fmtTemp(g_days[i].tmin, lo, sizeof(lo));
    char line[20];
    snprintf(line, sizeof(line), "%s / %s", hi, lo);
    Draw::text(x + 36, y + 36, w - 40, 16, line, nullptr, 2, Draw::cText,
               0, ML_DATUM);

    if (g_days[i].pop > 0) {
      char pop[12];
      snprintf(pop, sizeof(pop), "%d%%", g_days[i].pop);
      Draw::text(x + 36, y + 54, w - 40, 14, pop, nullptr, 2, Draw::cAccent,
                 0, ML_DATUM);
    }
  }
}

}  // namespace

void Weather::draw(bool full) {
  if (full) {
    applyPalette();
    Art::paintBackground();
  }

  if (!g_valid) {
    Draw::bigText(SCREEN_W / 2, 110,
                  g_error[0] ? g_error : T(S_WX_LOADING),
                  &FreeSans9pt7b, Draw::cText2, MC_DATUM);
    Draw::bigText(SCREEN_W / 2, 140, g_place, &FreeSansBold12pt7b,
                  Draw::cText, MC_DATUM);
    return;
  }

  uint16_t bg = Art::pixelAt(56, 74);
  Icons::weather(56, 74, 30, iconFor(g_code), g_isDay, Draw::cText,
                 Draw::cAccent, bg);

  // Teplota se sklada ze tri kusu: cislo, krouzek stupnu a "C". Krouzek
  // musi jit az za skutecnou sirku cisla, jinak by u "-12" lezel na minusu.
  char t[8];
  fmtTemp(g_temp, t, sizeof(t));
  Draw::bigText(108, 42, t, &FreeSansBold24pt7b, Draw::cText, TL_DATUM);

  int16_t tw = Draw::measure(t, &FreeSansBold24pt7b, 0);
  int16_t degX = min<int16_t>(108 + tw + 10, (int16_t)(SCREEN_W - 36));
  Draw::degree(degX, 48, 5, Draw::cText, bg);
  Draw::bigText(degX + 10, 42, "C", &FreeSansBold12pt7b, Draw::cText2,
                TL_DATUM);

  Draw::text(108, 84, SCREEN_W - 122, 20, Lang::weather(g_code),
             &FreeSans9pt7b, 0, Draw::cAccent);
  Draw::text(108, 106, SCREEN_W - 122, 16, g_place, nullptr, 2, Draw::cText3);

  char detail[64];
  snprintf(detail, sizeof(detail), T(S_WX_FEELS_FMT),
           (int)lroundf(g_feels), g_humidity);
  Draw::text(14, 128, 196, 18, detail, nullptr, 2, Draw::cText2);

  char wind[40];
  snprintf(wind, sizeof(wind), T(S_WX_WIND_FMT), (int)lroundf(g_wind));
  Draw::text(212, 128, 94, 18, wind, nullptr, 2, Draw::cText2, 0, MR_DATUM);

  drawForecast();
}

void Weather::tick() {}

bool Weather::handleTouch(const TouchEvent& ev) {
  // Klepnuti kdekoliv = vynutit obnovu.
  if (ev.released && ev.tap) {
    g_nextDelay = 0;
    return true;
  }
  return false;
}
