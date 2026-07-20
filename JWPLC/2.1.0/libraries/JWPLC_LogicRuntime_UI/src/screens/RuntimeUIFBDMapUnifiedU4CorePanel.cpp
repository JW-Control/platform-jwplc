#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace JWPLCUnifiedU4
{
void drawContextPanel(const uint8_t *levels,
                      const uint8_t *lanes,
                      uint8_t maxLevel,
                      int16_t panelY,
                      int16_t panelH,
                      bool sourceContext,
                      uint16_t source,
                      const char *message,
                      uint16_t messageColor)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(CONTEXT_X, panelY, CONTEXT_W, panelH, COLOR_PANEL);
  tft.drawRect(CONTEXT_X, panelY, CONTEXT_W, panelH, COLOR_BORDER);

  if (!sourceContext)
  {
    drawFieldLabel(tft,
                   CONTEXT_X + 6,
                   panelY + 6,
                   message != nullptr ? message : "SIN CONTEXTO",
                   messageColor,
                   COLOR_PANEL);
    return;
  }

  char sourceText[28];
  formatSource(gU4.model, source, sourceText, sizeof(sourceText));
  if (source >= (gU4.model != nullptr ? gU4.model->blockCount() : 0U))
  {
    char info[48];
    std::snprintf(info, sizeof(info), "%s - SIN POSICION FBD", sourceText);
    drawFieldLabel(tft,
                   CONTEXT_X + 6,
                   panelY + 6,
                   info,
                   COLOR_MUTED,
                   COLOR_PANEL);
    return;
  }

  char info[48];
  const LogicV2BlockRecord *definition = gU4.model->block(source);
  std::snprintf(info,
                sizeof(info),
                "B%02u %s   F%02u C%02u",
                static_cast<unsigned>(source),
                definition != nullptr ? gU4.model->typeShort(definition->type) : "?",
                static_cast<unsigned>(lanes[source] + 1U),
                static_cast<unsigned>(levels[source] + 1U));
  drawFieldLabel(tft,
                 CONTEXT_X + 6,
                 panelY + 5,
                 info,
                 COLOR_WARNING,
                 COLOR_PANEL);
  drawMiniMap(gU4.model,
              source,
              levels,
              lanes,
              maxLevel,
              CONTEXT_X + 5,
              panelY + 18,
              CONTEXT_W - 10,
              panelH - 23);
}

void drawTonField(uint8_t index,
                  const char *label,
                  const char *value,
                  bool selected,
                  bool full)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const int16_t x = TON_FIELD_X[index];
  const int16_t w = TON_FIELD_W[index];
  const uint16_t background = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t foreground = selected ? COLOR_WARNING : COLOR_TEXT;
  const uint16_t labelColor = selected ? COLOR_WARNING : COLOR_MUTED;

  if (full)
  {
    tft.fillRoundRect(x, TON_FIELD_Y, w, TON_FIELD_H, 4, background);
    tft.drawRoundRect(x,
                      TON_FIELD_Y,
                      w,
                      TON_FIELD_H,
                      4,
                      selected ? COLOR_WARNING : COLOR_BORDER);
    if (selected)
      tft.drawRoundRect(x + 1,
                        TON_FIELD_Y + 1,
                        w - 2,
                        TON_FIELD_H - 2,
                        3,
                        COLOR_WARNING);
  }

  tft.fillRect(x + 5, TON_FIELD_Y + 5, w - 10, 10, background);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(labelColor, background);
  tft.setCursor(x + 7, TON_FIELD_Y + 5);
  tft.print(label);

  tft.fillRect(x + 5, TON_FIELD_Y + 22, w - 10, 10, background);
  const int16_t valueWidth = static_cast<int16_t>(std::strlen(value) * 6U);
  tft.setTextColor(foreground, background);
  tft.setCursor(static_cast<int16_t>(x + (w - valueWidth) / 2), TON_FIELD_Y + 22);
  tft.print(value);
}

void formatTwoDigit(uint32_t value, char *destination, size_t capacity)
{
  std::snprintf(destination,
                capacity,
                "<%02lu>",
                static_cast<unsigned long>(value));
}

LogicV2InputLink makeLink(uint16_t source)
{
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
    return LogicV2InputLink::open();
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
    return LogicV2InputLink::constantTrue();
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
    return LogicV2InputLink::constantFalse();
  return LogicV2InputLink::block(source);
}
} // namespace JWPLCUnifiedU4
