#include "RuntimeUIWidgets.h"

namespace JWPLCLogicRuntimeUIWidgets
{
  void clearScreen(Adafruit_ST7789 &tft)
  {
    tft.fillScreen(COLOR_BACKGROUND);
  }

  void drawHeader(Adafruit_ST7789 &tft,
                  const char *title,
                  const char *stateText,
                  uint16_t stateColor)
  {
    tft.fillRect(0, 0, SCREEN_W, 24, COLOR_PANEL);
    tft.drawFastHLine(0, 23, SCREEN_W, COLOR_BORDER);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.setCursor(6, 5);
    tft.print(title ? title : "JWPLC LOGIC");

    tft.fillRoundRect(239, 4, 75, 16, 3, stateColor);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BACKGROUND, stateColor);
    tft.setCursor(247, 8);
    tft.print(stateText ? stateText : "UNKNOWN");
  }

  void drawPanel(Adafruit_ST7789 &tft,
                 int16_t x,
                 int16_t y,
                 int16_t w,
                 int16_t h,
                 const char *title)
  {
    tft.fillRoundRect(x, y, w, h, 4, COLOR_PANEL);
    tft.drawRoundRect(x, y, w, h, 4, COLOR_BORDER);

    if (title && title[0] != '\0')
    {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_ACCENT, COLOR_PANEL);
      tft.setCursor(x + 6, y + 4);
      tft.print(title);
    }
  }

  void drawLabelValue(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      const char *value,
                      int16_t clearWidth)
  {
    tft.fillRect(x, y, clearWidth, 10, COLOR_PANEL);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
    tft.setCursor(x, y);
    tft.print(label ? label : "");

    const int16_t valueX = x + 56;
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.setCursor(valueX, y);
    tft.print(value ? value : "-");
  }

  void drawMenuButton(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      int16_t w,
                      int16_t h,
                      const char *label,
                      bool selected)
  {
    const uint16_t fillColor = selected ? COLOR_SELECTED : COLOR_PANEL;
    const uint16_t borderColor = selected ? COLOR_ACCENT : COLOR_BORDER;

    tft.fillRoundRect(x, y, w, h, 4, fillColor);
    tft.drawRoundRect(x, y, w, h, 4, borderColor);

    tft.setTextSize(1);
    tft.setTextColor(selected ? COLOR_ACCENT : COLOR_TEXT, fillColor);

    int16_t textX = x + 8;
    if (label)
    {
      const int16_t approximateWidth = static_cast<int16_t>(strlen(label) * 6);
      textX = x + (w - approximateWidth) / 2;
    }

    tft.setCursor(textX, y + (h - 8) / 2);
    tft.print(label ? label : "");
  }

  void drawFooter(Adafruit_ST7789 &tft, const char *text)
  {
    tft.fillRect(0, 157, SCREEN_W, 13, COLOR_BACKGROUND);
    tft.drawFastHLine(0, 156, SCREEN_W, COLOR_BORDER);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
    tft.setCursor(5, 160);
    tft.print(text ? text : "");
  }
}
