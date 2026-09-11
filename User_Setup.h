// =====================================================================
//  TFT_eSPI  ->  User_Setup.h  pro desky "Cheap Yellow Display" (CYD)
//
//  Zkopiruj tenhle soubor pres:
//      Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//  (puvodni si radeji nejdriv prejmenuj na User_Setup.h.bak)
//
//  POZOR: tenhle soubor je spolecny pro cely pocitac - meni nastaveni
//  displeje i pro ostatni tve TFT_eSPI projekty.
// =====================================================================

// ---------------------------------------------------------------------
//  1) VYBER DESKU
// ---------------------------------------------------------------------
//    24 = ESP32-2432S024   2,4"  - podsviceni GPIO27, panel chce inverzi
//    28 = ESP32-2432S028   2,8"  - podsviceni GPIO21
//
//  Musi souhlasit s DECK_BOARD v SpotifyDeck/DeckConfig.h.
#define CYD_BOARD 24

#define USER_SETUP_INFO "CYD / SpotifyDeck"

// ---------------------------------------------------------------------
//  2) RADIC DISPLEJE
// ---------------------------------------------------------------------
//  Obe desky maji ILI9341 240x320. "ILI9341_2" je alternativni
//  inicializacni sekvence, kterou pouzivaji oficialni konfigurace CYD.
//  Kdyby byl displej bily nebo poprehazeny, zkus misto nej ILI9341_DRIVER.
#define ILI9341_2_DRIVER
// #define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

//  Inverze barev je zavisla na serii panelu. Kdyz je tmava barva svetla
//  a obraz vypada vybledle, prohod tenhle prepinac.
#if CYD_BOARD == 24
  #define TFT_INVERSION_ON
#endif

// ---------------------------------------------------------------------
//  3) SBERNICE A PINY DISPLEJE  (stejne na obou deskach)
// ---------------------------------------------------------------------
//  Displej je fyzicky na HSPI. Bez USE_HSPI_PORT zvoli TFT_eSPI VSPI
//  a piny vede pres GPIO matici - funguje to, ale pomaleji.
#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1      // RESET displeje visi na EN desky

// ---------------------------------------------------------------------
//  4) PODSVICENI  -  tady se desky lisi nejvic
// ---------------------------------------------------------------------
#if CYD_BOARD == 24
  #define TFT_BL 27
#else
  #define TFT_BL 21
#endif
#define TFT_BACKLIGHT_ON HIGH

// ---------------------------------------------------------------------
//  5) DOTYK
// ---------------------------------------------------------------------
//  2432S028R  - XPT2046 ma VLASTNI SPI sbernici (SCK 25, MOSI 32,
//               MISO 39, CS 33). TFT_eSPI ho obsluhovat neumi, resi ho
//               knihovna XPT2046_Touchscreen -> TOUCH_CS nechat nedefinovane.
//  2432S024R  - XPT2046 sdili sbernici s displejem, CS je GPIO33,
//               takze ho zvladne primo TFT_eSPI -> odkomentuj TOUCH_CS.
//  2432S024C  - kapacitni CST820 na I2C (SDA 33, SCL 32). TOUCH_CS MUSI
//               zustat nedefinovane, jinak by TFT_eSPI hazel chip select
//               na datovou linku I2C a rozbil dotyk.
//
//  Pozor: "#define TOUCH_CS -1" dotyk NEVYPNE, jen mu da neplatny pin.
//  Vypina se tim, ze radek vubec neexistuje.
#if CYD_BOARD == 24
  #define TOUCH_CS 33
#endif

// ---------------------------------------------------------------------
//  6) FONTY
// ---------------------------------------------------------------------
#define LOAD_GLCD    // font 1 - male bitmapove pismo
#define LOAD_FONT2   // font 2 - 16px
#define LOAD_FONT4   // font 4 - 26px
#define LOAD_FONT6   // 48px, jen cislice
#define LOAD_FONT7   // sedmisegmentove cislice - digitalni hodiny
#define LOAD_FONT8   // 75px cislice
#define LOAD_GFXFF   // Adafruit FreeFonts (FreeSansBold12pt7b atd.) - POVINNE
#define SMOOTH_FONT  // podpora .vlw fontu

// ---------------------------------------------------------------------
//  7) RYCHLOST SPI
// ---------------------------------------------------------------------
#define SPI_FREQUENCY       55000000   // kdyz obcas blikne sum, zkus 40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
