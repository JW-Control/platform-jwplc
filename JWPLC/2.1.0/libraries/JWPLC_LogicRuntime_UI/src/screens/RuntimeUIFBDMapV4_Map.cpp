#include "RuntimeUIFBDMapV4.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  int16_t minimum16(int16_t a, int16_t b) { return a < b ? a : b; }
  int16_t maximum16(int16_t a, int16_t b) { return a > b ? a : b; }
  int16_t clamp16(int16_t value, int16_t minimum, int16_t maximum)
  {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
  }
}

void RuntimeUIFBDMapV4::drawMapStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "MAPA FBD");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);
  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMapV4::clearMapArea()
{
  JWPLC_Display.tft().fillRect(MAP_X,
                               MAP_Y,
                               MAP_W,
                               MAP_H,
                               COLOR_PANEL);
}

void RuntimeUIFBDMapV4::drawMapHeaderInfo()
{
  char selectedText[32];

  if (_model == nullptr || _model->blockCount() == 0)
  {
    std::snprintf(selectedText,
                  sizeof(selectedText),
                  "SIN PROGRAMA");
  }
  else
  {
    std::snprintf(selectedText,
                  sizeof(selectedText),
                  "B%02u %u/%u %d,%d",
                  static_cast<unsigned>(_selectedIndex),
                  static_cast<unsigned>(_selectedIndex + 1U),
                  static_cast<unsigned>(_model->blockCount()),
                  static_cast<int>(_viewportX),
                  static_cast<int>(_viewportY));
  }

  updateTextField(JWPLC_Display.tft(),
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  selectedText,
                  COLOR_MUTED,
                  COLOR_PANEL);
}

void RuntimeUIFBDMapV4::drawMapFull()
{
  if (_model == nullptr)
  {
    return;
  }

  clearMapArea();
  drawMapHeaderInfo();

  if (_model->blockCount() == 0)
  {
    updateTextField(JWPLC_Display.tft(),
                    MAP_X + 80,
                    MAP_Y + 58,
                    24,
                    "SIN PROGRAMA V2",
                    COLOR_WARNING,
                    COLOR_PANEL);
    return;
  }

  drawWires();
  drawNodes();
  drawEdgeHints();
  cacheValues();
}

void RuntimeUIFBDMapV4::drawMapLive()
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    return;
  }

  drawWires();
  drawNodes();
  drawEdgeHints();
  cacheValues();
}

void RuntimeUIFBDMapV4::drawClippedHorizontal(int16_t x0,
                                               int16_t x1,
                                               int16_t y,
                                               uint16_t color)
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  if (y < MAP_Y || y > mapBottom)
  {
    return;
  }

  const int16_t left = maximum16(minimum16(x0, x1), MAP_X);
  const int16_t right = minimum16(maximum16(x0, x1), mapRight);
  if (left > right)
  {
    return;
  }

  JWPLC_Display.tft().drawFastHLine(
      left,
      y,
      static_cast<int16_t>(right - left + 1),
      color);
}

void RuntimeUIFBDMapV4::drawClippedVertical(int16_t x,
                                             int16_t y0,
                                             int16_t y1,
                                             uint16_t color)
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  if (x < MAP_X || x > mapRight)
  {
    return;
  }

  const int16_t top = maximum16(minimum16(y0, y1), MAP_Y);
  const int16_t bottom = minimum16(maximum16(y0, y1), mapBottom);
  if (top > bottom)
  {
    return;
  }

  JWPLC_Display.tft().drawFastVLine(
      x,
      top,
      static_cast<int16_t>(bottom - top + 1),
      color);
}

void RuntimeUIFBDMapV4::drawOrthogonalWireClipped(int16_t x0,
                                                   int16_t y0,
                                                   int16_t x1,
                                                   int16_t y1,
                                                   int16_t routeX,
                                                   uint16_t color)
{
  drawClippedHorizontal(x0, routeX, y0, color);
  drawClippedVertical(routeX, y0, y1, color);
  drawClippedHorizontal(routeX, x1, y1, color);
}

void RuntimeUIFBDMapV4::drawWires()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    const LogicV2BlockRecord *definition = _model->block(consumer);
    if (definition == nullptr)
    {
      continue;
    }

    for (uint8_t input = 0; input < definition->inputCount; ++input)
    {
      drawWire(consumer, input);
    }
  }
}

void RuntimeUIFBDMapV4::drawWire(uint16_t consumerIndex,
                                  uint8_t inputIndex)
{
  const LogicV2BlockRecord *consumer = _model->block(consumerIndex);
  const LogicV2InputLink *input =
      _model->inputLink(consumerIndex, inputIndex);
  if (consumer == nullptr || input == nullptr)
  {
    return;
  }

  const int16_t consumerNodeX = screenX(consumerIndex);
  const int16_t consumerNodeY = screenY(consumerIndex);
  const int16_t destinationX = consumerNodeX;
  const int16_t destinationY = static_cast<int16_t>(
      consumerNodeY + inputPortY(*consumer, inputIndex));
  const uint16_t sourceIndex = input->source();

  if (sourceIndex == JWPLC_LOGIC_V2_SOURCE_OPEN ||
      sourceIndex == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE ||
      sourceIndex == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    const uint16_t color = _model->inputValue(consumerIndex, inputIndex)
                               ? COLOR_OK
                               : COLOR_MUTED;
    drawClippedHorizontal(destinationX - 7,
                          destinationX,
                          destinationY,
                          color);
    return;
  }

  if (sourceIndex >= _model->blockCount())
  {
    return;
  }

  const int16_t sourceNodeX = screenX(sourceIndex);
  const int16_t sourceNodeY = screenY(sourceIndex);
  const int16_t sourceX = static_cast<int16_t>(sourceNodeX + NODE_W);
  const int16_t sourceY = static_cast<int16_t>(sourceNodeY + NODE_H / 2);

  int16_t routeX = static_cast<int16_t>((sourceX + destinationX) / 2);
  const int16_t gap = static_cast<int16_t>(destinationX - sourceX);
  if (gap >= 8)
  {
    const uint8_t divisor = static_cast<uint8_t>(consumer->inputCount + 1U);
    routeX = static_cast<int16_t>(
        sourceX +
        (static_cast<int32_t>(inputIndex + 1U) * gap) / divisor);
    routeX = clamp16(routeX,
                     static_cast<int16_t>(sourceX + 2),
                     static_cast<int16_t>(destinationX - 2));
  }

  const uint16_t color = _model->blockValue(sourceIndex)
                             ? COLOR_OK
                             : COLOR_MUTED;
  drawOrthogonalWireClipped(sourceX,
                            sourceY,
                            destinationX,
                            destinationY,
                            routeX,
                            color);
}

void RuntimeUIFBDMapV4::drawNodes()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    drawNode(blockIndex);
  }
}

void RuntimeUIFBDMapV4::drawNode(uint16_t blockIndex)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    return;
  }

  const int16_t x = screenX(blockIndex);
  const int16_t y = screenY(blockIndex);
  if (!nodeFullyVisible(x, y))
  {
    return;
  }

  const bool active = _model->blockValue(blockIndex);
  const bool selected = blockIndex == _selectedIndex;
  const uint16_t border = selected
                              ? COLOR_WARNING
                              : (active ? COLOR_OK : COLOR_BORDER);
  drawFullNode(blockIndex, x, y, border, active, selected);
}

void RuntimeUIFBDMapV4::drawFullNode(uint16_t blockIndex,
                                     int16_t x,
                                     int16_t y,
                                     uint16_t border,
                                     bool active,
                                     bool selected)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(x, y, NODE_W, NODE_H, COLOR_BACKGROUND);
  tft.drawRect(x, y, NODE_W, NODE_H, border);
  if (selected)
  {
    tft.drawRect(x + 1, y + 1, NODE_W - 2, NODE_H - 2, border);
  }

  const bool hasInputs = definition->inputCount > 0;
  const int16_t gutter = hasInputs ? NODE_GUTTER_W : 0;
  if (hasInputs)
  {
    tft.drawFastVLine(x + NODE_GUTTER_W,
                      y + 2,
                      NODE_H - 4,
                      COLOR_BORDER);
  }

  char blockId[8];
  char line2[10];
  formatBlockId(blockId, sizeof(blockId), blockIndex);
  formatMapLine2(line2, sizeof(line2), blockIndex);

  const int16_t contentX = static_cast<int16_t>(x + gutter);
  const int16_t contentW = static_cast<int16_t>(NODE_W - gutter);
  const int16_t idX = static_cast<int16_t>(
      contentX + maximum16(1, (contentW - static_cast<int16_t>(std::strlen(blockId) * 6)) / 2));
  const int16_t line2X = static_cast<int16_t>(
      contentX + maximum16(1, (contentW - static_cast<int16_t>(std::strlen(line2) * 6)) / 2));

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(idX, y + 5);
  tft.print(blockId);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(line2X, y + 17);
  tft.print(line2);

  drawNodePorts(blockIndex, x, y);
}

void RuntimeUIFBDMapV4::drawEdgeHints()
{
  const GridRange range = visibleGridRange();
  if (!range.valid)
  {
    return;
  }

  const uint16_t count = _model->blockCount();
  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    bool leftSide = false;
    if (!shouldDrawEdgeHint(blockIndex, range, leftSide))
    {
      continue;
    }

    const bool active = _model->blockValue(blockIndex);
    const bool selected = blockIndex == _selectedIndex;
    const uint16_t border = selected
                                ? COLOR_WARNING
                                : (active ? COLOR_OK : COLOR_BORDER);
    drawEdgeHint(blockIndex,
                 leftSide,
                 screenY(blockIndex),
                 border,
                 active);
  }
}

void RuntimeUIFBDMapV4::drawEdgeHint(uint16_t blockIndex,
                                     bool leftSide,
                                     int16_t nodeScreenY,
                                     uint16_t border,
                                     bool active)
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  const int16_t hintX = leftSide
                            ? MAP_X
                            : static_cast<int16_t>(mapRight - EDGE_HINT_W + 1);
  const int16_t hintY = clamp16(
      nodeScreenY,
      MAP_Y,
      static_cast<int16_t>(mapBottom - EDGE_HINT_H + 1));

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(hintX,
               hintY,
               EDGE_HINT_W,
               EDGE_HINT_H,
               COLOR_BACKGROUND);
  tft.drawRect(hintX,
               hintY,
               EDGE_HINT_W,
               EDGE_HINT_H,
               border);

  char blockId[8];
  char line2[10];
  char firstLine[10];
  formatBlockId(blockId, sizeof(blockId), blockIndex);
  formatMapLine2(line2, sizeof(line2), blockIndex);
  std::snprintf(firstLine,
                sizeof(firstLine),
                leftSide ? "<%s" : "%s>",
                blockId);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT,
                   COLOR_BACKGROUND);
  tft.setCursor(hintX + 3, hintY + 3);
  tft.print(firstLine);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                   COLOR_BACKGROUND);
  tft.setCursor(hintX + 9, hintY + 13);
  tft.print(line2);
}

void RuntimeUIFBDMapV4::drawNodePorts(uint16_t blockIndex,
                                      int16_t x,
                                      int16_t y)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount;
       ++inputIndex)
  {
    const LogicV2InputLink *input =
        _model->inputLink(blockIndex, inputIndex);
    if (input == nullptr)
    {
      continue;
    }

    const int16_t portY = static_cast<int16_t>(
        y + inputPortY(*definition, inputIndex));
    const bool inputActive = _model->inputValue(blockIndex, inputIndex);
    const uint16_t pinColor = inputActive ? COLOR_OK : COLOR_MUTED;

    if (input->inverted())
    {
      tft.fillCircle(x + 1, portY, 3, COLOR_BACKGROUND);
      tft.drawCircle(x + 1, portY, 2, COLOR_MUTED);
    }
    else
    {
      tft.fillCircle(x, portY, 1, pinColor);
    }

    const char *role = nullptr;
    if (definition->type == LogicV2BlockType::SetReset)
    {
      role = inputIndex == 0 ? "S" : "R";
    }
    else if (definition->type == LogicV2BlockType::Ton)
    {
      role = "T";
    }

    if (role != nullptr)
    {
      tft.setTextWrap(false);
      tft.setTextSize(1);
      tft.setTextColor(pinColor, COLOR_BACKGROUND);
      tft.setCursor(x + 4, portY - 3);
      tft.print(role);
    }
  }

  const int16_t outputY = static_cast<int16_t>(y + NODE_H / 2);
  tft.fillCircle(x + NODE_W,
                 outputY,
                 1,
                 _model->blockValue(blockIndex) ? COLOR_OK : COLOR_MUTED);
}
