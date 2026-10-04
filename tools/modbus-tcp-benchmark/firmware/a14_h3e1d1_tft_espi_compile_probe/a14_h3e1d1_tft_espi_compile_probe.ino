/*
  A14 H3E.1D.1 - TFT_eSPI compile/config qualification

  Gate de compilacion solamente.
  No debe subirse como firmware de produccion.

  Objetivo:
  - TFT_eSPI 2.5.43.
  - Arduino-ESP32 3.3.8 del package JWPLC.
  - ST7789 170x320.
  - SPI 80 MHz, MODE0.
  - pines reales JWPLC Basic v2.
  - GLCD clasica solamente.
  - transacciones SPI habilitadas.
*/

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <TFT_eSPI.h>

#ifndef JWPLC_H3E1D1_SETUP
#error "H3E1D1: tft_setup.h local no fue cargado"
#endif

#if ESP_ARDUINO_VERSION_MAJOR != 3 || \
    ESP_ARDUINO_VERSION_MINOR != 3 || \
    ESP_ARDUINO_VERSION_PATCH != 8
#error "H3E1D1: Arduino-ESP32 esperado = 3.3.8"
#endif

#ifndef ST7789_DRIVER
#error "H3E1D1: ST7789_DRIVER no definido"
#endif

#if TFT_WIDTH != 170
#error "H3E1D1: TFT_WIDTH esperado = 170"
#endif

#if TFT_HEIGHT != 320
#error "H3E1D1: TFT_HEIGHT esperado = 320"
#endif

#if TFT_MOSI != 23
#error "H3E1D1: TFT_MOSI esperado = 23"
#endif

#if TFT_MISO != 19
#error "H3E1D1: TFT_MISO esperado = 19"
#endif

#if TFT_SCLK != 18
#error "H3E1D1: TFT_SCLK esperado = 18"
#endif

#if TFT_CS != 33
#error "H3E1D1: TFT_CS esperado = 33"
#endif

#if TFT_DC != 25
#error "H3E1D1: TFT_DC esperado = 25"
#endif

#if TFT_RST != 14
#error "H3E1D1: TFT_RST esperado = 14"
#endif

#if SPI_FREQUENCY != 80000000
#error "H3E1D1: SPI_FREQUENCY esperado = 80000000"
#endif

#if TFT_SPI_MODE != SPI_MODE0
#error "H3E1D1: TFT_SPI_MODE esperado = SPI_MODE0"
#endif

#if TFT_RGB_ORDER != TFT_BGR
#error "H3E1D1: TFT_RGB_ORDER esperado = TFT_BGR"
#endif

#ifndef TFT_INVERSION_ON
#error "H3E1D1: TFT_INVERSION_ON requerido para paridad con Adafruit actual"
#endif

#ifndef LOAD_GLCD
#error "H3E1D1: LOAD_GLCD requerido"
#endif

#ifdef LOAD_FONT2
#error "H3E1D1: LOAD_FONT2 no debe cargarse"
#endif

#ifdef LOAD_FONT4
#error "H3E1D1: LOAD_FONT4 no debe cargarse"
#endif

#ifdef LOAD_FONT6
#error "H3E1D1: LOAD_FONT6 no debe cargarse"
#endif

#ifdef LOAD_FONT7
#error "H3E1D1: LOAD_FONT7 no debe cargarse"
#endif

#ifdef LOAD_FONT8
#error "H3E1D1: LOAD_FONT8 no debe cargarse"
#endif

#ifdef LOAD_GFXFF
#error "H3E1D1: LOAD_GFXFF no debe cargarse"
#endif

#ifdef SMOOTH_FONT
#error "H3E1D1: SMOOTH_FONT no debe cargarse"
#endif

#ifndef SUPPORT_TRANSACTIONS
#error "H3E1D1: SUPPORT_TRANSACTIONS requerido para bus SPI compartido"
#endif

TFT_eSPI h3e1d1Tft = TFT_eSPI();

void setup()
{
    // Estas llamadas fuerzan compilacion y link de los caminos que usara
    // el renderer experimental. Este gate NO realiza upload.
    h3e1d1Tft.init();
    h3e1d1Tft.setRotation(1);

    h3e1d1Tft.fillRect(
        8,
        42,
        80,
        18,
        TFT_BLACK);

    h3e1d1Tft.setTextSize(2);
    h3e1d1Tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK);
    h3e1d1Tft.setCursor(
        8,
        42);
    h3e1d1Tft.print("H3E1D1");
}

void loop()
{
}
