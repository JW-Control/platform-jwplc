#include "RuntimeUIWidgets.h"

#include <cstring>

namespace
{
  static constexpr uint8_t MAX_FIXED_TEXT_COLUMNS = 52;

  void buildFixedText(const char *source,
                      uint8_t requestedColumns,
                      char *destination,
                      size_t destinationCapacity)
  {
    if (destination == nullptr || destinationCapacity == 0)
    {
      return;
    }

    const uint8_t columns =
        requestedColumns < MAX_FIXED_TEXT_COLUMNS
            ? requestedColumns
            : MAX_FIXED_TEXT_COLUMNS;

    const char *safeSource = source ? source : "";
    const size_t sourceLength = std::strlen(safeSource);
    const size_t copyLength = sourceLength < columns ? sourceLength : columns;

    if (copyLength > 0)
    {
      std::memcpy(destination, safeSource, copyLength);
    }

    for (size_t index = copyLength; index < columns; ++index)
    {
      destination[index] = ' ';
    }

    destination[columns] = '\0';
  }
}

namespace JWPLCLogicRuntimeUIWidgets
{
  void clearScreen(Adafruit_ST7789 &tft)
  {
    tft.fillScreen(COLOR_BACKGROUND);
  }

  void drawHeaderStatic(Adafruit_ST7789 &tft,
                        const char *title)
  {
    tft.fillRect(0, 0, SCREEN_W, 24, COLOR_PANEL);
    tft.drawFastHLine(0, 23, SCREEN_W, COLOR_BORDER);

    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.setCursor(6, 5);
    tft.print(title ? title : "JWPLC LOGIC");
  }

  void updateHeaderState(Adafruit_ST7789 &tft,
                         const char *stateText,
                         uint16_t stateColor)
  {
    char fixedState[12];
    buildFixedText(stateText ? stateText : "UNKNOWN",
                   11,
                   fixedState,
                   sizeof(fixedState));

    tft.fillRoundRect(239, 4, 75, 16, 3, stateColor);
    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BACKGROUND, stateColor);
    tft.setCursor(247, 8);
    tft.print(fixedState);
  }

  void drawHeader(Adafruit_ST7789 &tft,
                  const char *title,
                  const char *stateText,
                  uint16_t stateColor)
  {
    drawHeaderStatic(tft, title);
    updateHeaderState(tft, stateText, stateColor);
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
      tft.setTextWrap(false);
      tft.setTextSize(1);
      tft.setTextColor(COLOR_ACCENT, COLOR_PANEL);
      tft.setCursor(x + 6, y + 4);
      tft.print(title);
    }
  }

  void drawFieldLabel(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      uint16_t foreground,
                      uint16_t background)
  {
    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(foreground, background);
    tft.setCursor(x, y);
    tft.print(label ? label : "");
  }

  void updateTextField(Adafruit_ST7789 &tft,
                       int16_t x,
                       int16_t y,
                       uint8_t columns,
                       const char *value,
                       uint16_t foreground,
                       uint16_t background)
  {
    if (columns == 0)
    {
      return;
    }

    char fixedText[MAX_FIXED_TEXT_COLUMNS + 1];
    buildFixedText(value,
                   columns,
                   fixedText,
                   sizeof(fixedText));

    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(foreground, background);
    tft.setCursor(x, y);
    tft.print(fixedText);
  }

  void drawLabelValue(Adafruit_ST7789 &tft,
                      int16_t x,
                      int16_t y,
                      const char *label,
                      const char *value,
                      int16_t clearWidth)
  {
    drawFieldLabel(tft, x, y, label);

    const int16_t valueX = x + 56;
    const int16_t valueWidth = clearWidth > 56 ? clearWidth - 56 : 6;
    const uint8_t columns = static_cast<uint8_t>(valueWidth / 6);
    updateTextField(tft,
                    valueX,
                    y,
                    columns > 0 ? columns : 1,
                    value);
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

    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setTextColor(selected ? COLOR_ACCENT : COLOR_TEXT, fillColor);

    int16_t textX = x + 8;
    if (label)
    {
      const int16_t approximateWidth =
          static_cast<int16_t>(std::strlen(label) * 6U);
      textX = x + (w - approximateWidth) / 2;
    }

    tft.setCursor(textX, y + (h - 8) / 2);
    tft.print(label ? label : "");
  }

  void drawFooter(Adafruit_ST7789 &tft, const char *text)
  {
    tft.drawFastHLine(0, 156, SCREEN_W, COLOR_BORDER);
    updateTextField(tft,
                    5,
                    160,
                    51,
                    text,
                    COLOR_MUTED,
                    COLOR_BACKGROUND);
  }
}
