// ---------------------------------------------------------------------
//  DeckConfig.h  -  everything you can tune without touching the logic
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// =====================================================================
//  FEATURES  -  0 = off, 1 = on
// =====================================================================
#define FEAT_BLUR_BACKGROUND  1   // blurred album art across the whole screen
#define FEAT_FULLSCREEN_ART   1   // tap the art -> art fills the screen
#define FEAT_VOLUME_SLIDER    1   // drag along the volume bar
#define FEAT_SEEK_BAR         1   // drag along the progress bar = seek
#define FEAT_AUTO_DIM         0   // dim the backlight after inactivity
#define FEAT_LDR_BRIGHTNESS   0   // brightness from ambient light (LDR on GPIO34)
#define FEAT_REPAINT_DIP      0   // brief brightness dip on a large repaint
#define FEAT_RGB_LED          1   // on-board RGB LED glows in the album color
#define FEAT_ART_CACHE        1   // artwork cache in LittleFS (faster switching)
#define FEAT_NTP_CLOCK        1   // clock in the status bar and on the idle screen

// =====================================================================
//  UI LANGUAGE
// =====================================================================
//  0 = Czech, 1 = English. This is only the default on the first boot -
//  after that you switch it on the board itself (long press -> Language)
//  and the choice is stored in NVS.
#define DEFAULT_LANG_EN       1

// =====================================================================
//  ORIENTATION
// =====================================================================
//  1 and 3 are both landscape, 180 degrees apart. Which one is "right"
//  differs from board to board - which is why it can be switched on the
//  device itself (Settings -> Rotate display) and is saved to NVS.
#define DEFAULT_ROTATION      1

#define SCREEN_W            320
#define SCREEN_H            240

// =====================================================================
//  BOARD MODEL
// =====================================================================
//  The "Cheap Yellow Display" family has several models and they differ
//  in the backlight pin. The display pins (12/13/14/15/2) are the same,
//  so a single User_Setup.h works on both.
//
//    28 = ESP32-2432S028   2.8" - backlight GPIO21
//    24 = ESP32-2432S024   2.4" - backlight GPIO27
//
//  If you do not know, flash with BL_SELFTEST and the board finds the
//  pin by itself.
#define DECK_BOARD           24

// =====================================================================
//  PINS
// =====================================================================
//  The display (SPI) is configured in TFT_eSPI/User_Setup.h, not here.
#if DECK_BOARD == 24
  #define PIN_BACKLIGHT      27
#else
  #define PIN_BACKLIGHT      21
#endif

// XPT2046 touch - its own SPI bus
#define PIN_TOUCH_SCK        25
#define PIN_TOUCH_MOSI       32   // T_DIN
#define PIN_TOUCH_MISO       39   // T_OUT  (input only)
#define PIN_TOUCH_CS         33
#define PIN_TOUCH_IRQ        36   // T_IRQ  (input only)

#define PIN_LDR              34   // photoresistor, ADC1 - works even with WiFi up
//  0 = a higher ADC value means more light (the usual CYD wiring)
//  1 = inverted; set this if the display dims in light instead of in the dark
#define LDR_INVERTED          0
#define PIN_LED_R             4   // the RGB LED is ACTIVE LOW
#define PIN_LED_G            16
#define PIN_LED_B            17

// =====================================================================
//  BACKLIGHT
// =====================================================================
//  Backlight polarity varies between CYD clones. If the display stays
//  black after flashing but the sketch the board shipped with did light
//  up (such a sketch usually never touched GPIO21), this is the switch.
//    0 = HIGH turns it on  (most boards)
//    1 = LOW turns it on
#define BL_ACTIVE_LOW         0

//  Backlight pin self-test: walks the candidates 27/21/16/5, holding
//  each HIGH for a moment and then LOW, and writes on the display which
//  one it is trying. Value = number of rounds. 0 = off (normal use).
#define BL_SELFTEST           0

//  Touch controller probe: on boot it detects whether the board has a
//  capacitive CST820 (I2C) or a resistive XPT2046 (SPI) and writes it
//  to Serial. Once you know which variant you have, turn it off.
#define TOUCH_PROBE           0   // 1 = detect the touch type on boot and print it to Serial

#define BL_PWM_FREQ       5000
#define BL_PWM_BITS         12          // 0..4095
#define BL_MAX            4095
#define BL_MIN             240          // a fully black screen is hard to find
#define BL_DIM_AFTER_MS  45000UL        // how long idle before dimming
#define BL_DIM_LEVEL       700
#define BL_FADE_STEP        90          // brightness fade speed

// On-board RGB LED (different frequency/resolution than the backlight ->
// its own LEDC timer; the ESP32 has plenty of them)
#define LED_PWM_FREQ      5000
#define LED_PWM_BITS         8

// =====================================================================
//  TOUCH
// =====================================================================
#define TOUCH_Z_MIN        350   // minimum "pressure" for a touch to count
#define TOUCH_DEBOUNCE_MS   40
#define TOUCH_REPEAT_MS    260   // guards against double taps on buttons
#define TOUCH_DRAG_PX       10   // past this many pixels it is a drag, not a tap
#define TOUCH_LONGPRESS_MS 900   // long press = settings

// Fallback calibration (raw ADC values). The first run walks you through
// a real calibration and saves it to NVS; this is only a starting guess.
#define TOUCH_RAW_X_MIN    200
#define TOUCH_RAW_X_MAX   3700
#define TOUCH_RAW_Y_MIN    240
#define TOUCH_RAW_Y_MAX   3800

// =====================================================================
//  SPOTIFY / NETWORK
// =====================================================================
#define POLL_PLAYING_MS      3000UL   // how often to ask while something plays
#define POLL_IDLE_MS        15000UL   // when nothing is playing
#define POLL_AFTER_CMD_MS     450UL   // quick poll right after a button press
#define HTTP_TIMEOUT_MS      8000
#define TLS_HANDSHAKE_S        12

#define TOKEN_EARLY_REFRESH_S  120    // refresh the token 2 min before it expires
#define BACKOFF_START_MS     5000UL
#define BACKOFF_MAX_MS     120000UL

// How many bytes to keep free before we start downloading artwork.
#define ART_MIN_FREE_HEAP   45000
#define ART_MAX_BYTES       72000     // a 300x300 JPEG from Spotify is usually 8-48 kB

// One-shot: forget the stored WiFi on boot and go straight to the
// portal. Set it back to 0 once you are connected, otherwise the board
// forgets the network on every restart.
#define WIFI_FORGET           0

// WiFi portal
#define AP_NAME          "SpotifyDeck"
#define AP_PASSWORD      "spotifydeck"   // min. 8 characters, otherwise the ESP32 AP refuses it
#define PORTAL_TIMEOUT_S  240

// NTP
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.nist.gov"
// Central European Time including DST (POSIX TZ string)
#define TZ_STRING        "CET-1CEST,M3.5.0,M10.5.0/3"

// =====================================================================
//  SCREEN LAYOUT  -  the main "now playing" page (320x240)
// =====================================================================
//
//   0 ---------------------------------------------------- 320
//   |  [wifi] device                                14:32  |  status bar
//  26  +--------------+   360                              |
//   |  |              |   Charli xcx                       |
//   |  |  cover 150px |   brat                             |
//   |  |              |   * Kitchen (Sonos)                |
//   |  |              |   [((o  ---------o-----  ]         |  volume
// 176  +--------------+                                    |
// 188  1:12  =========o--------------------------  3:33    |  progress
// 200  ( shuffle   prev   play/pause   next   repeat )      |  glass bar
// 240 ---------------------------------------------------- 
//
#define STATUS_Y              0
#define STATUS_H             22

#define ART_X                14
#define ART_Y                26
#define ART_SIZE            150          // 300x300 JPEG decoded at 1:2
#define ART_RADIUS            8

#define TXT_X               178
#define TXT_W               128
#define TITLE_Y              32
#define TITLE_H              30
#define ARTIST_Y             64
#define ARTIST_H             22
#define ALBUM_Y              88
#define ALBUM_H              18
#define DEVICE_Y            110
#define DEVICE_H             16

#define VOL_X               178
#define VOL_Y               142
#define VOL_W               128
#define VOL_H                24
#define VOL_BAR_X           200          // after the speaker icon
#define VOL_BAR_W           106
#define VOL_HIT_Y           130
#define VOL_HIT_H            46

#define PROG_ROW_Y          180
#define PROG_ROW_H           18
#define PROG_BAR_Y          188
#define PROG_BAR_H            5
#define PROG_BAR_X           54
#define PROG_BAR_W          212          // 54 .. 266
#define TIME_L_X             14
#define TIME_R_X            306
#define PROG_HIT_Y          178          // below the art, which ends at 176
#define PROG_HIT_H           18

#define CTRL_X                8
#define CTRL_Y              200
#define CTRL_W              304
#define CTRL_H               34
#define CTRL_R               15
#define CTRL_COUNT            5
#define CTRL_CY             217          // icon centers
#define CTRL_HIT_Y          197
#define CTRL_HIT_H           43

// Center of button i (i = 0..4)
#define CTRL_CX(i)  (CTRL_X + (CTRL_W * (2 * (i) + 1)) / (2 * CTRL_COUNT))

// =====================================================================
//  DEBUGGING
// =====================================================================
#define DECK_VERBOSE          1   // log to Serial (115200)
#define DECK_SHOW_HEAP        0   // keep printing the free heap

#if DECK_VERBOSE
  #define LOGF(...)  Serial.printf(__VA_ARGS__)
  #define LOGLN(x)   Serial.println(x)
#else
  #define LOGF(...)  do {} while (0)
  #define LOGLN(x)   do {} while (0)
#endif
