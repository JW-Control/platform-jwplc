#include "RuntimeUIFBDMapV4.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  uint8_t minimum8(uint8_t a, uint8_t b) { return a < b ? a : b; }
  uint8_t maximum8(uint8_t a, uint8_t b) { return a > b ? a : b; }
}

bool RuntimeUIFBDMapV4::valuesChanged()
{
  const uint16_t count = _model->blockCount();
  bool changed = !_valueCacheValid;

  for (uint16_t index = 0; index < count; ++index)
  {
    const bool value = _model->blockValue(index);
    if (!_valueCacheValid || _valueCache[index] != value)
    {
      changed = true;
      _valueCache[index] = value;
    }
  }

  _valueCacheValid = true;
  return changed;
}

void RuntimeUIFBDMapV4::cacheValues()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t index = 0; index < count; ++index)
  {
    _valueCache[index] = _model->blockValue(index);
  }
  _valueCacheValid = true;
}

void RuntimeUIFBDMapV4::updateHeaderStateIfNeeded(bool force)
{
  if (_model == nullptr)
  {
    return;
  }

  const LogicV2EngineState state = _model->state();
  if (!force && _headerStateValid && state == _lastHeaderState)
  {
    return;
  }

  updateHeaderState(JWPLC_Display.tft(), stateText(), stateColor());
  _lastHeaderState = state;
  _headerStateValid = true;
}

bool RuntimeUIFBDMapV4::isFullLevel(uint8_t level) const
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

bool RuntimeUIFBDMapV4::isPreviewLevel(uint8_t level,
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
    leftSide = false;
    return level == 4;

  case HorizontalWindowMode::Middle:
    if (level == static_cast<uint8_t>(_centralStartLevel - 1U))
    {
      leftSide = true;
      return true;
    }
    if (level == static_cast<uint8_t>(_centralStartLevel + 3U))
    {
      leftSide = false;
      return true;
    }
    return false;

  case HorizontalWindowMode::RightEdge:
  default:
    leftSide = true;
    return level == static_cast<uint8_t>(_maxLevel - 4U);
  }
}

int8_t RuntimeUIFBDMapV4::slotForLevel(uint8_t level) const
{
  if (_maxLevel <= 4)
  {
    if (level <= _maxLevel)
    {
      return static_cast<int8_t>(level);
    }
    return static_cast<int8_t>(SLOT_COUNT);
  }

  switch (_horizontalMode)
  {
  case HorizontalWindowMode::LeftEdge:
    if (level <= 4)
    {
      return static_cast<int8_t>(level);
    }
    return static_cast<int8_t>(SLOT_COUNT);

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

bool RuntimeUIFBDMapV4::nodeFullyVisible(int16_t x, int16_t y) const
{
  const int16_t mapRight = static_cast<int16_t>(MAP_X + MAP_W - 1);
  const int16_t mapBottom = static_cast<int16_t>(MAP_Y + MAP_H - 1);
  return x >= MAP_X &&
         y >= MAP_Y &&
         x + NODE_W - 1 <= mapRight &&
         y + NODE_H - 1 <= mapBottom;
}

RuntimeUIFBDMapV4::GridRange RuntimeUIFBDMapV4::visibleGridRange() const
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
      range.minLevel = minimum8(range.minLevel, _levels[index]);
      range.maxLevel = maximum8(range.maxLevel, _levels[index]);
      range.minLane = minimum8(range.minLane, _lanes[index]);
      range.maxLane = maximum8(range.maxLane, _lanes[index]);
    }
  }

  return range;
}

bool RuntimeUIFBDMapV4::shouldDrawEdgeHint(uint16_t blockIndex,
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
  return y >= MAP_Y &&
         y + EDGE_HINT_H - 1 <= mapBottom;
}

int16_t RuntimeUIFBDMapV4::screenX(uint16_t blockIndex) const
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

int16_t RuntimeUIFBDMapV4::screenY(uint16_t blockIndex) const
{
  return static_cast<int16_t>(
      MAP_Y + _nodeY[blockIndex] - _viewportY);
}

int16_t RuntimeUIFBDMapV4::renderedNodeWidth(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_W
             : NODE_W;
}

int16_t RuntimeUIFBDMapV4::renderedNodeHeight(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_H
             : NODE_H;
}

int16_t RuntimeUIFBDMapV4::renderedNodeYOffset(uint16_t blockIndex) const
{
  bool leftSide = false;
  return isPreviewLevel(_levels[blockIndex], leftSide)
             ? EDGE_HINT_Y_OFFSET
             : 0;
}

int16_t RuntimeUIFBDMapV4::inputPortY(
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

void RuntimeUIFBDMapV4::formatBlockId(char *destination,
                                      size_t capacity,
                                      uint16_t blockIndex) const
{
  std::snprintf(destination,
                capacity,
                "B%02u",
                static_cast<unsigned>(blockIndex));
}

void RuntimeUIFBDMapV4::formatMapLine2(char *destination,
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
    std::snprintf(destination,
                  capacity,
                  "%s",
                  _model->typeShort(definition->type));
  }
}

void RuntimeUIFBDMapV4::formatSourceCompact(char *destination,
                                            size_t capacity,
                                            uint16_t blockIndex,
                                            uint8_t inputIndex) const
{
  const LogicV2InputLink *input =
      _model->inputLink(blockIndex, inputIndex);
  if (input == nullptr)
  {
    std::snprintf(destination, capacity, "-");
    return;
  }

  const uint16_t source = input->source();
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(destination, capacity, "X");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(destination, capacity, "HI");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(destination, capacity, "LO");
  }
  else
  {
    std::snprintf(destination,
                  capacity,
                  "B%02u",
                  static_cast<unsigned>(source));
  }
}

const char *RuntimeUIFBDMapV4::detailSymbol(LogicV2BlockType type) const
{
  switch (type)
  {
  case LogicV2BlockType::And:
    return "&";
  case LogicV2BlockType::Or:
    return ">=1";
  case LogicV2BlockType::Nand:
    return "&!";
  case LogicV2BlockType::Nor:
    return ">=1!";
  case LogicV2BlockType::Xor:
    return "=1";
  case LogicV2BlockType::Not:
    return "1";
  case LogicV2BlockType::SetReset:
    return "SR";
  case LogicV2BlockType::Ton:
    return "TON";
  case LogicV2BlockType::DigitalInput:
    return "I";
  case LogicV2BlockType::DigitalOutput:
    return "Q";
  case LogicV2BlockType::ConstantTrue:
    return "1";
  case LogicV2BlockType::ConstantFalse:
  default:
    return "0";
  }
}

const char *RuntimeUIFBDMapV4::stateText() const
{
  if (_model == nullptr)
  {
    return "SIN MOTOR";
  }

  switch (_model->state())
  {
  case LogicV2EngineState::Ready:
    return "READY";
  case LogicV2EngineState::Running:
    return "RUN";
  case LogicV2EngineState::Stopped:
    return "STOP";
  case LogicV2EngineState::Fault:
    return "FAULT";
  case LogicV2EngineState::Empty:
  default:
    return "EMPTY";
  }
}

uint16_t RuntimeUIFBDMapV4::stateColor() const
{
  if (_model == nullptr)
  {
    return COLOR_MUTED;
  }

  switch (_model->state())
  {
  case LogicV2EngineState::Running:
    return COLOR_OK;
  case LogicV2EngineState::Ready:
    return COLOR_ACCENT;
  case LogicV2EngineState::Stopped:
    return COLOR_WARNING;
  case LogicV2EngineState::Fault:
    return COLOR_ERROR;
  case LogicV2EngineState::Empty:
  default:
    return COLOR_MUTED;
  }
}
