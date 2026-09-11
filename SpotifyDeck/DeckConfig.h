// ---------------------------------------------------------------------
//  DeckConfig.h  -  vsechno, co se da ladit bez zasahu do logiky
// ---------------------------------------------------------------------
#pragma once

#include <Arduino.h>

// =====================================================================
//  FUNKCE  -  0 = vypnuto, 1 = zapnuto
// =====================================================================
#define FEAT_BLUR_BACKGROUND  1   // rozmazany obal alba pres celou plochu
#define FEAT_FULLSCREEN_ART   1   // klepnuti na obal -> obal na celou obrazovku
#define FEAT_VOLUME_SLIDER    1   // tazeni po hlasitosti
#define FEAT_SEEK_BAR         1   // tazeni po progress baru = previjeni
#define FEAT_AUTO_DIM         0   // ztlumeni podsviceni po necinnosti
#define FEAT_LDR_BRIGHTNESS   0   // jas podle okolniho svetla (LDR na GPIO34)
#define FEAT_REPAINT_DIP      0   // kratke stazeni jasu pri velkem prekresleni
#define FEAT_RGB_LED          1   // RGB LED na desce sviti barvou alba
#define FEAT_ART_CACHE        1   // cache obalu v LittleFS (rychlejsi prepinani)
#define FEAT_NTP_CLOCK        1   // hodiny ve stavovem radku a na idle obrazovce

// =====================================================================
//  JAZYK PROSTREDI
// =====================================================================
//  0 = cesky, 1 = anglicky. Je to jen vychozi hodnota pri prvnim startu -
//  pak se prepina primo na desce (dlouhy stisk -> Jazyk) a volba se
//  uklada do NVS.
#define DEFAULT_LANG_EN       1

// =====================================================================
//  ORIENTACE
// =====================================================================
//  1 a 3 jsou obe na sirku, otocene o 180 stupnu. Ktera je "spravne"
//  se lisi kus od kusu - proto je to prepinatelne primo na desce
//  (Nastaveni -> Otocit displej) a ulozi se do NVS.
#define DEFAULT_ROTATION      1

#define SCREEN_W            320
#define SCREEN_H            240

// =====================================================================
//  MODEL DESKY
// =====================================================================
//  Rodina "Cheap Yellow Display" ma nekolik modelu a lisi se pinem
//  podsviceni. Piny displeje (12/13/14/15/2) maji stejne, proto stejny
//  User_Setup.h funguje na obou.
//
//    28 = ESP32-2432S028   2,8" - podsviceni GPIO21
//    24 = ESP32-2432S024   2,4" - podsviceni GPIO27
//
//  Kdyz nevis, nahraj s BL_SELFTEST a deska si pin najde sama.
#define DECK_BOARD           24

// =====================================================================
//  PINY
// =====================================================================
//  Displej (SPI) je nastaveny v TFT_eSPI/User_Setup.h, ne tady.
#if DECK_BOARD == 24
  #define PIN_BACKLIGHT      27
#else
  #define PIN_BACKLIGHT      21
#endif

// XPT2046 dotyk - vlastni SPI sbernice
#define PIN_TOUCH_SCK        25
#define PIN_TOUCH_MOSI       32   // T_DIN
#define PIN_TOUCH_MISO       39   // T_OUT  (jen vstup)
#define PIN_TOUCH_CS         33
#define PIN_TOUCH_IRQ        36   // T_IRQ  (jen vstup)

#define PIN_LDR              34   // fotorezistor, ADC1 - funguje i s WiFi
//  0 = vetsi hodnota z ADC znamena vic svetla (bezne zapojeni CYD)
//  1 = obracene; nastav, kdyz se displej ztlumuje na svetle misto ve tme
#define LDR_INVERTED          0
#define PIN_LED_R             4   // RGB LED je ACTIVE LOW
#define PIN_LED_G            16
#define PIN_LED_B            17

// =====================================================================
//  PODSVICENI
// =====================================================================
//  Polarita podsviceni se mezi klony CYD lisi. Kdyz je displej po
//  nahrani cerny, ale puvodni sketch na desce svitil (takovy sketch
//  na GPIO21 vetsinou vubec nesahal), je to tenhle prepinac.
//    0 = HIGH rozsvecuje  (vetsina desek)
//    1 = LOW rozsvecuje
#define BL_ACTIVE_LOW         0

//  Self-test hledani pinu podsviceni: projede kandidaty 27/21/16/5,
//  kazdy chvili HIGH a chvili LOW, a na displej napise, ktery zrovna
//  zkousi. Hodnota = pocet kol. 0 = test vypnuty (normalni provoz).
#define BL_SELFTEST           0

//  Sonda dotykoveho radice: pri startu zjisti, jestli je na desce
//  kapacitni CST820 (I2C) nebo rezistivni XPT2046 (SPI), a napise to
//  do Serialu. Po zjisteni varianty se da vypnout.
#define TOUCH_PROBE           0   // 1 = pri startu zjistit typ dotyku a napsat do Serialu

#define BL_PWM_FREQ       5000
#define BL_PWM_BITS         12          // 0..4095
#define BL_MAX            4095
#define BL_MIN             240          // uplne cerna obrazovka se blbe hleda
#define BL_DIM_AFTER_MS  45000UL        // po jak dlouhe necinnosti ztlumit
#define BL_DIM_LEVEL       700
#define BL_FADE_STEP        90          // rychlost prechodu jasu

// RGB LED na desce (jina frekvence/rozliseni nez podsviceni -> vlastni
// LEDC timer; ESP32 jich ma dost)
#define LED_PWM_FREQ      5000
#define LED_PWM_BITS         8

// =====================================================================
//  DOTYK
// =====================================================================
#define TOUCH_Z_MIN        350   // minimalni "pritlak", aby se dotyk pocital
#define TOUCH_DEBOUNCE_MS   40
#define TOUCH_REPEAT_MS    260   // ochrana proti dvojkliku na tlacitkach
#define TOUCH_DRAG_PX       10   // od kolika pixelu je to tazeni, ne klepnuti
#define TOUCH_LONGPRESS_MS 900   // dlouhy stisk = nastaveni

// Zalozni kalibrace (surove hodnoty ADC). Prvni spusteni te provede
// vlastni kalibraci a ulozi ji do NVS, tohle je jen vychozi odhad.
#define TOUCH_RAW_X_MIN    200
#define TOUCH_RAW_X_MAX   3700
#define TOUCH_RAW_Y_MIN    240
#define TOUCH_RAW_Y_MAX   3800

// =====================================================================
//  SPOTIFY / SIT
// =====================================================================
#define POLL_PLAYING_MS      3000UL   // jak casto se ptat, kdyz hraje
#define POLL_IDLE_MS        15000UL   // kdyz nic nehraje
#define POLL_AFTER_CMD_MS     450UL   // rychly dotaz hned po stisku tlacitka
#define HTTP_TIMEOUT_MS      8000
#define TLS_HANDSHAKE_S        12

#define TOKEN_EARLY_REFRESH_S  120    // obnovit token 2 min pred vyprsenim
#define BACKOFF_START_MS     5000UL
#define BACKOFF_MAX_MS     120000UL

// Kolik bajtu si nechat volnych, nez se pustime do stahovani obalu.
#define ART_MIN_FREE_HEAP   45000
#define ART_MAX_BYTES       72000     // 300x300 JPEG od Spotify byva 8-48 kB

// Jednorazove: pri startu zapomenout ulozenou WiFi a jit rovnou do
// portalu. Po uspesnem pripojeni vrat na 0, jinak deska zapomene sit
// pri kazdem restartu.
#define WIFI_FORGET           0

// WiFi portal
#define AP_NAME          "SpotifyDeck"
#define AP_PASSWORD      "spotifydeck"   // min. 8 znaku, jinak ESP32 AP odmitne
#define PORTAL_TIMEOUT_S  240

// NTP
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.nist.gov"
// Stredoevropsky cas vcetne letniho casu (POSIX TZ retezec)
#define TZ_STRING        "CET-1CEST,M3.5.0,M10.5.0/3"

// =====================================================================
//  ROZLOZENI OBRAZOVKY  -  hlavni "now playing" (320x240)
// =====================================================================
//
//   0 ---------------------------------------------------- 320
//   |  [wifi] zarizeni                              14:32  |  stavovy radek
//  26  +--------------+   360                              |
//   |  |              |   Charli xcx                       |
//   |  |  obal 150px  |   brat                             |
//   |  |              |   * Kuchyne (Sonos)                |
//   |  |              |   [((o  ---------o-----  ]         |  hlasitost
// 176  +--------------+                                    |
// 188  1:12  =========o--------------------------  3:33    |  prubeh
// 200  ( shuffle   prev   play/pause   next   repeat )      |  sklenena lista
// 240 ---------------------------------------------------- 
//
#define STATUS_Y              0
#define STATUS_H             22

#define ART_X                14
#define ART_Y                26
#define ART_SIZE            150          // JPEG 300x300 dekodovany 1:2
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
#define VOL_BAR_X           200          // za ikonou reproduktoru
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
#define PROG_HIT_Y          178          // pod obalem, ktery konci na 176
#define PROG_HIT_H           18

#define CTRL_X                8
#define CTRL_Y              200
#define CTRL_W              304
#define CTRL_H               34
#define CTRL_R               15
#define CTRL_COUNT            5
#define CTRL_CY             217          // stred ikon
#define CTRL_HIT_Y          197
#define CTRL_HIT_H           43

// Stred i-teho tlacitka (i = 0..4)
#define CTRL_CX(i)  (CTRL_X + (CTRL_W * (2 * (i) + 1)) / (2 * CTRL_COUNT))

// =====================================================================
//  LADENI
// =====================================================================
#define DECK_VERBOSE          1   // vypisy na Serial (115200)
#define DECK_SHOW_HEAP        0   // prubezne vypisovat volnou pamet

#if DECK_VERBOSE
  #define LOGF(...)  Serial.printf(__VA_ARGS__)
  #define LOGLN(x)   Serial.println(x)
#else
  #define LOGF(...)  do {} while (0)
  #define LOGLN(x)   do {} while (0)
#endif
