#include "RuntimeUIFBDMap.h"

#include <cstring>

namespace
{
uint8_t minimum8(uint8_t a, uint8_t b) { return a < b ? a : b; }
uint8_t maximum8(uint8_t a, uint8_t b) { return a > b ? a : b; }
}

void RuntimeUIFBDMap::invalidateLayout()
{
  _layoutValid = false;
  _valueCacheValid = false;
  _layoutBlockCount = 0;
  _layoutLinkCount = 0;
  _maxLevel = 0;
  std::memset(_levels, 0, sizeof(_levels));
  std::memset(_lanes, 0, sizeof(_lanes));
  std::memset(_nodeX, 0, sizeof(_nodeX));
  std::memset(_nodeY, 0, sizeof(_nodeY));
}

void RuntimeUIFBDMap::buildLayout()
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
  _valueCacheValid = false;
}

void RuntimeUIFBDMap::normalizeSelection()
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

bool RuntimeUIFBDMap::updateHorizontalWindow()
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
    if (start > maximumStart)
    {
      start = maximumStart;
    }
    _centralStartLevel = start;
  }

  return previousMode != _horizontalMode ||
         previousStart != _centralStartLevel;
}

bool RuntimeUIFBDMap::ensureSelectionVisible()
{
  const int16_t previousViewportY = _viewportY;
  const bool horizontalChanged = updateHorizontalWindow();

  if (_model == nullptr || _model->blockCount() == 0)
  {
    _viewportY = 0;
    const bool changed = horizontalChanged || previousViewportY != 0;
    if (changed)
    {
      _valueCacheValid = false;
    }
    return changed;
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

  const bool changed = horizontalChanged ||
                       _viewportY != previousViewportY;
  if (changed)
  {
    _valueCacheValid = false;
  }
  return changed;
}

bool RuntimeUIFBDMap::selectSource()
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

bool RuntimeUIFBDMap::selectConsumer()
{
  uint16_t consumers[16] = {};
  const uint8_t count = _model->collectConsumers(
      _selectedIndex,
      consumers,
      16);
  const uint16_t next = nearestByY(consumers, count);
  if (next == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = next;
  return true;
}

bool RuntimeUIFBDMap::selectVertical(bool down)
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    return false;
  }

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
    const int16_t deltaY = static_cast<int16_t>(_nodeY[candidate] - selectedY);
    if ((down && deltaY <= 0) || (!down && deltaY >= 0))
    {
      continue;
    }
    const uint32_t score =
        static_cast<uint32_t>(
            _nodeY[candidate] > selectedY
                ? _nodeY[candidate] - selectedY
                : selectedY - _nodeY[candidate]) * 4UL +
        static_cast<uint32_t>(
            _nodeX[candidate] > selectedX
                ? _nodeX[candidate] - selectedX
                : selectedX - _nodeX[candidate]);
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

bool RuntimeUIFBDMap::isFullLevel(uint8_t level) const
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

bool RuntimeUIFBDMap::isPreviewLevel(uint8_t level,
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

int8_t RuntimeUIFBDMap::slotForLevel(uint8_t level) const
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
    if (level < static_cast<uint8_t>(_centralStartLevel - 1U))
    {
      return -1;
    }
    if (level > static_cast<uint8_t>(_centralStartLevel + 3U))
    {
      return static_cast<int8_t>(SLOT_COUNT);
    }
    return static_cast<int8_t>(
        level - static_cast<uint8_t>(_centralStartLevel - 1U));
  case HorizontalWindowMode::RightEdge:
  default:
  {
    const uint8_t firstLevel = static_cast<uint8_t>(_maxLevel - 4U);
    if (level < firstLevel)
    {
      return -1;
    }
    if (level > _maxLevel)
    {
      return static_cast<int8_t>(SLOT_COUNT);
    }
    return static_cast<int8_t>(level - firstLevel);
  }
  }
}

bool RuntimeUIFBDMap::nodeFullyVisible(int16_t x, int16_t y) const
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  return x >= MAP_X && y >= MAP_Y &&
         x + NODE_W - 1 <= mapRight &&
         y + NODE_H - 1 <= mapBottom;
}

RuntimeUIFBDMap::GridRange RuntimeUIFBDMap::visibleGridRange() const
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
      range.minLevel = range.maxLevel = _levels[index];
      range.minLane = range.maxLane = _lanes[index];
      range.valid = true;
    }
    else
    {
      range.minLevel = minimum8(range.minLevel, _levels[index]);
      range.maxLevel = maximum8(range.maxLevel, _levels[index]);
      range.minLane = minimum8(range.minLane, _lanes[index]);
      range.maxLane = maximum8(range.maxLane, _lanes[index]);
    }
  }
  return range;
}

bool RuntimeUIFBDMap::shouldDrawEdgeHint(uint16_t blockIndex,
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
  const int16_t y = static_cast<int16_t>(
      screenY(blockIndex) + EDGE_HINT_Y_OFFSET);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  return y >= MAP_Y && y + EDGE_HINT_H - 1 <= mapBottom;
}

int16_t RuntimeUIFBDMap::screenX(uint16_t blockIndex) const
{
  const uint8_t level = _levels[blockIndex];
  const int8_t slot = slotForLevel(level);
  if (slot < 0)
  {
    return static_cast<int16_t>(MAP_X - NODE_W);
  }
  if (slot >= static_cast<int8_t>(SLOT_COUNT))
  {
    return static_cast<int16_t>(MAP_X + MAP_W);
  }

  bool leftSide = false;
  const bool preview = isPreviewLevel(level, leftSide);
  const int16_t baseX = static_cast<int16_t>(
      SLOT_X0 + static_cast<int16_t>(slot) * SLOT_STEP);
  return preview
             ? static_cast<int16_t>(baseX + (NODE_W - EDGE_HINT_W) / 2)
             : baseX;
}

int16_t RuntimeUIFBDMap::screenY(uint16_t blockIndex) const
{
  return static_cast<int16_t>(MAP_Y + _nodeY[blockIndex] - _viewportY);
}

int16_t RuntimeUIFBDMap::renderedNodeWidth(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_W
             : NODE_W;
}

int16_t RuntimeUIFBDMap::renderedNodeHeight(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_H
             : NODE_H;
}

int16_t RuntimeUIFBDMap::renderedNodeYOffset(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_Y_OFFSET
             : 0;
}

int16_t RuntimeUIFBDMap::inputPortY(const LogicV2BlockRecord &block,
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
