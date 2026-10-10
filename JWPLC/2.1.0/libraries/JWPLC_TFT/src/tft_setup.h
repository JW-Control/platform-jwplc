#pragma once

// Configuracion de mantenimiento para regenerar el backend JWPLC_TFT.
// Los builds normales del package usan src/esp32/libJWPLC_TFT.a y no
// requieren TFT_eSPI instalado ni User_Setup.h del usuario.

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

#define LOAD_GLCD
#define SUPPORT_TRANSACTIONS