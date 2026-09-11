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

void say(const char* title, const char* detail) {
  if (g_status) g_status(title, detail);
  LOGF("[net] %s | %s\n", title, detail ? detail : "");
}

void startTime() {
  if (g_timeStarted) return;
  configTzTime(TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
  g_timeStarted = true;
}

// Parametr musi prezit cele trvani portalu, proto je staticky.
WiFiManagerParameter* g_cityParam = nullptr;

void configurePortal(WiFiManager& wm) {
  wm.setDebugOutput(false);
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

  WiFiManager wm;
  configurePortal(wm);

  if (!wm.autoConnect(AP_NAME, AP_PASSWORD)) {
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
    g_downSince = 0;
    startTime();
    return;
  }

  if (g_downSince == 0) {
    g_downSince = now;
    LOGLN("[net] spojeni spadlo, zkousim znovu");
    WiFi.reconnect();
    return;
  }

  // Po dvou minutach marneho pokouseni radeji cely stack restartovat.
  if (now - g_downSince > 120000UL) {
    LOGLN("[net] 2 minuty bez WiFi - restart");
    ESP.restart();
  }
}
