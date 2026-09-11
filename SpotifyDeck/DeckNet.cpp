#include "DeckNet.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "DeckConfig.h"
#include "DeckLang.h"

namespace {

Net::StatusFn g_status      = nullptr;
char          g_cityDefault[41] = "Jirny";
char          g_cityResult[41]  = {0};
bool          g_timeStarted = false;
uint32_t      g_lastCheck   = 0;
uint32_t      g_downSince   = 0;
uint32_t      g_retryAt     = 0;
uint32_t      g_backoffS    = 0;

void say(const char* title, const char* detail) {
  if (g_status) g_status(title, detail);
  LOGF("[net] %s | %s\n", title, detail ? detail : "");
}

// Vypise, co deska ve vzduchu opravdu slysi. Kdyz v seznamu neni ta
// sit, na kterou se ma pripojit, nema smysl resit heslo - bud je to
// 5GHz pasmo, skryte SSID, nebo je router moc daleko.
void scanAndLog() {
#if DECK_VERBOSE
  LOGLN("[net] skenuji site v dosahu...");
  int n = WiFi.scanNetworks(false, true);    // sync, vcetne skrytych
  if (n <= 0) {
    LOGLN("[net] nenalezena zadna sit (ESP32 umi jen 2,4 GHz)");
  } else {
    for (int i = 0; i < n; i++) {
      LOGF("[net]   %2d. %-28s  %4d dBm  kanal %2d  %s\n", i + 1,
           WiFi.SSID(i).length() ? WiFi.SSID(i).c_str() : "(skryte)",
           WiFi.RSSI(i), WiFi.channel(i),
           WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "otevrena" : "heslo");
    }
  }
  WiFi.scanDelete();
#endif
}

void startTime() {
  if (g_timeStarted) return;
  configTzTime(TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
  g_timeStarted = true;
}

// Parametr musi prezit cele trvani portalu, proto je staticky.
WiFiManagerParameter* g_cityParam = nullptr;

void configurePortal(WiFiManager& wm) {
  // Pri ladeni chceme videt, co WiFiManager dela - jinak je po
  // "Pripojuji WiFi" na Serialu ticho a neni poznat, kde to vazne.
  wm.setDebugOutput(DECK_VERBOSE ? true : false);
  wm.setDarkMode(true);
  wm.setTitle("SpotifyDeck");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(20);
  wm.setConnectRetries(2);
  static WiFiManagerParameter cityParam("city", T(S_CITY_PARAM),
                                        g_cityDefault, 40);
  cityParam.setValue(g_cityDefault, 40);
  g_cityParam = &cityParam;
  wm.addParameter(&cityParam);

  wm.setSaveParamsCallback([]() {
    if (!g_cityParam) return;
    strncpy(g_cityResult, g_cityParam->getValue(), sizeof(g_cityResult) - 1);
    g_cityResult[sizeof(g_cityResult) - 1] = '\0';
    LOGF("[net] portal nastavil misto: %s\n", g_cityResult);
  });

  wm.setAPCallback([](WiFiManager* mgr) {
    static char detail[96];
    snprintf(detail, sizeof(detail), T(S_WIFI_CREDS),
             mgr->getConfigPortalSSID().c_str(), AP_PASSWORD);
    say(T(S_WIFI_SETUP), detail);
  });
}

}  // namespace

void Net::begin(StatusFn status) {
  g_status = status;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);            // bez tohohle jsou HTTP dotazy trhane
  WiFi.setAutoReconnect(true);

  say(T(S_WIFI_CONNECTING),
      WiFi.SSID().length() ? WiFi.SSID().c_str() : T(S_WIFI_SEARCHING));

  scanAndLog();

  WiFiManager wm;
  configurePortal(wm);

#if WIFI_FORGET
  // Jednorazove zahozeni ulozenych udaju - kdyz se deska porad marne
  // pokousi o sit se spatnym heslem, je rychlejsi zacit od nuly.
  LOGLN("[net] WIFI_FORGET: mazu ulozene pristupove udaje");
  wm.resetSettings();
  delay(300);
#endif

  LOGF("[net] ulozena sit: '%s'\n", WiFi.SSID().c_str());
  LOGF("[net] spoustim autoConnect, AP '%s' / heslo '%s'\n",
       AP_NAME, AP_PASSWORD);

  bool ok = wm.autoConnect(AP_NAME, AP_PASSWORD);

  LOGF("[net] autoConnect -> %s (status %d)\n", ok ? "OK" : "SELHALO",
       (int)WiFi.status());

  if (!ok) {
    say(T(S_WIFI_FAILED), T(S_RESTARTING));
    delay(1500);
    ESP.restart();
  }

  LOGF("[net] pripojeno k %s, IP %s\n",
       WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  startTime();
}

void Net::setCityDefault(const char* city) {
  if (!city || !*city) return;
  strncpy(g_cityDefault, city, sizeof(g_cityDefault) - 1);
  g_cityDefault[sizeof(g_cityDefault) - 1] = '\0';
}

const char* Net::cityFromPortal()   { return g_cityResult; }
void        Net::clearCityFromPortal() { g_cityResult[0] = '\0'; }

bool Net::connected() { return WiFi.status() == WL_CONNECTED; }

uint8_t Net::bars() {
  if (!connected()) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -66) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

void Net::startPortal(StatusFn status) {
  if (status) g_status = status;
  WiFiManager wm;
  configurePortal(wm);
  wm.startConfigPortal(AP_NAME, AP_PASSWORD);
}

void Net::forgetWifi() {
  WiFiManager wm;
  wm.resetSettings();
}

bool Net::timeValid() {
  time_t now = time(nullptr);
  return now > 1700000000;          // cokoliv po roce 2023 = cas uz sedi
}

bool Net::formatClock(char* out, size_t cap) {
  if (!timeValid()) { if (cap) out[0] = '\0'; return false; }
  struct tm tmNow;
  if (!getLocalTime(&tmNow, 5)) { if (cap) out[0] = '\0'; return false; }
  strftime(out, cap, "%H:%M", &tmNow);
  return true;
}

void Net::tick() {
  uint32_t now = millis();
  if (now - g_lastCheck < 2000) return;
  g_lastCheck = now;

  if (connected()) {
    if (g_downSince) LOGLN("[net] WiFi zpatky");
    g_downSince = 0;
    g_backoffS  = 0;
    startTime();
    return;
  }

  if (g_downSince == 0) {
    g_downSince = now;
    g_retryAt   = now + 5000;
    LOGLN("[net] spojeni spadlo, zkousim znovu");
    WiFi.reconnect();
    return;
  }

  // Drive se tady po dvou minutach restartovala cela deska. To je ale
  // zbytecne tvrde - restart WiFi sam o sobe nic nespravi a uzivatel
  // prijde o rozdelanou obrazovku i o cas z NTP. Misto toho se jen
  // opakuje pokus o pripojeni, s rostouci prodlevou.
  if ((int32_t)(now - g_retryAt) >= 0) {
    uint32_t down = (now - g_downSince) / 1000;
    g_backoffS = min<uint32_t>(g_backoffS ? g_backoffS * 2 : 5, 120);
    g_retryAt  = now + g_backoffS * 1000UL;
    LOGF("[net] bez WiFi uz %lu s, dalsi pokus za %lu s\n",
         (unsigned long)down, (unsigned long)g_backoffS);
    WiFi.disconnect();
    WiFi.reconnect();
  }
}
