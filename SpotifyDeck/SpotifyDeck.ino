/* =====================================================================
 *  SpotifyDeck  -  ctyri stranky na jedne male desce
 *
 *  Deska:     ESP32-2432S028R ("Cheap Yellow Display"), 2.8" 320x240,
 *             rezistivni dotyk XPT2046
 *  Knihovny:  TFT_eSPI, TJpg_Decoder, XPT2046_Touchscreen, ArduinoJson,
 *             WiFiManager  (vse z Library Manageru)
 *
 *  Stranky (prepinaji se tahem prstu doleva/doprava):
 *    1. Spotify  - obal alba rozmazany pres celou plochu, ostry ctverec
 *                  obalu, nazev / interpret / album, prubeh s previjenim,
 *                  play/pause, dalsi, predchozi, shuffle, repeat, hlasitost
 *    2. Pocasi   - Open-Meteo, bez API klice, misto se nastavi ve WiFi
 *                  portalu (vychozi Jirny)
 *    3. Bitcoin  - kurz z Binance + graf za 48 hodin, USD / EUR / CZK
 *    4. Hodiny   - analogovy cifernik, klepnutim prepne na digitalni
 *
 *  Dlouhy stisk kdekoliv otevre nastaveni (kalibrace dotyku, otoceni
 *  displeje, WiFi portal, smazani cache obalu).
 *
 *  Pred nahranim:
 *    1. zkopiruj secrets.example.h na secrets.h a vypln ho
 *       (nebo spust z korene projektu: python tools/get_token.py)
 *    2. Nastroje -> Deska: "ESP32 Dev Module"
 *       Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 *
 *  MIT licence, viz LICENSE.
 * ===================================================================== */

#include <Arduino.h>
#include <LittleFS.h>

#include "DeckArt.h"
#include "DeckClock.h"
#include "DeckConfig.h"
#include "DeckCrypto.h"
#include "DeckDraw.h"
#include "DeckHttp.h"
#include "DeckLang.h"
#include "DeckNet.h"
#include "DeckSpotify.h"
#include "DeckTft.h"
#include "DeckTouch.h"
#include "DeckUi.h"
#include "DeckWeather.h"

// ---------------------------------------------------------------------
//  Stranky
// ---------------------------------------------------------------------
enum class Page : uint8_t { Spotify = 0, Weather, Crypto, Clock, COUNT };
constexpr uint8_t PAGE_N = (uint8_t)Page::COUNT;

Page     g_page = Page::Spotify;
uint32_t g_nextPoll[PAGE_N] = {0, 0, 0, 0};

// Podstavy stranky Spotify
enum class Sub : uint8_t { NowPlaying, Idle, Error, FullArt, Reauth };
Sub g_sub = Sub::Idle;

PlayerState g_state;
char     g_shownTrack[40] = {0};
char     g_shownArt[48]   = {0};
uint32_t g_lastPollAt = 0;
uint32_t g_backoff    = BACKOFF_START_MS;
bool     g_haveState  = false;

// Dotyk
Hit      g_pressedHit = Hit::None;
uint32_t g_lastCmdAt  = 0;
int      g_dragValue  = -1;

// ---------------------------------------------------------------------
//  Pomocne
// ---------------------------------------------------------------------
inline bool due(uint32_t deadline) { return (int32_t)(millis() - deadline) >= 0; }

// Progress se mezi dotazy dopocitava lokalne, takze bar plyne plynule
// i kdyz se Spotify pta jen jednou za tri vteriny.
uint32_t effectiveProgress() {
  if (!g_haveState || !g_state.hasTrack) return 0;
  uint32_t p = g_state.progressMs;
  if (g_state.isPlaying) p += millis() - g_lastPollAt;
  if (g_state.durationMs && p > g_state.durationMs) p = g_state.durationMs;
  return p;
}

void applyLed() {
#if FEAT_RGB_LED
  if (g_page == Page::Spotify &&
      (g_sub == Sub::NowPlaying || g_sub == Sub::FullArt))
    Ui::setLed(Art::dominant());
  else
    Ui::setLed(0x000000);
#endif
}

// ---------------------------------------------------------------------
//  Vykresleni aktualni stranky
// ---------------------------------------------------------------------
void drawSpotify(bool full) {
  switch (g_sub) {
    case Sub::NowPlaying: Ui::nowPlaying(g_state, full); break;
    case Sub::FullArt:    Ui::fullArt(g_state);          break;
    case Sub::Reauth:     Ui::reauth();                  break;
    case Sub::Error:
      Ui::error(T(S_SPOTIFY_DOWN), Spotify::lastError());
      break;
    case Sub::Idle:
    default:
      Ui::idle(T(S_NOTHING_PLAYING), T(S_START_MUSIC));
      break;
  }
  applyLed();
}

void drawPage(bool full) {
  if (full) Ui::dipBegin();

  switch (g_page) {
    case Page::Spotify:
      // Stranka Spotify si pozadi bere z obalu alba, ostatni maji
      // vlastni barevny prechod - proto se to tady prepina.
      Art::useArtBackground(true);
      Draw::refreshPalette();
      drawSpotify(full);
      break;
    case Page::Weather: Weather::draw(full); break;
    case Page::Crypto:  Crypto::draw(full);  break;
    case Page::Clock:   Clock::draw(full);   break;
    default: break;
  }

  Ui::setPageDots(PAGE_N, (uint8_t)g_page);
  Ui::updateStatus();

  if (full) Ui::dipEnd();
}

void switchPage(int delta) {
  if (g_page == Page::Clock) Clock::leave();

  int idx = ((int)g_page + delta + PAGE_N) % PAGE_N;
  g_page = (Page)idx;
  LOGF("[main] stranka -> %d\n", idx);

  if (g_page == Page::Clock) Clock::enter();

  Ui::setPageDots(PAGE_N, (uint8_t)g_page);
  drawPage(true);
  applyLed();
}

// ---------------------------------------------------------------------
//  Spotify - dotaz a stavy
// ---------------------------------------------------------------------
void showIdleSub(const char* detail) {
  if (g_sub == Sub::Idle) return;
  g_sub = Sub::Idle;
  g_shownTrack[0] = g_shownArt[0] = '\0';
  if (g_page == Page::Spotify) {
    Ui::idle(T(S_NOTHING_PLAYING), detail);
    applyLed();
  }
}

void showErrorSub(const char* title, const char* detail) {
  if (g_sub == Sub::Error) return;
  g_sub = Sub::Error;
  g_shownTrack[0] = g_shownArt[0] = '\0';
  if (g_page == Page::Spotify) {
    Ui::error(title, detail);
    applyLed();
  }
}

void pollSpotify() {
  PlayerState fresh;
  PollResult  res = Spotify::poll(fresh);

  switch (res) {
    case PollResult::Ok: {
      g_backoff    = BACKOFF_START_MS;
      g_lastPollAt = millis();
      g_haveState  = true;

      bool trackChanged = strcmp(g_shownTrack, fresh.trackId) != 0;
      bool artChanged   = strcmp(g_shownArt, fresh.artId) != 0;
      bool wasOther     = (g_sub != Sub::NowPlaying && g_sub != Sub::FullArt);

      if (!fresh.hasTrack) {
        g_state = fresh;
        showIdleSub(T(S_AD_OR_UNKNOWN));
        break;
      }

      g_state = fresh;

      if (artChanged) {
        strncpy(g_shownArt, fresh.artId, sizeof(g_shownArt) - 1);
        g_shownArt[sizeof(g_shownArt) - 1] = '\0';
        if (!Art::load(fresh.artId, fresh.artUrl)) {
          LOGLN("[main] obal se nepodarilo nacist");
          Art::unload();
        }
      }

      strncpy(g_shownTrack, fresh.trackId, sizeof(g_shownTrack) - 1);
      g_shownTrack[sizeof(g_shownTrack) - 1] = '\0';

      if (g_sub != Sub::FullArt) g_sub = Sub::NowPlaying;

      if (g_page == Page::Spotify) {
        Art::useArtBackground(true);
        if (g_sub == Sub::FullArt) {
          if (artChanged || trackChanged) Ui::fullArt(g_state);
        } else {
          bool full = trackChanged || artChanged || wasOther;
          if (full) Ui::dipBegin();
          Ui::nowPlaying(g_state, full);
          if (full) { Ui::setPageDots(PAGE_N, (uint8_t)g_page); Ui::dipEnd(); }
        }
        applyLed();
      }
      break;
    }

    case PollResult::Idle:
      g_backoff    = BACKOFF_START_MS;
      g_lastPollAt = millis();
      g_haveState  = false;
      g_state      = PlayerState();
      showIdleSub(T(S_START_MUSIC));
      break;

    case PollResult::AuthFailed:
      if (g_sub != Sub::Reauth) {
        g_sub = Sub::Reauth;
        if (g_page == Page::Spotify) { Ui::reauth(); applyLed(); }
      }
      break;

    case PollResult::Forbidden:
      showErrorSub(T(S_PREMIUM_TITLE), T(S_PREMIUM_DETAIL));
      break;

    case PollResult::RateLimited:
      Ui::toast(T(S_RATE_LIMITED));
      break;

    case PollResult::NoNetwork:
      showErrorSub(T(S_NO_WIFI), T(S_RECONNECTING));
      break;

    case PollResult::Error:
    default:
      LOGF("[main] poll chyba: %s\n", Spotify::lastError());
      g_backoff = min<uint32_t>(g_backoff * 2, BACKOFF_MAX_MS);
      break;
  }

  uint32_t wait;
  if (res == PollResult::Ok)
    wait = g_state.isPlaying ? POLL_PLAYING_MS : POLL_IDLE_MS;
  else if (res == PollResult::Idle)
    wait = POLL_IDLE_MS;
  else if (res == PollResult::RateLimited) {
    uint32_t until = Spotify::backoffUntil();
    wait = ((int32_t)(until - millis()) > 0) ? (until - millis()) + 500 : 5000;
  } else if (res == PollResult::AuthFailed) {
    wait = 60000;
  } else {
    wait = g_backoff;
  }

  g_nextPoll[(uint8_t)Page::Spotify] = millis() + wait;
}

// Po stisku tlacitka se vyplati zeptat drive, at obrazovka nelze pozadu.
void pollSoon() {
  g_nextPoll[(uint8_t)Page::Spotify] = millis() + POLL_AFTER_CMD_MS;
}

void pollActivePage() {
  uint8_t i = (uint8_t)g_page;
  if (!due(g_nextPoll[i]) || !Net::connected()) return;

  switch (g_page) {
    case Page::Spotify:
      pollSpotify();
      break;

    case Page::Weather:
      Weather::poll();
      g_nextPoll[i] = millis() + max<uint32_t>(Weather::nextPollDelay(), 1000);
      drawPage(true);
      break;

    case Page::Crypto:
      Crypto::poll();
      g_nextPoll[i] = millis() + max<uint32_t>(Crypto::nextPollDelay(), 1000);
      drawPage(true);
      break;

    case Page::Clock:
      g_nextPoll[i] = millis() + 60000;      // hodiny nic nestahuji
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------
//  Ovladani Spotify
// ---------------------------------------------------------------------
void runCommand(Hit h) {
  if (millis() - g_lastCmdAt < TOUCH_REPEAT_MS) return;
  g_lastCmdAt = millis();

  switch (h) {
    case Hit::PlayPause:
      // Optimisticky prehodime ikonu hned, at to nepusobi zaseknute.
      if (g_state.isPlaying) { Spotify::pause(); g_state.isPlaying = false; }
      else                   { Spotify::play();  g_state.isPlaying = true; }
      Ui::updateControls(g_state);
      break;

    case Hit::Next:
      Ui::toast(T(S_NEXT_TRACK));
      Spotify::next();
      break;

    case Hit::Prev:
      // Spotify chape "predchozi" jako skok na zacatek, kdyz uz skladba
      // chvili hraje - stejne jako mobilni aplikace.
      Ui::toast(T(S_PREV_TRACK));
      Spotify::previous();
      break;

    case Hit::Shuffle:
      g_state.shuffle = !g_state.shuffle;
      Spotify::setShuffle(g_state.shuffle);
      Ui::updateControls(g_state);
      break;

    case Hit::Repeat: {
      RepeatMode next = (g_state.repeat == RepeatMode::Off)     ? RepeatMode::Context
                      : (g_state.repeat == RepeatMode::Context) ? RepeatMode::Track
                                                                : RepeatMode::Off;
      g_state.repeat = next;
      Spotify::setRepeat(next);
      Ui::updateControls(g_state);
      break;
    }

    default:
      return;
  }
  pollSoon();
}

// ---------------------------------------------------------------------
//  Nastaveni
// ---------------------------------------------------------------------
void applyCityFromPortal() {
  const char* city = Net::cityFromPortal();
  if (city && *city && strcmp(city, Weather::place()) != 0) {
    Weather::setPlace(city);
    g_nextPoll[(uint8_t)Page::Weather] = millis();
    LOGF("[main] nove misto pro pocasi: %s\n", city);
  }
  Net::clearCityFromPortal();
}

void openSettings() {
  if (g_page == Page::Clock) Clock::leave();
  Ui::SettingsAction action = Ui::runSettings();

  switch (action) {
    case Ui::SettingsAction::Calibrate:
      Touch::calibrate();
      break;

    case Ui::SettingsAction::Rotate:
      Touch::setRotation(Touch::rotation() == 1 ? 3 : 1);
      tft.setRotation(Touch::rotation());
      Touch::calibrate();          // po otoceni uz stara kalibrace neplati
      break;

    case Ui::SettingsAction::WifiPortal:
      Net::setCityDefault(Weather::place());
      Ui::bootStatus(T(S_PORTAL_OPENING), T(S_PORTAL_JOIN));
      Net::startPortal(Ui::bootStatus);
      applyCityFromPortal();
      break;

    case Ui::SettingsAction::Language:
      // Prepnuti jazyka se projevi az pri prekresleni na konci funkce.
      Lang::toggle();
      break;

    case Ui::SettingsAction::ClearCache:
      Art::unload();
      Ui::bootStatus("Cache", T(S_CACHE_CLEARING));
      LittleFS.format();
      delay(400);
      ESP.restart();
      break;

    case Ui::SettingsAction::Restart:
      ESP.restart();
      break;

    case Ui::SettingsAction::Back:
    default:
      break;
  }

  g_shownTrack[0] = g_shownArt[0] = '\0';
  if (g_page == Page::Clock) Clock::enter();
  drawPage(true);
  g_nextPoll[(uint8_t)g_page] = millis();
}

// ---------------------------------------------------------------------
//  Dotyk
// ---------------------------------------------------------------------
void handleSpotifyRelease(const TouchEvent& ev) {
  Hit h = g_pressedHit;
  g_pressedHit = Hit::None;
  Ui::pressFeedback(h, false);

  switch (h) {
    case Hit::Shuffle:
    case Hit::Prev:
    case Hit::PlayPause:
    case Hit::Next:
    case Hit::Repeat:
      if (ev.tap) runCommand(h);
      break;

    case Hit::Art:
#if FEAT_FULLSCREEN_ART
      if (ev.tap && Art::hasArt()) {
        g_sub = Sub::FullArt;
        Ui::fullArt(g_state);
      }
#endif
      break;

    case Hit::Progress: {
#if FEAT_SEEK_BAR
      int v = (g_dragValue >= 0) ? g_dragValue
                                 : Ui::valueFromX(Hit::Progress, ev.x);
      if (v >= 0 && g_state.durationMs && g_state.canSeek) {
        uint32_t target = (uint32_t)((uint64_t)g_state.durationMs * v / 100);
        Spotify::seek(target);
        g_state.progressMs = target;
        g_lastPollAt = millis();
        pollSoon();
      }
#endif
      break;
    }

    case Hit::Volume: {
#if FEAT_VOLUME_SLIDER
      int v = (g_dragValue >= 0) ? g_dragValue
                                 : Ui::valueFromX(Hit::Volume, ev.x);
      if (v >= 0 && g_state.supportsVolume) {
        Spotify::setVolume(v);
        g_state.volume = v;
        char msg[24];
        snprintf(msg, sizeof(msg), T(S_VOLUME_FMT), v);
        Ui::toast(msg);
        pollSoon();
      } else if (!g_state.supportsVolume) {
        Ui::toast(T(S_NO_VOLUME));
      }
#endif
      break;
    }

    default:
      break;
  }
  g_dragValue = -1;
}

void handleTouch() {
  TouchEvent ev = Touch::poll();
  if (!ev.pressed && !ev.released && !ev.longPress && !ev.dragging) return;

  if (ev.pressed) Ui::wakeBacklight();

  if (ev.longPress) {
    g_pressedHit = Hit::None;
    openSettings();
    return;
  }

  // --- stisk -----------------------------------------------------------
  if (ev.pressed) {
    g_dragValue  = -1;
    g_pressedHit = Hit::None;
    if (g_page == Page::Spotify && g_sub == Sub::NowPlaying) {
      g_pressedHit = Ui::hitTest(ev.x, ev.y);
      Ui::pressFeedback(g_pressedHit, true);
    }
    return;
  }

  // --- tazeni po posuvnicich ------------------------------------------
  if (ev.down && ev.dragging && g_page == Page::Spotify) {
    if (g_pressedHit == Hit::Volume && g_state.supportsVolume) {
      g_dragValue = Ui::valueFromX(Hit::Volume, ev.x);
      Ui::updateVolume(g_dragValue, true);
    } else if (g_pressedHit == Hit::Progress && g_state.durationMs) {
      g_dragValue = Ui::valueFromX(Hit::Progress, ev.x);
      Ui::updateProgress(
          (uint32_t)((uint64_t)g_state.durationMs * g_dragValue / 100),
          g_state.durationMs);
    }
    return;
  }

  if (!ev.released) return;

  // --- prejeti prstem = zmena stranky ---------------------------------
  int dx = ev.x - ev.startX;
  int dy = ev.y - ev.startY;
  bool sliderCaptured =
      (g_pressedHit == Hit::Volume || g_pressedHit == Hit::Progress);

  if (!sliderCaptured && abs(dx) > 60 && abs(dx) > 2 * abs(dy)) {
    if (g_pressedHit != Hit::None) Ui::pressFeedback(g_pressedHit, false);
    g_pressedHit = Hit::None;
    switchPage(dx < 0 ? 1 : -1);      // tah doleva = dalsi stranka
    return;
  }

  // --- klepnuti na konkretni strance ----------------------------------
  switch (g_page) {
    case Page::Spotify:
      if (g_sub == Sub::FullArt) {
        if (ev.tap) {
          g_sub = Sub::NowPlaying;
          g_shownTrack[0] = '\0';
          drawPage(true);
        }
      } else if (g_sub == Sub::NowPlaying) {
        handleSpotifyRelease(ev);
      } else if (ev.tap) {
        g_nextPoll[(uint8_t)Page::Spotify] = millis();   // zkusit znovu
      }
      break;

    case Page::Weather:
      if (Weather::handleTouch(ev))
        g_nextPoll[(uint8_t)Page::Weather] = millis();
      break;

    case Page::Crypto:
      if (Crypto::handleTouch(ev))
        g_nextPoll[(uint8_t)Page::Crypto] = millis();
      break;

    case Page::Clock:
      Clock::handleTouch(ev);
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  LOGLN("\n=== SpotifyDeck ===");

  Lang::begin();           // jazyk prostredi nacteny z NVS
  Touch::begin();          // nacte otoceni z NVS jeste pred inicializaci TFT
  Ui::begin();
  Ui::splash();

  Art::begin();
  Weather::begin();
  Crypto::begin();
  Clock::begin();

  Net::setCityDefault(Weather::place());
  Net::begin(Ui::bootStatus);
  applyCityFromPortal();

  Ui::bootStatus(T(S_SPOTIFY_SIGNIN), "");
  Spotify::begin();
  if (!Spotify::ensureToken() && Spotify::needsReauth()) g_sub = Sub::Reauth;

  // Prvni spusteni bez kalibrace - nabidnout ji rovnou.
  if (!Touch::hasCalibration()) {
    Ui::bootStatus(T(S_CAL_TITLE), T(S_CAL_SOON));
    delay(1200);
    Touch::calibrate();
  }

  for (uint8_t i = 0; i < PAGE_N; i++) g_nextPoll[i] = millis();
  drawPage(true);

  LOGF("[main] volna pamet po startu: %u B\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  Net::tick();
  handleTouch();
  pollActivePage();

  switch (g_page) {
    case Page::Spotify:
      if (g_sub == Sub::NowPlaying) {
        Ui::tickMarquee();
        if (g_haveState && g_state.hasTrack)
          Ui::updateProgress(effectiveProgress(), g_state.durationMs);
      }
      break;

    case Page::Clock:
      Clock::tick();
      break;

    default:
      break;
  }

  Ui::updateStatus();
  Ui::tickBacklight(Touch::lastActivityMs());

#if DECK_SHOW_HEAP
  static uint32_t heapAt = 0;
  if (due(heapAt)) {
    heapAt = millis() + 5000;
    LOGF("[heap] free=%u max=%u\n",
         (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  }
#endif

  delay(4);
}
