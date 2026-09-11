// =====================================================================
//  TFT_eSPI  ->  User_Setup.h  pro ESP32-2432S028R ("Cheap Yellow Display")
//
//  Zkopiruj tenhle soubor pres:
//      Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//
//  Proc je jiny nez ten z bezneho navodu:
//    * ILI9341_2_DRIVER  - alternativni init sekvence, kterou pouziva
//                          oficialni CYD repo. Nepousti INVON, takze
//                          barvy nejsou invertovane.
//    * USE_HSPI_PORT     - displej je fyzicky na HSPI. Bez tehle radky
//                          TFT_eSPI zvoli VSPI a piny 12/13/14/15 vede
//                          pres GPIO matrix: funguje to, ale pomaleji
//                          a leze si to do cesty s SD kartou.
//    * TFT_BL / TFT_BACKLIGHT_ON - aby knihovna vedela o podsviceni.
//    * TOUCH_CS         - schvalne NENI definovane. Dotyk resi
//                          XPT2046_Touchscreen na druhe SPI sbernici.
//                          Pozor: "#define TOUCH_CS -1" NEVYPNE dotyk
//                          v TFT_eSPI, jen mu da neplatny pin.
// =====================================================================

#define USER_SETUP_INFO "CYD ESP32-2432S028R / SpotifyDeck"

// ---------------------------------------------------------------------
//  DRIVER DISPLEJE  -  vyber podle toho, kolik ma deska USB konektoru
// ---------------------------------------------------------------------
//
//   1x micro-USB          -> ILI9341  (nech, jak to je)
//   micro-USB + USB-C     -> ST7789   (zakomentuj blok A, odkomentuj B)
//
// Rychly test: nahraj sketch a podivej se na obrazovku. Kdyz jsou barvy
// invertovane (cerna sviti bile), mas tu druhou desku.

// ---- BLOK A: ILI9341 (1x USB) ----------------------------------------
#define ILI9341_2_DRIVER

// ---- BLOK B: ST7789 (2x USB, s USB-C) --------------------------------
// #define ST7789_DRIVER
// #define TFT_RGB_ORDER TFT_BGR
// #define TFT_INVERSION_OFF

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---------------------------------------------------------------------
//  SBERNICE A PINY DISPLEJE
// ---------------------------------------------------------------------
#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1      // RESET displeje visi na EN desky

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// ---------------------------------------------------------------------
//  FONTY
// ---------------------------------------------------------------------
#define LOAD_GLCD    // font 1 - male bitmapove pismo
#define LOAD_FONT2   // font 2 - 16px
#define LOAD_FONT4   // font 4 - 26px
#define LOAD_FONT6   // 48px, jen cislice a dvojtecka
#define LOAD_FONT7   // sedmisegmentove cislice - digitalni hodiny
#define LOAD_FONT8   // 75px cislice
#define LOAD_GFXFF   // Adafruit FreeFonts (FreeSansBold12pt7b atd.) - POVINNE
#define SMOOTH_FONT  // podpora .vlw fontu

// ---------------------------------------------------------------------
//  RYCHLOST SPI
// ---------------------------------------------------------------------
#define SPI_FREQUENCY       55000000   // kdyz obcas blikne sum, zkus 40000000
#define SPI_READ_FREQUENCY  20000000
