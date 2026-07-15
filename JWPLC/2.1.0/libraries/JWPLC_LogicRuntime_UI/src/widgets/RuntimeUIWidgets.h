#ifndef JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H
#define JWPLC_LOGIC_RUNTIME_UI_WIDGETS_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>

namespace JWPLCLogicRuntimeUIWidgets
{
  static constexpr int16_t SCREEN_W = 320;
  static constexpr int16_t SCREEN_H = 170;

  static constexpr uint16_t COLOR_BACKGROUND = ST77XX_BLACK;
  static constexpr uint16_t COLOR_TEXT = ST77XX_WHITE;
  static constexpr uint16_t COLOR_MUTED = 0x9CF3;
  static constexpr uint16_t COLOR_BORDER = 0x7BEF;
  static constexpr uint16_t COLOR_ACCENT = 0x07FF;
  static constexpr uint16_t COLOR_OK = 0x07E0;
  static constexpr uint16_t COLOR_WARNING = 0xFFE0;
  static constexpr uint16_t COLOR_ERROR = ST77XX_RED;
  static constexpr uint16_t COLOR_PANEL = 0x1082;
  static constexpr uint16_t COLOR_SELECTED = 0x18E3;

  void clearScreen(Adafruit_ST7789 &tft);
  void drawHeader(Adafruit_ST7789 &tft,
                  const char *title,
                  const char *stateText,
                  uint16_t stateColor);
  void drawPanel(Adafruit_ST7789 &tft,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 int16_t h,
                 const char *title);
  void drawLabelValue(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      const char *value,
                      int16_t clearWidth);
  void drawMenuButton(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      int16_t w,
                      int16_t h,
                      const char *label,
                      bool selected);
  void drawFooter(Adafruit_ST7789 &tft, const char *text);
}

#endif
