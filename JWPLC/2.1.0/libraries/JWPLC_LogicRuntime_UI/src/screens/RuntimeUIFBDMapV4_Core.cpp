#include "RuntimeUIFBDMapV4.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  uint16_t absoluteDistance(int16_t a, int16_t b)
  {
    return static_cast<uint16_t>(a > b ? a - b : b - a);
  }
}

RuntimeUIFBDMapV4::RuntimeUIFBDMapV4()
    : _model(nullptr),
      _editSession(),
      _mode(Mode::Map),
      _horizontalMode(HorizontalWindowMode::LeftEdge),
      _fullRedraw(true),
      _layoutValid(false),
      _valueCacheValid(false),
      _headerStateValid(false),
      _lastHeaderState(LogicV2EngineState::Empty),
      _selectedIndex(0),
      _detailInputIndex(0),
      _layoutBlockCount(0),
      _layoutLinkCount(0),
      _centralStartLevel(1),
      _maxLevel(0),
      _viewportY(0),
      _lastValueRefreshMs(0),
      _lastDetailRefreshMs(0),
      _inputReleaseGate(false),
      _restoreIdleReturnPending(false),
      _applyRequested(false),
      _applyCompleted(false),
      _applySuccess(false),
      _awaitingApply(false),
      _editFocus(EditFocus::Source),
      _editFeedback(EditFeedback::None),
      _editSourceCandidate(0),
      _editInverted(false),
      _levels{},
      _lanes{},
      _nodeX{},
      _nodeY{},
      _valueCache{}
{
}

void RuntimeUIFBDMapV4::attach(RuntimeUIV2ReadModel &model)
{
  _model = &model;
  LogicV2EnginePrototype *engine = model.mutableEngine();
  if (engine != nullptr)
  {
    _editSession.attach(*engine);
  }

  _mode = Mode::Map;
  _selectedIndex = 0;
  _detailInputIndex = 0;
  _horizontalMode = HorizontalWindowMode::LeftEdge;
  _centralStartLevel = 1;
  _maxLevel = 0;
  _viewportY = 0;
  _headerStateValid = false;
  _inputReleaseGate = false;
  _restoreIdleReturnPending = false;
  _applyRequested = false;
  _applyCompleted = false;
  _applySuccess = false;
  _awaitingApply = false;
  _editFocus = EditFocus::Source;
  _editFeedback = EditFeedback::None;
  invalidateLayout();
  forceRedraw();
}

void RuntimeUIFBDMapV4::detach()
{
  if (_mode == Mode::EditInput)
  {
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  }

  _editSession.detach();
  _model = nullptr;
  _mode = Mode::Map;
  _selectedIndex = 0;
  _detailInputIndex = 0;
  _horizontalMode = HorizontalWindowMode::LeftEdge;
  _centralStartLevel = 1;
  _maxLevel = 0;
  _viewportY = 0;
  _headerStateValid = false;
  _inputReleaseGate = false;
  _restoreIdleReturnPending = false;
  _applyRequested = false;
  _applyCompleted = false;
  _awaitingApply = false;
  invalidateLayout();
}

void RuntimeUIFBDMapV4::enter()
{
  _mode = Mode::Map;
  _applyRequested = false;
  _applyCompleted = false;
  _awaitingApply = false;
  _editFeedback = EditFeedback::None;
  _lastValueRefreshMs = millis();
  _lastDetailRefreshMs = _lastValueRefreshMs;
  normalizeSelection();
  buildLayout();
  ensureSelectionVisible();
  drawMapStatic();
  drawMapFull();
  gateInputUntilRelease(false);
  _fullRedraw = false;
}

void RuntimeUIFBDMapV4::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  if (_applyCompleted)
  {
    const bool success = _applySuccess;
    _applyCompleted = false;
    _awaitingApply = false;

    if (success)
    {
      _editSession.cancel();
      _editFeedback = EditFeedback::None;
      _mode = Mode::Detail;
      invalidateLayout();
      buildLayout();
      normalizeSelection();
      ensureSelectionVisible();
      _lastDetailRefreshMs = millis();
      gateInputUntilRelease(true);
    }
    else
    {
      _editFeedback = EditFeedback::ApplyFailed;
      gateInputUntilRelease(false);
    }

    _fullRedraw = true;
  }

  if (_layoutValid &&
      (_layoutBlockCount != _model->blockCount() ||
       _layoutLinkCount != _model->linkCount()))
  {
    invalidateLayout();
  }

  if (!_layoutValid)
  {
    buildLayout();
    normalizeSelection();
    ensureSelectionVisible();
    _fullRedraw = true;
  }

  if (_fullRedraw)
  {
    if (_mode == Mode::Map)
    {
      drawMapStatic();
      drawMapFull();
    }
    else if (_mode == Mode::Detail)
    {
      drawDetailStatic();
      drawDetail(true);
    }
    else
    {
      drawInputEditStatic();
      drawInputEdit();
    }
    _fullRedraw = false;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  if (_mode == Mode::Map)
  {
    handleMapInput();
  }
  else if (_mode == Mode::Detail)
  {
    handleDetailInput();
  }
  else
  {
    handleEditInput();
  }

  const uint32_t nowMs = millis();
  if (_mode == Mode::Map)
  {
    if (static_cast<uint32_t>(nowMs - _lastValueRefreshMs) < VALUE_REFRESH_MS)
    {
      return;
    }
    _lastValueRefreshMs = nowMs;

    if (valuesChanged())
    {
      drawMapLive();
    }
  }
  else if (_mode == Mode::Detail)
  {
    if (static_cast<uint32_t>(nowMs - _lastDetailRefreshMs) < DETAIL_REFRESH_MS)
    {
      return;
    }
    _lastDetailRefreshMs = nowMs;

    if (valuesChanged())
    {
      drawDetail(false);
    }
  }
}

void RuntimeUIFBDMapV4::exit()
{
  _valueCacheValid = false;
  _headerStateValid = false;
  _applyRequested = false;
  _applyCompleted = false;
  _awaitingApply = false;

  if (_editSession.active())
  {
    _editSession.cancel();
  }

  if (_mode == Mode::EditInput || _restoreIdleReturnPending)
  {
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  }

  _inputReleaseGate = false;
  _restoreIdleReturnPending = false;
}

void RuntimeUIFBDMapV4::forceRedraw()
{
  _fullRedraw = true;
  _valueCacheValid = false;
  _headerStateValid = false;
}

void RuntimeUIFBDMapV4::processPendingEdit()
{
  if (!_applyRequested)
  {
    return;
  }

  _applyRequested = false;
  const bool success = _editSession.apply(true);
  _applySuccess = success;
  _applyCompleted = true;
}

void RuntimeUIFBDMapV4::invalidateLayout()
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

void RuntimeUIFBDMapV4::buildLayout()
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

void RuntimeUIFBDMapV4::normalizeSelection()
{
  const uint16_t count = _model ? _model->blockCount() : 0;
  if (count == 0)
  {
    _selectedIndex = 0;
  }
  else if (_selectedIndex >= count)
  {
    _selectedIndex = static_cast<uint16_t>(count - 1U);
  }
}

bool RuntimeUIFBDMapV4::updateHorizontalWindow()
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
    if (start < 1)
    {
      start = 1;
    }
    if (start > maximumStart)
    {
      start = maximumStart;
    }
    _centralStartLevel = start;
  }

  return previousMode != _horizontalMode ||
         previousStart != _centralStartLevel;
}

bool RuntimeUIFBDMapV4::ensureSelectionVisible()
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

bool RuntimeUIFBDMapV4::anyButtonHeld() const
{
  for (uint8_t button = 0; button < BTN_COUNT; ++button)
  {
    if (JWPLC_Buttons.isDown(button))
    {
      return true;
    }
  }
  return false;
}

void RuntimeUIFBDMapV4::gateInputUntilRelease(bool restoreIdleReturn)
{
  _inputReleaseGate = true;
  if (restoreIdleReturn)
  {
    _restoreIdleReturnPending = true;
  }
  JWPLC_Buttons.clearPendingInput();
}

bool RuntimeUIFBDMapV4::consumeInputReleaseGate()
{
  if (!_inputReleaseGate)
  {
    return false;
  }

  JWPLC_Buttons.clearPendingInput();
  if (anyButtonHeld())
  {
    return true;
  }

  _inputReleaseGate = false;
  JWPLC_Buttons.clearPendingInput();

  if (_restoreIdleReturnPending)
  {
    _restoreIdleReturnPending = false;
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
  }

  // Se consume un frame adicional para evitar que un RELEASE se interprete
  // como una acción de la pantalla recién restaurada.
  return true;
}

uint16_t RuntimeUIFBDMapV4::nearestByY(const uint16_t *indices,
                                       uint8_t count) const
{
  if (indices == nullptr || count == 0)
  {
    return JWPLC_LOGIC_NO_SOURCE;
  }

  uint16_t selected = indices[0];
  uint16_t bestDistance = absoluteDistance(
      _nodeY[selected],
      _nodeY[_selectedIndex]);

  for (uint8_t index = 1; index < count; ++index)
  {
    const uint16_t candidate = indices[index];
    const uint16_t distance = absoluteDistance(
        _nodeY[candidate],
        _nodeY[_selectedIndex]);
    if (distance < bestDistance)
    {
      selected = candidate;
      bestDistance = distance;
    }
  }

  return selected;
}

bool RuntimeUIFBDMapV4::selectSource()
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

bool RuntimeUIFBDMapV4::selectConsumer()
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

bool RuntimeUIFBDMapV4::selectVertical(bool down)
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

    const int16_t deltaY = static_cast<int16_t>(
        _nodeY[candidate] - selectedY);
    if ((down && deltaY <= 0) || (!down && deltaY >= 0))
    {
      continue;
    }

    const uint32_t score =
        static_cast<uint32_t>(absoluteDistance(_nodeY[candidate], selectedY)) * 4UL +
        absoluteDistance(_nodeX[candidate], selectedX);
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

void RuntimeUIFBDMapV4::handleMapInput()
{
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
    if (viewportChanged)
    {
      drawMapFull();
    }
    else
    {
      drawMapHeaderInfo();
      drawMapLive();
    }
  }

  if (JWPLC_Buttons.pressed(BTN_OK) &&
      _model->blockCount() > 0)
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::Detail;
    _detailInputIndex = 0;
    _lastDetailRefreshMs = millis();
    gateInputUntilRelease(false);
    drawDetailStatic();
    drawDetail(true);
  }
}

void RuntimeUIFBDMapV4::handleDetailInput()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  bool changed = false;
  if (definition->inputCount > 0 && JWPLC_Buttons.pressed(BTN_UP))
  {
    _detailInputIndex = _detailInputIndex == 0
                            ? static_cast<uint8_t>(definition->inputCount - 1U)
                            : static_cast<uint8_t>(_detailInputIndex - 1U);
    changed = true;
  }
  else if (definition->inputCount > 0 && JWPLC_Buttons.pressed(BTN_DOWN))
  {
    _detailInputIndex = static_cast<uint8_t>(
        (_detailInputIndex + 1U) % definition->inputCount);
    changed = true;
  }
  else if (definition->inputCount > 0 && JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (beginInputEdit())
    {
      JWPLC_Display.notifyActivity();
      _mode = Mode::EditInput;
      JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
      gateInputUntilRelease(false);
      drawInputEditStatic();
      drawInputEdit();
      return;
    }
  }
  else if (definition->inputCount > 0 && JWPLC_Buttons.pressed(BTN_LEFT))
  {
    const LogicV2InputLink *input =
        _model->inputLink(_selectedIndex, _detailInputIndex);
    if (input != nullptr && input->source() < _model->blockCount())
    {
      _selectedIndex = input->source();
      _mode = Mode::Map;
      ensureSelectionVisible();
      gateInputUntilRelease(false);
      drawMapStatic();
      drawMapFull();
      JWPLC_Display.notifyActivity();
      return;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::Map;
    gateInputUntilRelease(false);
    drawMapStatic();
    drawMapFull();
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawDetail(true);
  }
}

bool RuntimeUIFBDMapV4::beginInputEdit()
{
  if (!_editSession.begin())
  {
    return false;
  }

  const LogicV2InputLink *input =
      _editSession.inputLink(_selectedIndex, _detailInputIndex);
  if (input == nullptr)
  {
    _editSession.cancel();
    return false;
  }

  _editSourceCandidate = sourceCandidateIndex(input->source());
  _editInverted = input->inverted();
  _editFocus = EditFocus::Source;
  _editFeedback = EditFeedback::None;
  _awaitingApply = false;
  return true;
}

void RuntimeUIFBDMapV4::cancelInputEdit()
{
  _editSession.cancel();
  _awaitingApply = false;
  _editFeedback = EditFeedback::None;
  _mode = Mode::Detail;
  gateInputUntilRelease(true);
  _fullRedraw = true;
}

bool RuntimeUIFBDMapV4::selectedInputAllowsOpen() const
{
  const LogicV2BlockRecord *definition = _model != nullptr
                                             ? _model->block(_selectedIndex)
                                             : nullptr;
  if (definition == nullptr)
  {
    return false;
  }

  switch (definition->type)
  {
  case LogicV2BlockType::And:
  case LogicV2BlockType::Or:
  case LogicV2BlockType::Nand:
  case LogicV2BlockType::Nor:
  case LogicV2BlockType::Xor:
    return true;

  default:
    return false;
  }
}

uint16_t RuntimeUIFBDMapV4::sourceCandidateCount() const
{
  return static_cast<uint16_t>(
      _selectedIndex + (selectedInputAllowsOpen() ? 3U : 2U));
}

uint16_t RuntimeUIFBDMapV4::sourceCandidateAt(uint16_t candidateIndex) const
{
  const bool allowOpen = selectedInputAllowsOpen();
  if (allowOpen)
  {
    if (candidateIndex == 0)
    {
      return JWPLC_LOGIC_V2_SOURCE_OPEN;
    }
    --candidateIndex;
  }

  if (candidateIndex == 0)
  {
    return JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  }
  if (candidateIndex == 1)
  {
    return JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  }
  return static_cast<uint16_t>(candidateIndex - 2U);
}

uint16_t RuntimeUIFBDMapV4::sourceCandidateIndex(uint16_t source) const
{
  const bool allowOpen = selectedInputAllowsOpen();
  const uint16_t specialOffset = allowOpen ? 1U : 0U;

  if (allowOpen && source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    return 0;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    return specialOffset;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    return static_cast<uint16_t>(specialOffset + 1U);
  }
  if (source < _selectedIndex)
  {
    return static_cast<uint16_t>(specialOffset + 2U + source);
  }
  return specialOffset;
}

void RuntimeUIFBDMapV4::moveSourceCandidate(bool forward)
{
  const uint16_t count = sourceCandidateCount();
  if (count == 0)
  {
    _editSourceCandidate = 0;
    return;
  }

  if (forward)
  {
    _editSourceCandidate = static_cast<uint16_t>(
        (_editSourceCandidate + 1U) % count);
  }
  else
  {
    _editSourceCandidate = _editSourceCandidate == 0
                               ? static_cast<uint16_t>(count - 1U)
                               : static_cast<uint16_t>(_editSourceCandidate - 1U);
  }
}

void RuntimeUIFBDMapV4::handleEditInput()
{
  if (_awaitingApply)
  {
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    _editFocus = EditFocus::Source;
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    _editFocus = EditFocus::Logic;
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    if (_editFocus == EditFocus::Source)
    {
      moveSourceCandidate(false);
    }
    else
    {
      _editInverted = !_editInverted;
    }
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (_editFocus == EditFocus::Source)
    {
      moveSourceCandidate(true);
    }
    else
    {
      _editInverted = !_editInverted;
    }
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelInputEdit();
    return;
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const bool prepared = _editSession.setInputSource(
        _selectedIndex,
        _detailInputIndex,
        sourceCandidateAt(_editSourceCandidate),
        _editInverted);
    const bool valid = prepared &&
                       _editSession.validate() == LogicV2PrototypeError::None;

    JWPLC_Display.notifyActivity();
    if (!valid)
    {
      _editFeedback = EditFeedback::InvalidDraft;
      drawInputEdit();
      return;
    }

    _awaitingApply = true;
    _editFeedback = EditFeedback::Applying;
    _applyRequested = true;
    gateInputUntilRelease(false);
    drawInputEdit();
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawInputEdit();
  }
}
