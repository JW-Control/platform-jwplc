#include "RuntimeUIFBDMapUnified.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
int16_t minimum16Unified(int16_t a, int16_t b) { return a < b ? a : b; }
int16_t maximum16Unified(int16_t a, int16_t b) { return a > b ? a : b; }

int16_t clamp16Unified(int16_t value, int16_t minimum, int16_t maximum)
{
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

uint16_t absoluteDistanceUnified(int16_t a, int16_t b)
{
  return static_cast<uint16_t>(a > b ? a - b : b - a);
}

uint8_t minimum8Unified(uint8_t a, uint8_t b) { return a < b ? a : b; }
uint8_t maximum8Unified(uint8_t a, uint8_t b) { return a > b ? a : b; }
}

void RuntimeUIFBDMapUnified::buildLayout()
{
  if (_model == nullptr)
  {
    return;
  }

  uint8_t lanes[MAX_LEVELS] = {};
  const uint16_t count = _model->blockCount();
  _maxLevel = 0;

  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    const LogicV2BlockRecord *definition = _model->block(blockIndex);
    uint8_t level = 0;

    if (definition != nullptr)
    {
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

        const uint16_t source = input->source();
        if (source < blockIndex)
        {
          const uint8_t candidate =
              _levels[source] >= MAX_LEVELS - 1U
                  ? static_cast<uint8_t>(MAX_LEVELS - 1U)
                  : static_cast<uint8_t>(_levels[source] + 1U);
          if (candidate > level)
          {
            level = candidate;
          }
        }
      }
    }

    _levels[blockIndex] = level;
    _lanes[blockIndex] = lanes[level]++;
    _nodeX[blockIndex] = static_cast<int16_t>(level) * SLOT_STEP;
    _nodeY[blockIndex] = static_cast<int16_t>(
        WORLD_MARGIN_Y + static_cast<int16_t>(_lanes[blockIndex]) * ROW_STEP);

    if (level > _maxLevel)
    {
      _maxLevel = level;
    }
  }

  if (_maxLevel <= 4)
  {
    _horizontalMode = HorizontalWindowMode::LeftEdge;
    _centralStartLevel = 1;
  }
  else if (_centralStartLevel > static_cast<uint8_t>(_maxLevel - 3U))
  {
    _centralStartLevel = static_cast<uint8_t>(_maxLevel - 3U);
  }

  _layoutBlockCount = count;
  _layoutLinkCount = _model->linkCount();
  _layoutValid = true;
  invalidateMapCache();
}

void RuntimeUIFBDMapUnified::normalizeSelection()
{
  const uint16_t count = _model != nullptr ? _model->blockCount() : 0;
  if (count == 0)
  {
    _selectedIndex = 0;
  }
  else if (_selectedIndex >= count)
  {
    _selectedIndex = static_cast<uint16_t>(count - 1U);
  }
}

bool RuntimeUIFBDMapUnified::updateHorizontalWindow()
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    const bool changed =
        _horizontalMode != HorizontalWindowMode::LeftEdge ||
        _centralStartLevel != 1;
    _horizontalMode = HorizontalWindowMode::LeftEdge;
    _centralStartLevel = 1;
    return changed;
  }

  const uint8_t selectedLevel = _levels[_selectedIndex];
  if (_maxLevel <= 4 || isFullLevel(selectedLevel))
  {
    return false;
  }

  const HorizontalWindowMode previousMode = _horizontalMode;
  const uint8_t previousStart = _centralStartLevel;

  if (selectedLevel <= 3)
  {
    _horizontalMode = HorizontalWindowMode::LeftEdge;
    _centralStartLevel = 1;
  }
  else if (selectedLevel >= _maxLevel)
  {
    _horizontalMode = HorizontalWindowMode::RightEdge;
    _centralStartLevel = static_cast<uint8_t>(_maxLevel - 3U);
  }
  else
  {
    _horizontalMode = HorizontalWindowMode::Middle;
    uint8_t start = selectedLevel > 1
                        ? static_cast<uint8_t>(selectedLevel - 2U)
                        : 1;
    const uint8_t maximumStart = static_cast<uint8_t>(_maxLevel - 3U);
    if (start < 1) start = 1;
    if (start > maximumStart) start = maximumStart;
    _centralStartLevel = start;
  }

  return previousMode != _horizontalMode ||
         previousStart != _centralStartLevel;
}

bool RuntimeUIFBDMapUnified::ensureSelectionVisible()
{
  const int16_t previousViewportY = _viewportY;
  const bool horizontalChanged = updateHorizontalWindow();

  if (_model == nullptr || _model->blockCount() == 0)
  {
    _viewportY = 0;
    return horizontalChanged || previousViewportY != 0;
  }

  const int16_t nodeWorldY = _nodeY[_selectedIndex];
  const int16_t relativeY = static_cast<int16_t>(nodeWorldY - _viewportY);

  if (relativeY < KEEP_MARGIN_Y)
  {
    _viewportY = nodeWorldY > KEEP_MARGIN_Y
                     ? static_cast<int16_t>(nodeWorldY - KEEP_MARGIN_Y)
                     : 0;
  }
  else if (relativeY + NODE_H > MAP_H - KEEP_MARGIN_Y)
  {
    _viewportY = static_cast<int16_t>(
        nodeWorldY + NODE_H - (MAP_H - KEEP_MARGIN_Y));
  }

  if (_viewportY < 0)
  {
    _viewportY = 0;
  }

  const bool changed =
      horizontalChanged || _viewportY != previousViewportY;
  if (changed)
  {
    invalidateMapCache();
  }
  return changed;
}

uint16_t RuntimeUIFBDMapUnified::nearestByY(const uint16_t *indices,
                                            uint8_t count) const
{
  if (indices == nullptr || count == 0)
  {
    return JWPLC_LOGIC_NO_SOURCE;
  }

  uint16_t selected = indices[0];
  uint16_t bestDistance =
      absoluteDistanceUnified(_nodeY[selected], _nodeY[_selectedIndex]);

  for (uint8_t index = 1; index < count; ++index)
  {
    const uint16_t candidate = indices[index];
    const uint16_t distance =
        absoluteDistanceUnified(_nodeY[candidate], _nodeY[_selectedIndex]);
    if (distance < bestDistance)
    {
      selected = candidate;
      bestDistance = distance;
    }
  }
  return selected;
}

bool RuntimeUIFBDMapUnified::selectSource()
{
  uint16_t sources[JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK] = {};
  const uint8_t count = _model->collectSources(
      _selectedIndex,
      sources,
      JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK);
  const uint16_t next = nearestByY(sources, count);
  if (next == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = next;
  return true;
}

bool RuntimeUIFBDMapUnified::selectConsumer()
{
  uint16_t consumers[16] = {};
  const uint8_t count =
      _model->collectConsumers(_selectedIndex, consumers, 16);
  const uint16_t next = nearestByY(consumers, count);
  if (next == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = next;
  return true;
}

bool RuntimeUIFBDMapUnified::selectVertical(bool down)
{
  const uint16_t count = _model->blockCount();
  const int16_t selectedY = _nodeY[_selectedIndex];
  const int16_t selectedX = _nodeX[_selectedIndex];
  uint16_t best = JWPLC_LOGIC_NO_SOURCE;
  uint32_t bestScore = 0xFFFFFFFFUL;

  for (uint16_t candidate = 0; candidate < count; ++candidate)
  {
    if (candidate == _selectedIndex)
    {
      continue;
    }

    const int16_t deltaY =
        static_cast<int16_t>(_nodeY[candidate] - selectedY);
    if ((down && deltaY <= 0) || (!down && deltaY >= 0))
    {
      continue;
    }

    const uint32_t score =
        static_cast<uint32_t>(
            absoluteDistanceUnified(_nodeY[candidate], selectedY)) *
            4UL +
        absoluteDistanceUnified(_nodeX[candidate], selectedX);
    if (score < bestScore)
    {
      bestScore = score;
      best = candidate;
    }
  }

  if (best == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }

  _selectedIndex = best;
  return true;
}

void RuntimeUIFBDMapUnified::handleMapInput()
{
  if (_model == nullptr)
  {
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    changed = selectSource();
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    changed = selectConsumer();
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    changed = selectVertical(false);
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    changed = selectVertical(true);
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    const bool viewportChanged = ensureSelectionVisible();
    _headerDirty = true;
    if (viewportChanged)
    {
      _contentDirty = true;
    }
  }

  if (JWPLC_Buttons.pressed(BTN_OK) && _model->blockCount() > 0)
  {
    JWPLC_Display.notifyActivity();
    transitionTo(View::Detail);
  }
}

void RuntimeUIFBDMapUnified::renderMap(bool force)
{
  if (_model == nullptr)
  {
    return;
  }

  if (force)
  {
    clearContentArea();
    drawMapFull();
    return;
  }

  drawMapLive();
}

void RuntimeUIFBDMapUnified::drawMapFull()
{
  if (_model == nullptr)
  {
    return;
  }

  if (_model->blockCount() == 0)
  {
    updateTextField(JWPLC_Display.tft(),
                    MAP_X + 80,
                    MAP_Y + 58,
                    24,
                    "SIN PROGRAMA V2",
                    COLOR_WARNING,
                    COLOR_PANEL);
    invalidateMapCache();
    return;
  }

  drawWires();
  drawNodes();
  drawEdgeHints();

  const uint16_t count = _model->blockCount();
  for (uint16_t block = 0; block < count; ++block)
  {
    _mapValueCache[block] = _model->blockValue(block);
  }
  _mapValueCacheValid = true;
  _mapSelectionCache = _selectedIndex;
  _mapSelectionCacheValid = true;
}

void RuntimeUIFBDMapUnified::drawMapLive()
{
  const uint16_t count = _model->blockCount();
  if (count == 0)
  {
    return;
  }

  if (!_mapValueCacheValid || !_mapSelectionCacheValid)
  {
    clearContentArea();
    drawMapFull();
    return;
  }

  bool valueChanged[MAX_BLOCKS] = {};
  bool redrawNode[MAX_BLOCKS] = {};
  bool anyChange = false;

  for (uint16_t block = 0; block < count; ++block)
  {
    const bool value = _model->blockValue(block);
    if (_mapValueCache[block] != value)
    {
      valueChanged[block] = true;
      redrawNode[block] = true;
      anyChange = true;
      if (block == _selectedIndex)
      {
        _headerDirty = true;
      }
    }
  }

  if (_mapSelectionCache != _selectedIndex)
  {
    if (_mapSelectionCache < count)
    {
      redrawNode[_mapSelectionCache] = true;
    }
    redrawNode[_selectedIndex] = true;
    anyChange = true;
    _headerDirty = true;
  }

  if (!anyChange)
  {
    return;
  }

  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    const LogicV2BlockRecord *definition = _model->block(consumer);
    if (definition == nullptr)
    {
      continue;
    }

    for (uint8_t input = 0; input < definition->inputCount; ++input)
    {
      const LogicV2InputLink *link = _model->inputLink(consumer, input);
      if (link == nullptr)
      {
        continue;
      }

      const uint16_t source = link->source();
      if (source < count && valueChanged[source])
      {
        drawWire(consumer, input);
        redrawNode[consumer] = true;
      }
    }
  }

  for (uint16_t block = 0; block < count; ++block)
  {
    if (redrawNode[block])
    {
      drawNode(block);
    }
  }

  drawEdgeHints();

  for (uint16_t block = 0; block < count; ++block)
  {
    _mapValueCache[block] = _model->blockValue(block);
  }
  _mapValueCacheValid = true;
  _mapSelectionCache = _selectedIndex;
  _mapSelectionCacheValid = true;
}

bool RuntimeUIFBDMapUnified::isFullLevel(uint8_t level) const
{
  if (_maxLevel <= 4)
  {
    return level <= _maxLevel;
  }

  switch (_horizontalMode)
  {
  case HorizontalWindowMode::LeftEdge:
    return level <= 3;
  case HorizontalWindowMode::Middle:
    return level >= _centralStartLevel &&
           level <= static_cast<uint8_t>(_centralStartLevel + 2U);
  case HorizontalWindowMode::RightEdge:
  default:
    return level >= static_cast<uint8_t>(_maxLevel - 3U) &&
           level <= _maxLevel;
  }
}

bool RuntimeUIFBDMapUnified::isPreviewLevel(uint8_t level,
                                            bool &leftSide) const
{
  leftSide = false;
  if (_maxLevel <= 4)
  {
    return false;
  }

  switch (_horizontalMode)
  {
  case HorizontalWindowMode::LeftEdge:
    return level == 4;
  case HorizontalWindowMode::Middle:
    if (level == static_cast<uint8_t>(_centralStartLevel - 1U))
    {
      leftSide = true;
      return true;
    }
    if (level == static_cast<uint8_t>(_centralStartLevel + 3U))
    {
      return true;
    }
    return false;
  case HorizontalWindowMode::RightEdge:
  default:
    leftSide = true;
    return level == static_cast<uint8_t>(_maxLevel - 4U);
  }
}

int8_t RuntimeUIFBDMapUnified::slotForLevel(uint8_t level) const
{
  if (_maxLevel <= 4)
  {
    return level <= _maxLevel
               ? static_cast<int8_t>(level)
               : static_cast<int8_t>(SLOT_COUNT);
  }

  switch (_horizontalMode)
  {
  case HorizontalWindowMode::LeftEdge:
    return level <= 4
               ? static_cast<int8_t>(level)
               : static_cast<int8_t>(SLOT_COUNT);

  case HorizontalWindowMode::Middle:
    if (level < static_cast<uint8_t>(_centralStartLevel - 1U)) return -1;
    if (level > static_cast<uint8_t>(_centralStartLevel + 3U))
      return static_cast<int8_t>(SLOT_COUNT);
    return static_cast<int8_t>(
        level - static_cast<uint8_t>(_centralStartLevel - 1U));

  case HorizontalWindowMode::RightEdge:
  default:
  {
    const uint8_t firstLevel = static_cast<uint8_t>(_maxLevel - 4U);
    if (level < firstLevel) return -1;
    if (level > _maxLevel) return static_cast<int8_t>(SLOT_COUNT);
    return static_cast<int8_t>(level - firstLevel);
  }
  }
}

int16_t RuntimeUIFBDMapUnified::screenX(uint16_t blockIndex) const
{
  const uint8_t level = _levels[blockIndex];
  const int8_t slot = slotForLevel(level);
  if (slot < 0) return static_cast<int16_t>(MAP_X - NODE_W);
  if (slot >= static_cast<int8_t>(SLOT_COUNT))
    return static_cast<int16_t>(MAP_X + MAP_W);

  bool leftSide = false;
  const bool preview = isPreviewLevel(level, leftSide);
  const int16_t baseX = static_cast<int16_t>(
      SLOT_X0 + static_cast<int16_t>(slot) * SLOT_STEP);
  return preview
             ? static_cast<int16_t>(baseX + (NODE_W - EDGE_HINT_W) / 2)
             : baseX;
}

int16_t RuntimeUIFBDMapUnified::screenY(uint16_t blockIndex) const
{
  return static_cast<int16_t>(
      MAP_Y + _nodeY[blockIndex] - _viewportY);
}

bool RuntimeUIFBDMapUnified::nodeFullyVisible(int16_t x, int16_t y) const
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  return x >= MAP_X &&
         y >= MAP_Y &&
         x + NODE_W - 1 <= mapRight &&
         y + NODE_H - 1 <= mapBottom;
}

int16_t RuntimeUIFBDMapUnified::renderedNodeWidth(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_W
             : NODE_W;
}

int16_t RuntimeUIFBDMapUnified::renderedNodeHeight(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_H
             : NODE_H;
}

int16_t RuntimeUIFBDMapUnified::renderedNodeYOffset(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_Y_OFFSET
             : 0;
}

int16_t RuntimeUIFBDMapUnified::inputPortY(
    const LogicV2BlockRecord &block,
    uint8_t inputIndex) const
{
  if (block.type == LogicV2BlockType::SetReset && block.inputCount >= 2)
  {
    return inputIndex == 0 ? 9 : 21;
  }
  if (block.inputCount <= 1)
  {
    return NODE_H / 2;
  }

  const uint16_t top = 4;
  const uint16_t span = static_cast<uint16_t>(NODE_H - 8);
  return static_cast<int16_t>(
      top + (static_cast<uint16_t>(inputIndex) * span) /
                (block.inputCount - 1U));
}

RuntimeUIFBDMapUnified::GridRange
RuntimeUIFBDMapUnified::visibleGridRange() const
{
  GridRange range = {0, 0, 0, 0, false};
  if (_model == nullptr)
  {
    return range;
  }

  const uint16_t count = _model->blockCount();
  for (uint16_t index = 0; index < count; ++index)
  {
    if (!isFullLevel(_levels[index]) ||
        !nodeFullyVisible(screenX(index), screenY(index)))
    {
      continue;
    }

    if (!range.valid)
    {
      range.minLevel = _levels[index];
      range.maxLevel = _levels[index];
      range.minLane = _lanes[index];
      range.maxLane = _lanes[index];
      range.valid = true;
    }
    else
    {
      range.minLevel = minimum8Unified(range.minLevel, _levels[index]);
      range.maxLevel = maximum8Unified(range.maxLevel, _levels[index]);
      range.minLane = minimum8Unified(range.minLane, _lanes[index]);
      range.maxLane = maximum8Unified(range.maxLane, _lanes[index]);
    }
  }
  return range;
}

bool RuntimeUIFBDMapUnified::shouldDrawEdgeHint(
    uint16_t blockIndex,
    const GridRange &range,
    bool &leftSide) const
{
  (void)range;
  if (_model == nullptr || blockIndex >= _model->blockCount())
  {
    return false;
  }

  if (!isPreviewLevel(_levels[blockIndex], leftSide))
  {
    return false;
  }

  const int16_t y =
      static_cast<int16_t>(screenY(blockIndex) + EDGE_HINT_Y_OFFSET);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  return y >= MAP_Y && y + EDGE_HINT_H - 1 <= mapBottom;
}

void RuntimeUIFBDMapUnified::drawClippedHorizontal(
    int16_t x0, int16_t x1, int16_t y, uint16_t color)
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  if (y < MAP_Y || y > mapBottom) return;

  const int16_t left =
      maximum16Unified(minimum16Unified(x0, x1), MAP_X);
  const int16_t right =
      minimum16Unified(maximum16Unified(x0, x1), mapRight);
  if (left > right) return;

  JWPLC_Display.tft().drawFastHLine(
      left, y, static_cast<int16_t>(right - left + 1), color);
}

void RuntimeUIFBDMapUnified::drawClippedVertical(
    int16_t x, int16_t y0, int16_t y1, uint16_t color)
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  if (x < MAP_X || x > mapRight) return;

  const int16_t top =
      maximum16Unified(minimum16Unified(y0, y1), MAP_Y);
  const int16_t bottom =
      minimum16Unified(maximum16Unified(y0, y1), mapBottom);
  if (top > bottom) return;

  JWPLC_Display.tft().drawFastVLine(
      x, top, static_cast<int16_t>(bottom - top + 1), color);
}

void RuntimeUIFBDMapUnified::drawOrthogonalWireClipped(
    int16_t x0,
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

void RuntimeUIFBDMapUnified::drawWires()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    const LogicV2BlockRecord *definition = _model->block(consumer);
    if (definition == nullptr) continue;
    for (uint8_t input = 0; input < definition->inputCount; ++input)
    {
      drawWire(consumer, input);
    }
  }
}

void RuntimeUIFBDMapUnified::drawWire(uint16_t consumerIndex,
                                      uint8_t inputIndex)
{
  const LogicV2BlockRecord *consumer = _model->block(consumerIndex);
  const LogicV2InputLink *input =
      _model->inputLink(consumerIndex, inputIndex);
  if (consumer == nullptr || input == nullptr) return;

  bool consumerLeftPreview = false;
  const bool consumerPreview =
      isPreviewLevel(_levels[consumerIndex], consumerLeftPreview);
  if (!isFullLevel(_levels[consumerIndex]) && !consumerPreview) return;

  const int16_t consumerNodeX = screenX(consumerIndex);
  const int16_t consumerNodeY = static_cast<int16_t>(
      screenY(consumerIndex) + renderedNodeYOffset(consumerIndex));
  const int16_t destinationX = consumerNodeX;
  const int16_t destinationY =
      consumerPreview
          ? static_cast<int16_t>(
                consumerNodeY + renderedNodeHeight(consumerIndex) / 2)
          : static_cast<int16_t>(
                consumerNodeY + inputPortY(*consumer, inputIndex));
  const uint16_t sourceIndex = input->source();

  if (sourceIndex == JWPLC_LOGIC_V2_SOURCE_OPEN ||
      sourceIndex == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE ||
      sourceIndex == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    const uint16_t color =
        _model->inputValue(consumerIndex, inputIndex)
            ? COLOR_OK
            : COLOR_MUTED;
    drawClippedHorizontal(destinationX - 7,
                          destinationX,
                          destinationY,
                          color);
    return;
  }

  if (sourceIndex >= _model->blockCount()) return;

  const int16_t sourceNodeX = screenX(sourceIndex);
  const int16_t sourceNodeY = static_cast<int16_t>(
      screenY(sourceIndex) + renderedNodeYOffset(sourceIndex));
  const int16_t sourceX = static_cast<int16_t>(
      sourceNodeX + renderedNodeWidth(sourceIndex));
  const int16_t sourceY = static_cast<int16_t>(
      sourceNodeY + renderedNodeHeight(sourceIndex) / 2);

  int16_t routeX = static_cast<int16_t>((sourceX + destinationX) / 2);
  const int16_t gap = static_cast<int16_t>(destinationX - sourceX);
  if (gap >= 8)
  {
    const uint8_t divisor = static_cast<uint8_t>(consumer->inputCount + 1U);
    routeX = static_cast<int16_t>(
        sourceX +
        (static_cast<int32_t>(inputIndex + 1U) * gap) / divisor);
    routeX = clamp16Unified(
        routeX,
        static_cast<int16_t>(sourceX + 2),
        static_cast<int16_t>(destinationX - 2));
  }

  const uint16_t color =
      _model->blockValue(sourceIndex) ? COLOR_OK : COLOR_MUTED;
  drawOrthogonalWireClipped(
      sourceX, sourceY, destinationX, destinationY, routeX, color);
}

void RuntimeUIFBDMapUnified::drawNodes()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    drawNode(blockIndex);
  }
}

void RuntimeUIFBDMapUnified::drawNode(uint16_t blockIndex)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr || !isFullLevel(_levels[blockIndex])) return;

  const int16_t x = screenX(blockIndex);
  const int16_t y = screenY(blockIndex);
  if (!nodeFullyVisible(x, y)) return;

  const bool active = _model->blockValue(blockIndex);
  const bool selected = blockIndex == _selectedIndex;
  const uint16_t border =
      selected ? COLOR_WARNING : (active ? COLOR_OK : COLOR_BORDER);
  drawFullNode(blockIndex, x, y, border, active, selected);
}

void RuntimeUIFBDMapUnified::drawFullNode(
    uint16_t blockIndex,
    int16_t x,
    int16_t y,
    uint16_t border,
    bool active,
    bool selected)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr) return;

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
    tft.drawFastVLine(
        x + NODE_GUTTER_W, y + 2, NODE_H - 4, COLOR_BORDER);
  }

  char blockId[8];
  char line2[10];
  formatBlockId(blockId, sizeof(blockId), blockIndex);
  formatMapLine2(line2, sizeof(line2), blockIndex);

  const int16_t contentX = static_cast<int16_t>(x + gutter);
  const int16_t contentW = static_cast<int16_t>(NODE_W - gutter);
  const int16_t idX = static_cast<int16_t>(
      contentX + maximum16Unified(
                     1,
                     static_cast<int16_t>(
                         (contentW - std::strlen(blockId) * 6) / 2)));
  const int16_t line2X = static_cast<int16_t>(
      contentX + maximum16Unified(
                     1,
                     static_cast<int16_t>(
                         (contentW - std::strlen(line2) * 6) / 2)));

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

void RuntimeUIFBDMapUnified::drawEdgeHints()
{
  const GridRange range = visibleGridRange();
  const uint16_t count = _model->blockCount();
  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    bool leftSide = false;
    if (!shouldDrawEdgeHint(blockIndex, range, leftSide)) continue;

    const bool active = _model->blockValue(blockIndex);
    const bool selected = blockIndex == _selectedIndex;
    const uint16_t border =
        selected ? COLOR_WARNING : (active ? COLOR_OK : COLOR_BORDER);
    drawEdgeHint(blockIndex,
                 leftSide,
                 screenY(blockIndex),
                 border,
                 active);
  }
}

void RuntimeUIFBDMapUnified::drawEdgeHint(
    uint16_t blockIndex,
    bool leftSide,
    int16_t nodeScreenY,
    uint16_t border,
    bool active)
{
  const int16_t hintX = screenX(blockIndex);
  const int16_t hintY =
      static_cast<int16_t>(nodeScreenY + EDGE_HINT_Y_OFFSET);

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(hintX, hintY, EDGE_HINT_W, EDGE_HINT_H, COLOR_BACKGROUND);
  tft.drawRect(hintX, hintY, EDGE_HINT_W, EDGE_HINT_H, border);

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
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(hintX + 3, hintY + 3);
  tft.print(firstLine);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(hintX + 9, hintY + 13);
  tft.print(line2);
}

void RuntimeUIFBDMapUnified::drawNodePorts(
    uint16_t blockIndex, int16_t x, int16_t y)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr) return;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount;
       ++inputIndex)
  {
    const LogicV2InputLink *input =
        _model->inputLink(blockIndex, inputIndex);
    if (input == nullptr) continue;

    const int16_t portY =
        static_cast<int16_t>(y + inputPortY(*definition, inputIndex));
    const bool inputActive =
        _model->inputValue(blockIndex, inputIndex);
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

    const char *role = _model->inputRole(definition->type, inputIndex);
    if (role != nullptr && role[0] != '\0')
    {
      tft.setTextWrap(false);
      tft.setTextSize(1);
      tft.setTextColor(pinColor, COLOR_BACKGROUND);
      tft.setCursor(x + 4, portY - 3);
      tft.print(role);
    }
  }

  tft.fillCircle(x + NODE_W,
                 static_cast<int16_t>(y + NODE_H / 2),
                 1,
                 _model->blockValue(blockIndex) ? COLOR_OK : COLOR_MUTED);
}

void RuntimeUIFBDMapUnified::formatBlockId(
    char *destination,
    size_t capacity,
    uint16_t blockIndex) const
{
  std::snprintf(destination,
                capacity,
                "B%02u",
                static_cast<unsigned>(blockIndex));
}

void RuntimeUIFBDMapUnified::formatMapLine2(
    char *destination,
    size_t capacity,
    uint16_t blockIndex) const
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    std::snprintf(destination, capacity, "?");
    return;
  }

  if (definition->type == LogicV2BlockType::DigitalInput)
  {
    std::snprintf(destination,
                  capacity,
                  "I0.%u",
                  static_cast<unsigned>(definition->resource));
  }
  else if (definition->type == LogicV2BlockType::DigitalOutput)
  {
    std::snprintf(destination,
                  capacity,
                  "Q0.%u",
                  static_cast<unsigned>(definition->resource));
  }
  else
  {
    std::snprintf(
        destination, capacity, "%s", _model->typeShort(definition->type));
  }
}
