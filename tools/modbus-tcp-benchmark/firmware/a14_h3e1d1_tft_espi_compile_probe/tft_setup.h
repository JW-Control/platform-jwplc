#pragma once

// A14 H3E.1D.1 - configuracion local y reproducible para TFT_eSPI 2.5.43.
// TFT_eSPI detecta tft_setup.h desde el sketch y evita depender de editar
// User_Setup.h dentro de la libreria instalada.

#define USER_SETUP_INFO "JWPLC Basic v2 H3E1D1"
#define JWPLC_H3E1D1_SETUP 1

#define ST7789_DRIVER

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

// Paridad con el backend Adafruit actual.
#define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_ON

// Bus SPI compartido JWPLC Basic v2.
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18

#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  14

// H3E.1D compara contra el baseline validado a 80 MHz.
// Adafruit_ST7789::init() usa SPI_MODE0 por defecto.
#define TFT_SPI_MODE SPI_MODE0
#define SPI_FREQUENCY 80000000

// Cargar solamente la fuente clasica equivalente a la usada por el HMI actual.
#define LOAD_GLCD

// El TFT comparte SPI con W5500, microSD y FRAM.
#define SUPPORT_TRANSACTIONS
