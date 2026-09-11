// =====================================================================
//  TFT_eSPI  ->  User_Setup.h  for "Cheap Yellow Display" (CYD) boards
//
//  Copy this file over:
//      Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//  (better rename the original to User_Setup.h.bak first)
//
//  WARNING: this file is shared across the whole machine - it changes the
//  display settings for your other TFT_eSPI projects too.
// =====================================================================

// ---------------------------------------------------------------------
//  1) PICK YOUR BOARD
// ---------------------------------------------------------------------
//    24 = ESP32-2432S024   2.4"  - backlight GPIO27, the panel needs inversion
//    28 = ESP32-2432S028   2.8"  - backlight GPIO21
//
//  This has to match DECK_BOARD in SpotifyDeck/DeckConfig.h.
#define CYD_BOARD 24

#define USER_SETUP_INFO "CYD / SpotifyDeck"

// ---------------------------------------------------------------------
//  2) DISPLAY CONTROLLER
// ---------------------------------------------------------------------
//  Both boards carry an ILI9341 240x320. "ILI9341_2" is the alternative
//  init sequence that the official CYD configurations use. If the display
//  comes up white or scrambled, try ILI9341_DRIVER instead.
#define ILI9341_2_DRIVER
// #define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

//  Color inversion depends on the panel batch. If dark colors come out
//  light and the image looks washed out, flip this switch.
#if CYD_BOARD == 24
  #define TFT_INVERSION_ON
#endif

// ---------------------------------------------------------------------
//  3) DISPLAY BUS AND PINS  (the same on both boards)
// ---------------------------------------------------------------------
//  The display physically sits on HSPI. Without USE_HSPI_PORT, TFT_eSPI
//  picks VSPI and routes the pins through the GPIO matrix - that works,
//  but it is slower.
#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1      // the display RESET hangs off the board EN

// ---------------------------------------------------------------------
//  4) BACKLIGHT  -  where the boards differ the most
// ---------------------------------------------------------------------
#if CYD_BOARD == 24
  #define TFT_BL 27
#else
  #define TFT_BL 21
#endif
#define TFT_BACKLIGHT_ON HIGH

// ---------------------------------------------------------------------
//  5) TOUCH
// ---------------------------------------------------------------------
//  2432S028R  - the XPT2046 has its OWN SPI bus (SCK 25, MOSI 32,
//               MISO 39, CS 33). TFT_eSPI cannot drive it, the
//               XPT2046_Touchscreen library does -> leave TOUCH_CS undefined.
//  2432S024R  - the XPT2046 shares the bus with the display, CS is GPIO33,
//               so TFT_eSPI can drive it directly -> uncomment TOUCH_CS.
//  2432S024C  - a capacitive CST820 on I2C (SDA 33, SCL 32). TOUCH_CS MUST
//               stay undefined, otherwise TFT_eSPI would drive a chip
//               select onto the I2C data line and break the touch.
//
//  Careful: "#define TOUCH_CS -1" does NOT disable touch, it just gives it
//  an invalid pin. You disable it by not having the line at all.
#if CYD_BOARD == 24
  #define TOUCH_CS 33
#endif

// ---------------------------------------------------------------------
//  6) FONTS
// ---------------------------------------------------------------------
#define LOAD_GLCD    // font 1 - small bitmap typeface
#define LOAD_FONT2   // font 2 - 16px
#define LOAD_FONT4   // font 4 - 26px
#define LOAD_FONT6   // 48px, digits only
#define LOAD_FONT7   // seven-segment digits - the digital clock
#define LOAD_FONT8   // 75px digits
#define LOAD_GFXFF   // Adafruit FreeFonts (FreeSansBold12pt7b etc.) - REQUIRED
#define SMOOTH_FONT  // .vlw font support

// ---------------------------------------------------------------------
//  7) SPI SPEED
// ---------------------------------------------------------------------
#define SPI_FREQUENCY       55000000   // if you see occasional noise, try 40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
