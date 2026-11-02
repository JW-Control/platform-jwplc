#pragma once

// Configuracion del backend JWPLC_TFT. En modo fuente (alpha.16) la copia de
// TFT_eSPI 2.5.43 de src/ la toma con __has_include(<tft_setup.h>); al cierre
// del alpha se regenera src/esp32/libJWPLC_TFT.a y se vuelve a precompiled=full.
// El usuario no necesita TFT_eSPI instalado ni User_Setup.h propio.

#define USER_SETUP_INFO "JWPLC_TFT Basic v2"
#define JWPLC_TFT_BACKEND_SETUP 1
#define JWPLC_TFT_DEFER_DISPON 1

#define ST7789_DRIVER

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON

#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  14

#define TFT_SPI_MODE SPI_MODE0
#define SPI_FREQUENCY 80000000

#define LOAD_GLCD   // Font 1: 5x7 clásica
#define LOAD_GFXFF  // FreeFonts: sólo se enlazan las que el sketch usa (0.3.0)
// Font2/Font4 de TFT_eSPI NO: agregan ~9 KB a todo sketch aunque no se usen.
#define SUPPORT_TRANSACTIONS