#include "RuntimeUIFBDMap.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

void RuntimeUIFBDMap::drawDetailFull(bool clearInterior)
{
  if (_model == nullptr || _model->block(_selectedIndex) == nullptr)
  {
    return;
  }
  if (clearInterior)
  {
    clearMapArea();
  }
  normalizeDetailFocus();
  drawDetailWires();
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
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
                       _detailFocus == DetailFocus::Inputs &&
                           inputIndex == _detailInputIndex);
    }
  }
  drawDetailBlock();
  if (definition->type == LogicV2BlockType::Ton)
  {
    drawTonDetailPanel(_detailFocus == DetailFocus::Parameters, true);
  }
  _detailCacheValid = false;
}

void RuntimeUIFBDMap::drawDetailLive()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  bool redrawBlock = !_detailCacheValid ||
                     _detailCacheBlock != _selectedIndex ||
                     _detailBlockValueCache != _model->blockValue(_selectedIndex);
  const bool inputValue = definition->inputCount > 0
                              ? _model->inputValue(_selectedIndex,
                                                  _detailInputIndex)
                              : false;
  const bool redrawInput = !_detailCacheValid ||
                           _detailCacheBlock != _selectedIndex ||
                           _detailCacheInput != _detailInputIndex ||
                           _detailCacheFocus != _detailFocus ||
                           _detailInputValueCache != inputValue;

  if (redrawInput && definition->inputCount > 0)
  {
    const uint8_t pageStart = detailPageStart();
    const uint8_t row = static_cast<uint8_t>(_detailInputIndex - pageStart);
    if (row < DETAIL_INPUTS_PER_PAGE)
    {
      drawDetailSource(_detailInputIndex,
                       row,
                       _detailFocus == DetailFocus::Inputs);
    }
  }
  if (redrawBlock)
  {
    drawDetailBlock();
  }
  if (definition->type == LogicV2BlockType::Ton)
  {
    drawTonDetailPanel(_detailFocus == DetailFocus::Parameters, false);
  }

  _detailCacheBlock = _selectedIndex;
  _detailCacheInput = _detailInputIndex;
  _detailCacheFocus = _detailFocus;
  _detailBlockValueCache = _model->blockValue(_selectedIndex);
  _detailInputValueCache = inputValue;
  _detailCacheValid = true;
}

void RuntimeUIFBDMap::drawDetailSource(uint8_t inputIndex,
                                       uint8_t visibleRow,
                                       bool selected)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  const LogicV2InputLink *input = _model->inputLink(_selectedIndex, inputIndex);
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
  formatSource(input->source(),
               sourceId,
               sizeof(sourceId),
               sourceType,
               sizeof(sourceType));
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

void RuntimeUIFBDMap::drawDetailWires()
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
    const int16_t routeX = static_cast<int16_t>(sourceX + 46 + row * 7);
    drawOrthogonalWireClipped(sourceX,
                              sourceY,
                              DETAIL_BLOCK_X,
                              destinationY,
                              routeX,
                              color);
  }
}

void RuntimeUIFBDMap::drawDetailBlock()
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
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 8,
                DETAIL_BLOCK_Y + 7);
  tft.print(blockId);
  tft.setTextSize(2);
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 19,
                DETAIL_BLOCK_Y + 32);
  tft.print(detailSymbol(definition->type));

  if (definition->type == LogicV2BlockType::DigitalInput ||
      definition->type == LogicV2BlockType::DigitalOutput)
  {
    char resource[10];
    formatMapLine2(resource, sizeof(resource), _selectedIndex);
    tft.setTextSize(1);
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
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
    const LogicV2InputLink *input = _model->inputLink(_selectedIndex,
                                                       inputIndex);
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
      tft.fillCircle(DETAIL_BLOCK_X + 1, pinY, 4, COLOR_BACKGROUND);
      tft.drawCircle(DETAIL_BLOCK_X + 1, pinY, 3, COLOR_MUTED);
    }
    else
    {
      tft.fillCircle(DETAIL_BLOCK_X, pinY, 1, pinColor);
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

void RuntimeUIFBDMap::drawTonDetailPanel(bool selected, bool force)
{
  if (_model == nullptr || !_model->isTon(_selectedIndex))
  {
    return;
  }
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  char configured[20];
  char elapsed[20];
  formatTonMilliseconds(definition->parameter,
                        definition->resource,
                        configured,
                        sizeof(configured));
  uint32_t elapsedMajor = 0;
  uint32_t elapsedMinor = 0;
  const TonBase base = effectiveTonBase(definition->parameter,
                                        definition->resource);
  decodeTonMilliseconds(_model->tonElapsedMs(_selectedIndex, millis()),
                        base,
                        elapsedMajor,
                        elapsedMinor);
  formatTon(elapsedMajor,
            elapsedMinor,
            base,
            elapsed,
            sizeof(elapsed));

  const bool configuredChanged = force ||
      !_detailCacheValid ||
      _detailTonSelectedCache != selected ||
      std::strcmp(_detailTonConfiguredCache, configured) != 0;
  const bool elapsedChanged = force ||
      !_detailCacheValid ||
      std::strcmp(_detailTonElapsedCache, elapsed) != 0;

  static constexpr int16_t x = DETAIL_BLOCK_X + DETAIL_GUTTER_W + 4;
  static constexpr int16_t y = DETAIL_BLOCK_Y + 57;
  static constexpr int16_t w = DETAIL_BLOCK_W - DETAIL_GUTTER_W - 8;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  if (configuredChanged)
  {
    tft.fillRect(x, y, w, 14, COLOR_BACKGROUND);
    if (selected)
    {
      tft.drawRect(x, y, w, 14, COLOR_WARNING);
      tft.drawRect(x + 1, y + 1, w - 2, 12, COLOR_WARNING);
    }
    tft.setTextSize(1);
    tft.setTextColor(selected ? COLOR_WARNING : COLOR_TEXT,
                     COLOR_BACKGROUND);
    tft.setCursor(x + 4, y + 4);
    tft.print("T ");
    tft.print(configured);
  }
  if (elapsedChanged)
  {
    tft.fillRect(x, y + 15, w, 14, COLOR_BACKGROUND);
    const bool active = _model->tonTiming(_selectedIndex) ||
                        _model->blockValue(_selectedIndex);
    tft.setTextSize(1);
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                     COLOR_BACKGROUND);
    tft.setCursor(x + 4, y + 19);
    tft.print("Ta ");
    tft.print(elapsed);
  }

  _detailTonSelectedCache = selected;
  std::snprintf(_detailTonConfiguredCache,
                sizeof(_detailTonConfiguredCache),
                "%s",
                configured);
  std::snprintf(_detailTonElapsedCache,
                sizeof(_detailTonElapsedCache),
                "%s",
                elapsed);
}
