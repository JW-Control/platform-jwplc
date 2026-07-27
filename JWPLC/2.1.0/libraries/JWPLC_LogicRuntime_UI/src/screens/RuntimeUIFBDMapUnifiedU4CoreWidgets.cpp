#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace JWPLCUnifiedU4
{
void drawFooter(const char *text, uint16_t color)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.drawFastHLine(5, FOOTER_Y - 4, 308, COLOR_BORDER);
  updateTextField(tft, 10, FOOTER_Y, 48, text, color, COLOR_PANEL);
}

void drawTypeChoice(U4Type type, bool selected)
{
  const uint8_t index = static_cast<uint8_t>(type);
  drawMenuButton(JWPLC_Display.tft(),
                 TYPE_X,
                 static_cast<int16_t>(TYPE_Y + index * TYPE_STEP),
                 TYPE_W,
                 TYPE_H,
                 typeName(type),
                 selected);
}

void drawDisabledGroup(uint8_t index, const char *label)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const int16_t x = GROUP_X[index];
  tft.fillRoundRect(x, GROUP_Y, GROUP_W, GROUP_H, 4, COLOR_PANEL);
  tft.drawRoundRect(x, GROUP_Y, GROUP_W, GROUP_H, 4, COLOR_BACKGROUND);
  const int16_t width = static_cast<int16_t>(std::strlen(label) * 6U);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(0x52AA, COLOR_PANEL);
  tft.setCursor(static_cast<int16_t>(x + (GROUP_W - width) / 2), GROUP_Y + 8);
  tft.print(label);
}

void drawMainGroup(U4MainFocus focus)
{
  const uint8_t index = static_cast<uint8_t>(focus);
  const char *label = mainFocusName(focus);
  if (!mainFocusSelectable(focus))
  {
    drawDisabledGroup(index, label);
    return;
  }
  drawMenuButton(JWPLC_Display.tft(),
                 GROUP_X[index],
                 GROUP_Y,
                 GROUP_W,
                 GROUP_H,
                 label,
                 gU4.mainFocus == focus);
}

void drawListRow(uint8_t input, bool selected)
{
  char source[28];
  char label[48];
  formatSource(gU4.model, sourceReference(input), source, sizeof(source));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                sourceRole(gU4.type, input),
                source);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 static_cast<int16_t>(LIST_Y + input * LIST_STEP),
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 selected);
}

} // namespace JWPLCUnifiedU4
