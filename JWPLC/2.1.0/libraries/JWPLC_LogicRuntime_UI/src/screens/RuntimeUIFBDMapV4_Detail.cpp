#include "RuntimeUIFBDMapV4.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

void RuntimeUIFBDMapV4::drawDetailStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  // MAPA y DETALLE comparten el mismo panel exterior. No se borra toda la TFT:
  // drawDetail(true) limpia únicamente el interior común mediante clearMapArea()
  // y dibuja inmediatamente el contenido final. Así desaparece el barrido negro
  // que antes producía clearScreen().
  drawHeaderStatic(tft, "DETALLE");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

uint8_t RuntimeUIFBDMapV4::detailPageStart() const
{
  return static_cast<uint8_t>(
      (_detailInputIndex / DETAIL_INPUTS_PER_PAGE) * DETAIL_INPUTS_PER_PAGE);
}

int16_t RuntimeUIFBDMapV4::detailInputY(uint8_t visibleRow) const
{
  return static_cast<int16_t>(
      34 + visibleRow * DETAIL_SOURCE_STEP + DETAIL_SOURCE_H / 2);
}

void RuntimeUIFBDMapV4::drawDetail(bool force)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  if (definition->inputCount == 0)
  {
    _detailInputIndex = 0;
  }
  else if (_detailInputIndex >= definition->inputCount)
  {
    _detailInputIndex = static_cast<uint8_t>(definition->inputCount - 1U);
  }

  if (force)
  {
    clearMapArea();
  }

  // Toda la información central del encabezado pasa por un único hook virtual.
  // RuntimeUIFBDMapActiveRenderer lo anula y dibuja su cabecera de dos filas al
  // final del mismo callback. De esta manera nunca aparece el frame histórico
  // "Bxx TIPO IN1/1" al entrar o volver a DETALLE.
  drawMapHeaderInfo();

  drawDetailWires();

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    const int16_t y = static_cast<int16_t>(34 + row * DETAIL_SOURCE_STEP);
    JWPLC_Display.tft().fillRect(DETAIL_SOURCE_X,
                                 y,
                                 DETAIL_SOURCE_W,
                                 DETAIL_SOURCE_H,
                                 COLOR_PANEL);
    if (inputIndex < definition->inputCount)
    {
      drawDetailSource(inputIndex,
                       row,
                       inputIndex == _detailInputIndex);
    }
  }

  drawDetailBlock();
  cacheValues();
}

void RuntimeUIFBDMapV4::drawDetailSource(uint8_t inputIndex,
                                         uint8_t visibleRow,
                                         bool selected)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  const LogicV2InputLink *input =
      _model->inputLink(_selectedIndex, inputIndex);
  if (definition == nullptr || input == nullptr)
  {
    return;
  }

  const int16_t y = static_cast<int16_t>(34 + visibleRow * DETAIL_SOURCE_STEP);
  const bool active = _model->inputValue(_selectedIndex, inputIndex);
  const uint16_t border = selected
                              ? COLOR_WARNING
                              : (active ? COLOR_OK : COLOR_BORDER);

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(DETAIL_SOURCE_X,
               y,
               DETAIL_SOURCE_W,
               DETAIL_SOURCE_H,
               COLOR_BACKGROUND);
  tft.drawRect(DETAIL_SOURCE_X,
               y,
               DETAIL_SOURCE_W,
               DETAIL_SOURCE_H,
               border);
  if (selected)
  {
    tft.drawRect(DETAIL_SOURCE_X + 1,
                 y + 1,
                 DETAIL_SOURCE_W - 2,
                 DETAIL_SOURCE_H - 2,
                 border);
  }

  char sourceId[12];
  char sourceType[12];
  const uint16_t source = input->source();
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(sourceId, sizeof(sourceId), "X");
    std::snprintf(sourceType, sizeof(sourceType), "ABIERTO");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(sourceId, sizeof(sourceId), "HI");
    std::snprintf(sourceType, sizeof(sourceType), "CONST 1");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(sourceId, sizeof(sourceId), "LO");
    std::snprintf(sourceType, sizeof(sourceType), "CONST 0");
  }
  else
  {
    formatBlockId(sourceId, sizeof(sourceId), source);
    formatMapLine2(sourceType, sizeof(sourceType), source);
  }

  const char *role = _model->inputRole(definition->type, inputIndex);
  char roleText[6];
  if (role != nullptr && role[0] != '\0')
  {
    std::snprintf(roleText, sizeof(roleText), "%s", role);
  }
  else
  {
    std::snprintf(roleText,
                  sizeof(roleText),
                  "%u",
                  static_cast<unsigned>(inputIndex + 1U));
  }

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_SOURCE_X + 4, y + 3);
  tft.print(roleText);
  tft.setCursor(DETAIL_SOURCE_X + 20, y + 3);
  tft.print(sourceId);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_SOURCE_X + 20, y + 13);
  tft.print(sourceType);
}

void RuntimeUIFBDMapV4::drawDetailWires()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    if (inputIndex >= definition->inputCount)
    {
      continue;
    }

    const bool active = _model->inputValue(_selectedIndex, inputIndex);
    const uint16_t color = active ? COLOR_OK : COLOR_MUTED;
    const int16_t sourceY = static_cast<int16_t>(
        34 + row * DETAIL_SOURCE_STEP + DETAIL_SOURCE_H / 2);
    const int16_t destinationY = static_cast<int16_t>(
        DETAIL_BLOCK_Y + 17 + row * 18);
    const int16_t sourceX = static_cast<int16_t>(
        DETAIL_SOURCE_X + DETAIL_SOURCE_W);
    const int16_t destinationX = DETAIL_BLOCK_X;
    const int16_t routeX = static_cast<int16_t>(
        sourceX + 46 + row * 7);

    drawOrthogonalWireClipped(sourceX,
                              sourceY,
                              destinationX,
                              destinationY,
                              routeX,
                              color);
  }
}

void RuntimeUIFBDMapV4::drawDetailBlock()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const bool active = _model->blockValue(_selectedIndex);
  const uint16_t border = active ? COLOR_OK : COLOR_BORDER;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  tft.fillRect(DETAIL_BLOCK_X,
               DETAIL_BLOCK_Y,
               DETAIL_BLOCK_W,
               DETAIL_BLOCK_H,
               COLOR_BACKGROUND);
  tft.drawRect(DETAIL_BLOCK_X,
               DETAIL_BLOCK_Y,
               DETAIL_BLOCK_W,
               DETAIL_BLOCK_H,
               border);
  if (definition->inputCount > 0)
  {
    tft.drawFastVLine(DETAIL_BLOCK_X + DETAIL_GUTTER_W,
                      DETAIL_BLOCK_Y + 2,
                      DETAIL_BLOCK_H - 4,
                      COLOR_BORDER);
  }

  char blockId[8];
  formatBlockId(blockId, sizeof(blockId), _selectedIndex);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 8,
                DETAIL_BLOCK_Y + 7);
  tft.print(blockId);

  tft.setTextSize(2);
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT,
                   COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 19,
                DETAIL_BLOCK_Y + 32);
  tft.print(detailSymbol(definition->type));

  tft.setTextSize(1);
  if (definition->type == LogicV2BlockType::Ton)
  {
    char parameter[16];
    std::snprintf(parameter,
                  sizeof(parameter),
                  "%lums",
                  static_cast<unsigned long>(definition->parameter));
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                     COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 8,
                  DETAIL_BLOCK_Y + 67);
    tft.print(parameter);
  }
  else if (definition->type == LogicV2BlockType::DigitalInput ||
           definition->type == LogicV2BlockType::DigitalOutput)
  {
    char resource[10];
    formatMapLine2(resource, sizeof(resource), _selectedIndex);
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                     COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 14,
                  DETAIL_BLOCK_Y + 67);
    tft.print(resource);
  }

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    if (inputIndex >= definition->inputCount)
    {
      continue;
    }

    const LogicV2InputLink *input =
        _model->inputLink(_selectedIndex, inputIndex);
    if (input == nullptr)
    {
      continue;
    }

    const int16_t pinY = static_cast<int16_t>(
        DETAIL_BLOCK_Y + 17 + row * 18);
    const bool inputActive = _model->inputValue(_selectedIndex, inputIndex);
    const uint16_t pinColor = inputActive ? COLOR_OK : COLOR_MUTED;

    if (input->inverted())
    {
      tft.fillCircle(DETAIL_BLOCK_X + 1,
                     pinY,
                     4,
                     COLOR_BACKGROUND);
      tft.drawCircle(DETAIL_BLOCK_X + 1,
                     pinY,
                     3,
                     COLOR_MUTED);
    }
    else
    {
      tft.fillCircle(DETAIL_BLOCK_X,
                     pinY,
                     1,
                     pinColor);
    }

    const char *role = _model->inputRole(definition->type, inputIndex);
    char pinLabel[5];
    if (role != nullptr && role[0] != '\0')
    {
      std::snprintf(pinLabel, sizeof(pinLabel), "%s", role);
    }
    else
    {
      std::snprintf(pinLabel,
                    sizeof(pinLabel),
                    "%u",
                    static_cast<unsigned>(inputIndex + 1U));
    }

    tft.setTextSize(1);
    tft.setTextColor(pinColor, COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + 7, pinY - 3);
    tft.print(pinLabel);
  }

  tft.fillCircle(DETAIL_BLOCK_X + DETAIL_BLOCK_W,
                 DETAIL_BLOCK_Y + DETAIL_BLOCK_H / 2,
                 2,
                 active ? COLOR_OK : COLOR_MUTED);
}
