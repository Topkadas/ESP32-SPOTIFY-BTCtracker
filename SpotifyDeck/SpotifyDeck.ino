/* =====================================================================
 *  SpotifyDeck  -  four pages on one small board
 *
 *  Board:     ESP32-2432S028R ("Cheap Yellow Display"), 2.8" 320x240,
 *             resistive XPT2046 touch
 *  Libraries: TFT_eSPI, TJpg_Decoder, XPT2046_Touchscreen, ArduinoJson,
 *             WiFiManager  (all from the Library Manager)
 *
 *  Pages (swipe left/right to switch):
 *    1. Spotify  - the album art blurred across the whole screen, the
 *                  sharp cover square, title / artist / album, a progress
 *                  bar you can seek on, play/pause, next, previous,
 *                  shuffle, repeat, volume
 *    2. Weather  - Open-Meteo, no API key, the place is set in the WiFi
 *                  portal (Jirny by default)
 *    3. Bitcoin  - the price from Binance + a 48 hour chart, USD/EUR/CZK
 *    4. Clock    - an analog dial, tap to switch to digital
 *
 *  A long press anywhere opens the settings (touch calibration, display
 *  rotation, the WiFi portal, clearing the artwork cache).
 *
 *  Before flashing:
 *    1. copy secrets.example.h to secrets.h and fill it in
 *       (or run this from the project root: python tools/get_token.py)
 *    2. Tools -> Board: "ESP32 Dev Module"
 *       Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 *
 *  MIT licence, see LICENSE.
 * ===================================================================== */

#include <Arduino.h>
#include <LittleFS.h>
#include <Wire.h>

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
//  Pages
// ---------------------------------------------------------------------
// The order here sets the order of the pages when you swipe and the
// order of the dots in the status bar. It can be rearranged freely -
// everything else refers to the pages by name, not by number.
enum class Page : uint8_t { Spotify = 0, Crypto, Weather, Clock, COUNT };
constexpr uint8_t PAGE_N = (uint8_t)Page::COUNT;

Page     g_page = Page::Spotify;
uint32_t g_nextPoll[PAGE_N] = {0, 0, 0, 0};

// Sub-states of the Spotify page
enum class Sub : uint8_t { NowPlaying, Idle, Error, FullArt, Reauth };
Sub g_sub = Sub::Idle;

PlayerState g_state;
char     g_shownTrack[40] = {0};
char     g_shownArt[48]   = {0};
uint32_t g_lastPollAt = 0;
uint32_t g_backoff    = BACKOFF_START_MS;
bool     g_haveState  = false;

// Touch
Hit      g_pressedHit = Hit::None;
uint32_t g_lastCmdAt  = 0;
int      g_dragValue  = -1;

// ---------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------
inline bool due(uint32_t deadline) { return (int32_t)(millis() - deadline) >= 0; }

// Between requests the progress is extrapolated locally, so the bar runs
// smoothly even though Spotify is only asked every three seconds.
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
//  Drawing the current page
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
      // The Spotify page takes its background from the album art, the
      // others have a color gradient of their own - which is what gets
      // switched here.
      Art::useArtBackground(true);
      Draw::refreshPalette();
      drawSpotify(full);
      break;
    case Page::Weather: Weather::draw(full); break;
    case Page::Crypto:  Crypto::draw(full);  break;
    case Page::Clock:   Clock::draw(full);   break;
    default: break;
  }

  // Repainting the whole page wiped the status bar too, so it has to be
  // forced to draw again - otherwise it would stay empty until the clock
  // or the signal strength changed for the first time.
  if (full) Ui::invalidateStatus();
  Ui::setPageDots(PAGE_N, (uint8_t)g_page);
  Ui::updateStatus();

  if (full) Ui::dipEnd();
}

void switchPage(int delta) {
  if (g_page == Page::Clock) Clock::leave();

  int idx = ((int)g_page + delta + PAGE_N) % PAGE_N;
  g_page = (Page)idx;
  LOGF("[main] page -> %d\n", idx);

  if (g_page == Page::Clock) Clock::enter();

  Ui::setPageDots(PAGE_N, (uint8_t)g_page);
  drawPage(true);
  applyLed();
}

// ---------------------------------------------------------------------
//  Spotify - polling and states
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
          LOGLN("[main] the artwork would not load");
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
          if (full) {
            Ui::invalidateStatus();
            Ui::setPageDots(PAGE_N, (uint8_t)g_page);
            Ui::updateStatus();
            Ui::dipEnd();
          }
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
      LOGF("[main] poll error: %s\n", Spotify::lastError());
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

// After a button press it pays to ask sooner, so the screen does not
// lag behind.
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

    case Page::Weather: {
      // Only repaint when the request actually brought new data. It used
      // to draw the whole screen twice after swiping to the page - once
      // from switchPage() and straight away again from the first request
      // - including two backlight dips in a row.
      static bool wxDrawn = false;
      bool fresh = Weather::poll();
      g_nextPoll[i] = millis() + max<uint32_t>(Weather::nextPollDelay(), 1000);
      // If the request failed, repaint anyway - but only once, so the
      // error message shows instead of an eternal "loading".
      if (fresh || !wxDrawn) { drawPage(true); wxDrawn = true; }
      break;
    }

    case Page::Crypto: {
      static bool btcDrawn = false;
      bool fresh = Crypto::poll();
      g_nextPoll[i] = millis() + max<uint32_t>(Crypto::nextPollDelay(), 1000);
      if (fresh || !btcDrawn) { drawPage(true); btcDrawn = true; }
      break;
    }

    case Page::Clock:
      g_nextPoll[i] = millis() + 60000;      // the clock downloads nothing
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------
//  Spotify controls
// ---------------------------------------------------------------------
void runCommand(Hit h) {
  if (millis() - g_lastCmdAt < TOUCH_REPEAT_MS) return;
  g_lastCmdAt = millis();

  switch (h) {
    case Hit::PlayPause:
      // Flip the icon optimistically right away, so it does not feel
      // stuck.
      if (g_state.isPlaying) { Spotify::pause(); g_state.isPlaying = false; }
      else                   { Spotify::play();  g_state.isPlaying = true; }
      Ui::updateControls(g_state);
      break;

    case Hit::Next:
      Ui::toast(T(S_NEXT_TRACK));
      Spotify::next();
      break;

    case Hit::Prev:
      // Spotify reads "previous" as a jump back to the start once the
      // track has been playing for a while - same as the mobile app.
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
//  Settings
// ---------------------------------------------------------------------
void applyCityFromPortal() {
  const char* city = Net::cityFromPortal();
  if (city && *city && strcmp(city, Weather::place()) != 0) {
    Weather::setPlace(city);
    g_nextPoll[(uint8_t)Page::Weather] = millis();
    LOGF("[main] new place for the weather: %s\n", city);
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
      Touch::calibrate();          // after a rotation the old calibration is void
      break;

    case Ui::SettingsAction::WifiPortal:
      Net::setCityDefault(Weather::place());
      Ui::bootStatus(T(S_PORTAL_OPENING), T(S_PORTAL_JOIN));
      Net::startPortal(Ui::bootStatus);
      applyCityFromPortal();
      break;

    case Ui::SettingsAction::Language:
      // The language switch only shows up in the repaint at the end of
      // this function.
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
//  Touch
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

  // --- press -----------------------------------------------------------
  if (ev.pressed) {
    g_dragValue  = -1;
    g_pressedHit = Hit::None;
    if (g_page == Page::Spotify && g_sub == Sub::NowPlaying) {
      g_pressedHit = Ui::hitTest(ev.x, ev.y);
      Ui::pressFeedback(g_pressedHit, true);
    }
    return;
  }

  // --- dragging along the sliders --------------------------------------
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

  // --- a swipe = change of page ---------------------------------------
  int dx = ev.x - ev.startX;
  int dy = ev.y - ev.startY;
  bool sliderCaptured =
      (g_pressedHit == Hit::Volume || g_pressedHit == Hit::Progress);

  if (!sliderCaptured && abs(dx) > 60 && abs(dx) > 2 * abs(dy)) {
    if (g_pressedHit != Hit::None) Ui::pressFeedback(g_pressedHit, false);
    g_pressedHit = Hit::None;
    switchPage(dx < 0 ? 1 : -1);      // swipe left = next page
    return;
  }

  // --- a tap on the individual pages -----------------------------------
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
        g_nextPoll[(uint8_t)Page::Spotify] = millis();   // try again
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
//  Touch controller probe
//
//  The 2432S024 family has three variants and each puts the touch
//  somewhere else entirely:
//    -C  capacitive CST820 on I2C  (SDA 33, SCL 32, RST 25, INT 21)
//    -R  resistive XPT2046 on the SPI shared with the display (CS 33, IRQ 36)
//    -N  no touch at all
//  Rather than decipher the silkscreen, the board simply asks itself.
//
//  This MUST be called before tft.init(), because it borrows the display
//  pins (SCK/MOSI/MISO) for a moment to bit-bang a read of the XPT2046.
// ---------------------------------------------------------------------
#if TOUCH_PROBE

// A bit-banged XPT2046 read - independent of any library.
uint16_t xptRead(uint8_t cmd, uint8_t sck, uint8_t mosi, uint8_t miso,
                 uint8_t cs) {
  digitalWrite(cs, LOW);
  delayMicroseconds(5);

  for (int i = 7; i >= 0; i--) {                 // the command, MSB first
    digitalWrite(mosi, (cmd >> i) & 1);
    digitalWrite(sck, HIGH); delayMicroseconds(3);
    digitalWrite(sck, LOW);  delayMicroseconds(3);
  }
  digitalWrite(sck, HIGH); delayMicroseconds(3); // the converter's busy clock
  digitalWrite(sck, LOW);  delayMicroseconds(3);

  uint16_t v = 0;
  for (int i = 0; i < 12; i++) {                 // 12 bits of result
    digitalWrite(sck, HIGH); delayMicroseconds(3);
    v = (uint16_t)((v << 1) | (digitalRead(miso) ? 1 : 0));
    digitalWrite(sck, LOW);  delayMicroseconds(3);
  }

  digitalWrite(cs, HIGH);
  return v;
}

void probeTouch() {
  LOGLN("[probe] looking for a touch controller...");

  // --- 1) the capacitive CST820 on I2C --------------------------------
  // First, because the SPI sequence would hold RST on the capacitive chip.
  pinMode(25, OUTPUT);                  // CST820 RST
  digitalWrite(25, LOW);  delay(20);
  digitalWrite(25, HIGH); delay(60);    // the chip takes a moment to come up

  int i2cFound = 0;
  if (Wire.begin(33, 32, 100000)) {     // SDA 33, SCL 32
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        LOGF("[probe] I2C address 0x%02X answers\n", addr);
        i2cFound++;
      }
    }
    Wire.end();
  } else {
    LOGLN("[probe] I2C would not start");
  }

  if (i2cFound) {
    LOGLN("[probe] RESULT: capacitive touch (the 2432S024C variant)");
    return;
  }
  LOGLN("[probe] nobody answered on I2C");

  // --- 2) the resistive XPT2046 on the display bus ---------------------
  const uint8_t SCK = 14, MOSI = 13, MISO = 12, CS = 33;
  pinMode(15, OUTPUT); digitalWrite(15, HIGH);   // deselect the display CS
  pinMode(SCK, OUTPUT);  digitalWrite(SCK, LOW);
  pinMode(MOSI, OUTPUT); digitalWrite(MOSI, LOW);
  pinMode(MISO, INPUT);                          // GPIO12 is a strapping pin - read only
  pinMode(CS, OUTPUT);   digitalWrite(CS, HIGH);
  delay(5);

  uint16_t z1 = xptRead(0xB1, SCK, MOSI, MISO, CS);
  uint16_t x  = xptRead(0xD1, SCK, MOSI, MISO, CS);
  uint16_t y  = xptRead(0x91, SCK, MOSI, MISO, CS);
  LOGF("[probe] XPT2046 na CS33: z1=%u x=%u y=%u\n", z1, x, y);

  // A chip that is not there returns all 0 or all 4095 (a floating input).
  bool plausible = !((z1 == 0 && x == 0 && y == 0) ||
                     (z1 == 4095 && x == 4095 && y == 4095));

  if (plausible) {
    LOGLN("[probe] RESULT: resistive XPT2046 touch (the 2432S024R variant)");
  } else {
    LOGLN("[probe] RESULT: no touch found (the 2432S024N variant?)");
  }

  // Release the pins again, so tft.init() can take them over.
  pinMode(SCK, INPUT);
  pinMode(MOSI, INPUT);
  pinMode(CS, INPUT);
}
#endif  // TOUCH_PROBE

// ---------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  LOGLN("\n=== SpotifyDeck ===");

  // The reason for the last reset. When the board restarts on its own,
  // this says right away whether it was a panic, a watchdog, a brownout
  // or our own ESP.restart() - otherwise you are guessing.
  const char* reasons[] = {
      "unknown",    "power on",       "external reset",  "software",
      "panic",      "watchdog (int)", "watchdog (task)", "watchdog (other)",
      "deep sleep", "brownout",       "SDIO"};
  int r = (int)esp_reset_reason();
  LOGF("[main] reset reason: %s (%d)\n",
       (r >= 0 && r < (int)(sizeof(reasons) / sizeof(reasons[0]))) ? reasons[r]
                                                                  : "?",
       r);

  Lang::begin();           // the UI language, loaded from NVS

#if TOUCH_PROBE
  probeTouch();            // has to run before tft.init()
#endif

  Touch::begin();          // loads the rotation from NVS, before the TFT is up
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

  // A first run with no calibration - offer one right away.
  if (!Touch::hasCalibration()) {
    Ui::bootStatus(T(S_CAL_TITLE), T(S_CAL_SOON));
    delay(1200);
    Touch::calibrate();
  }

  for (uint8_t i = 0; i < PAGE_N; i++) g_nextPoll[i] = millis();
  drawPage(true);

  LOGF("[main] free heap after startup: %u B\n", (unsigned)ESP.getFreeHeap());
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
