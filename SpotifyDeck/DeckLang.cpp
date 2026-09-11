#include "DeckLang.h"

#include <Preferences.h>

#include "DeckConfig.h"

namespace {

bool        g_english = (DEFAULT_LANG_EN != 0);
Preferences prefs;

// Sloupec 0 = cesky, sloupec 1 = anglicky.
// Poradi MUSI presne odpovidat enumu StrId v DeckLang.h.
const char* const TABLE[STR_COUNT][2] = {
    // --- start a stav site ---
    {"startuji...",              "starting up..."},
    {"Pripojuji WiFi",           "Connecting to WiFi"},
    {"hledam sit",               "looking for a network"},
    {"Nastav WiFi",              "Set up WiFi"},
    {"WiFi: %s / heslo: %s",     "WiFi: %s / password: %s"},
    {"WiFi se nepripojilo",      "Could not join WiFi"},
    {"restartuji",               "restarting"},
    {"Bez WiFi",                 "No WiFi"},
    {"zkousim se pripojit zpatky", "trying to reconnect"},
    {"Prihlasuji ke Spotify",    "Signing in to Spotify"},
    {"Oteviram portal",          "Opening portal"},
    {"pripoj se na WiFi SpotifyDeck", "join the SpotifyDeck WiFi"},
    {"Mesto pro pocasi",         "City for weather"},

    // --- Spotify ---
    {"Nic se neprehrava",        "Nothing is playing"},
    {"spust hudbu na telefonu nebo v pocitaci",
     "start playing on your phone or computer"},
    {"reklama nebo neznamy obsah", "ad or unknown content"},
    {"Chybi Premium",            "Premium required"},
    {"ovladani prehravani vyzaduje Spotify Premium",
     "playback control needs Spotify Premium"},
    {"Spotify nedostupne",       "Spotify unavailable"},
    {"Spotify: moc dotazu",      "Spotify: too many requests"},
    {"dalsi skladba",            "next track"},
    {"predchozi skladba",        "previous track"},
    {"hlasitost %d %%",          "volume %d %%"},
    {"zarizeni hlasitost neumi", "device has no volume control"},
    {"nelze menit",              "not adjustable"},
    {"Chyba",                    "Error"},

    // --- nove prihlaseni ---
    {"Spotify se odhlasilo",     "Spotify signed out"},
    {"Refresh token uz neplati.", "The refresh token has expired."},
    {"Spust znovu tools/get_token.py", "Run tools/get_token.py again"},
    {"a nahraj sketch s novym tokenem.",
     "and upload the sketch with the new token."},

    // --- pocasi ---
    {"nacitam pocasi...",        "loading weather..."},
    {"pocitove %d C   vlhkost %d%%", "feels %d C   humidity %d%%"},
    {"vitr %d km/h",             "wind %d km/h"},
    {"geokodovani: HTTP %d",     "geocoding: HTTP %d"},
    {"misto '%s' nenalezeno",    "place '%s' not found"},
    {"pocasi: HTTP %d",          "weather: HTTP %d"},

    // --- kurzy ---
    {"nacitam kurz...",          "loading price..."},
    {"graf neni k dispozici",    "chart unavailable"},
    {"24h max",                  "24h high"},
    {"24h min",                  "24h low"},
    {"kurz CNB se nepodarilo nacist", "could not load the CNB rate"},
    {"Binance: HTTP %d",         "Binance: HTTP %d"},

    // --- hodiny ---
    {"cekam na cas z internetu", "waiting for time from the internet"},

    // --- kalibrace ---
    {"Klepni presne do stredu terce", "Tap the centre of the target"},
    {"Kalibrace ulozena",        "Calibration saved"},
    {"Kalibrace dotyku",         "Touch calibration"},
    {"za chvili klepni na terce", "tap the targets in a moment"},

    // --- nastaveni ---
    {"Nastaveni",                "Settings"},
    {"klepni mimo = zpet",       "tap outside = back"},
    {"Kalibrace dotyku",         "Touch calibration"},
    {"klepni na dva terce",      "tap two targets"},
    {"Otocit displej",           "Rotate display"},
    {"kdyz je obraz vzhuru nohama", "if the image is upside down"},
    {"Nastavit WiFi a misto",    "WiFi and location"},
    {"otevre portal SpotifyDeck", "opens the SpotifyDeck portal"},
    {"Jazyk: cesky",             "Language: English"},
    {"prepnout na English",      "switch to cestina"},
    {"Smazat cache obalu",       "Clear artwork cache"},
    {"uvolni misto v pameti",    "frees up storage"},
    {"Restartovat",              "Restart"},
    {"",                         ""},
    {"mazu ulozene obaly...",    "clearing cached artwork..."},
};

// Kdyz se pridá retezec do enumu a zapomene se na radek v tabulce
// (nebo naopak), chceme to vedet uz pri prekladu, ne az na displeji.
static_assert(sizeof(TABLE) / sizeof(TABLE[0]) == STR_COUNT,
              "DeckLang: tabulka a enum StrId nesedi");

const char* const DAYS_CS[7] = {"Ne", "Po", "Ut", "St", "Ct", "Pa", "So"};
const char* const DAYS_EN[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

const char* const MONTHS_CS[12] = {"led", "uno", "bre", "dub", "kve", "cvn",
                                   "cvc", "srp", "zar", "rij", "lis", "pro"};
const char* const MONTHS_EN[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

}  // namespace

// =====================================================================
void Lang::begin() {
  prefs.begin("deck", true);
  g_english = prefs.getBool("lang_en", DEFAULT_LANG_EN != 0);
  prefs.end();
  LOGF("[lang] %s\n", g_english ? "English" : "cestina");
}

bool Lang::isEnglish() { return g_english; }

void Lang::set(bool english) {
  if (english == g_english) return;
  g_english = english;
  prefs.begin("deck", false);
  prefs.putBool("lang_en", g_english);
  prefs.end();
}

void Lang::toggle() { Lang::set(!g_english); }

const char* Lang::t(StrId id) {
  if (id >= STR_COUNT) return "";
  return TABLE[id][g_english ? 1 : 0];
}

const char* Lang::dayShort(int dow) {
  dow = ((dow % 7) + 7) % 7;
  return g_english ? DAYS_EN[dow] : DAYS_CS[dow];
}

const char* Lang::monthShort(int month) {
  month = ((month % 12) + 12) % 12;
  return g_english ? MONTHS_EN[month] : MONTHS_CS[month];
}

// Kody WMO. Drzi se to pohromade v jedne funkci, protoze prirazovat
// padesati kodum vlastni StrId by tabulku jen znepřehlednilo.
const char* Lang::weather(int code) {
  const bool en = g_english;
  switch (code) {
    case 0:  return en ? "clear"                : "jasno";
    case 1:  return en ? "mainly clear"         : "skoro jasno";
    case 2:  return en ? "partly cloudy"        : "polojasno";
    case 3:  return en ? "overcast"             : "zatazeno";
    case 45: return en ? "fog"                  : "mlha";
    case 48: return en ? "freezing fog"         : "namrzajici mlha";
    case 51: return en ? "light drizzle"        : "slabe mrholeni";
    case 53: return en ? "drizzle"              : "mrholeni";
    case 55: return en ? "heavy drizzle"        : "huste mrholeni";
    case 56:
    case 57: return en ? "freezing drizzle"     : "namrzajici mrholeni";
    case 61: return en ? "light rain"           : "slaby dest";
    case 63: return en ? "rain"                 : "dest";
    case 65: return en ? "heavy rain"           : "vydatny dest";
    case 66:
    case 67: return en ? "freezing rain"        : "mrznouci dest";
    case 71: return en ? "light snow"           : "slabe snezeni";
    case 73: return en ? "snow"                 : "snezeni";
    case 75: return en ? "heavy snow"           : "huste snezeni";
    case 77: return en ? "snow grains"          : "snehove krupky";
    case 80: return en ? "showers"              : "prehanky";
    case 81: return en ? "heavy showers"        : "silne prehanky";
    case 82: return en ? "violent showers"      : "pritrze";
    case 85: return en ? "snow showers"         : "snehove prehanky";
    case 86: return en ? "heavy snow showers"   : "silne snezeni";
    case 95: return en ? "thunderstorm"         : "bourka";
    case 96:
    case 99: return en ? "thunderstorm, hail"   : "bourka s krupobitim";
    default: return "-";
  }
}
