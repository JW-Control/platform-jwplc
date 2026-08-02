#include "RuntimeUIFBDMapV8.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapV8::RuntimeUIFBDMapV8()
    : RuntimeUIFBDMapV7(),
      _addSelected(false),
      _addPreviewDrawn(false),
      _addOriginIndex(0),
      _previewMaxLevel(0),
      _previewWindowMode(HorizontalWindowMode::LeftEdge),
      _previewCentralStart(1),
      _previewViewportY(0),
      _wizardPage(WizardPage::None),
      _wizardType(WizardType::DigitalInput),
      _wizardField(0),
      _wizardSourceA(JWPLC_LOGIC_V2_SOURCE_CONST_FALSE),
      _wizardSourceB(JWPLC_LOGIC_V2_SOURCE_CONST_TRUE),
      _wizardResource(0),
      _wizardTimeUnit(WizardTimeUnit::Seconds),
      _wizardTimeValue(1),
      _wizardApplying(false),
      _wizardError(false),
      _wizardNewIndex(0)
{
}

void RuntimeUIFBDMapV8::resetAddState()
{
  _addSelected = false;
  _addPreviewDrawn = false;
  _addOriginIndex = 0;
  _previewMaxLevel = 0;
  _previewWindowMode = HorizontalWindowMode::LeftEdge;
  _previewCentralStart = 1;
  _previewViewportY = 0;

  _wizardPage = WizardPage::None;
  _wizardType = WizardType::DigitalInput;
  _wizardField = 0;
  _wizardSourceA = JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  _wizardSourceB = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  _wizardResource = 0;
  _wizardTimeUnit = WizardTimeUnit::Seconds;
  _wizardTimeValue = 1;
  _wizardApplying = false;
  _wizardError = false;
  _wizardNewIndex = 0;
}

void RuntimeUIFBDMapV8::attach(RuntimeUIV2ReadModel &model)
{
  resetAddState();
  RuntimeUIFBDMapV7::attach(model);
}

void RuntimeUIFBDMapV8::detach()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  resetAddState();
  RuntimeUIFBDMapV7::detach();
}

void RuntimeUIFBDMapV8::enter()
{
  resetAddState();
  RuntimeUIFBDMapV7::enter();
  drawAddPreview();
}

void RuntimeUIFBDMapV8::exit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  resetAddState();
  RuntimeUIFBDMapV7::exit();
}

void RuntimeUIFBDMapV8::forceRedraw()
{
  _addPreviewDrawn = false;
  RuntimeUIFBDMapV7::forceRedraw();
}

bool RuntimeUIFBDMapV8::canAddBlock() const
{
  return _model != nullptr &&
         _model->isAttached() &&
         _model->blockCount() > 0 &&
         _model->blockCount() < MAX_BLOCKS &&
         _maxLevel < MAX_LEVELS - 1U;
}

bool RuntimeUIFBDMapV8::selectedAtLastLevel() const
{
  return canAddBlock() &&
         _selectedIndex < _model->blockCount() &&
         _levels[_selectedIndex] == _maxLevel;
}

void RuntimeUIFBDMapV8::refresh(const JWPLC_IOState *io,
                                const JWPLC_RTCState *rtc)
{
  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  if (_wizardPage != WizardPage::None)
  {
    refreshWizard();
    return;
  }

  if (_addSelected && _mode == Mode::Map)
  {
    refreshAddNode();
    return;
  }

  refreshNormalMap(io, rtc);
}

void RuntimeUIFBDMapV8::refreshNormalMap(const JWPLC_IOState *io,
                                         const JWPLC_RTCState *rtc)
{
  if (_mode != Mode::Map)
  {
    RuntimeUIFBDMapV7::refresh(io, rtc);
    return;
  }

  const uint16_t previousSelection = _selectedIndex;
  const uint8_t previousMaxLevel = _maxLevel;
  const HorizontalWindowMode previousWindow = _horizontalMode;
  const uint8_t previousStart = _centralStartLevel;
  const int16_t previousViewport = _viewportY;
  const bool fullRedrawWasPending = _fullRedraw;

  RuntimeUIFBDMapV7::refresh(io, rtc);

  if (_mode != Mode::Map || _wizardPage != WizardPage::None)
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_RIGHT) &&
      previousSelection == _selectedIndex &&
      selectedAtLastLevel())
  {
    selectAddNode();
    return;
  }

  const bool geometryChanged =
      previousMaxLevel != _maxLevel ||
      previousWindow != _horizontalMode ||
      previousStart != _centralStartLevel ||
      previousViewport != _viewportY;

  if (fullRedrawWasPending || geometryChanged || !_addPreviewDrawn)
  {
    drawAddPreview();
  }
}

int16_t RuntimeUIFBDMapV8::addNodeY() const
{
  int16_t y = static_cast<int16_t>(MAP_Y + (MAP_H - NODE_H) / 2);
  if (_addOriginIndex < (_model != nullptr ? _model->blockCount() : 0))
  {
    y = screenY(_addOriginIndex);
  }

  if (y < MAP_Y + 2)
  {
    y = MAP_Y + 2;
  }
  const int16_t maximum = static_cast<int16_t>(MAP_Y + MAP_H - NODE_H - 2);
  if (y > maximum)
  {
    y = maximum;
  }
  return y;
}

bool RuntimeUIFBDMapV8::addPreviewVisible() const
{
  if (!canAddBlock())
  {
    return false;
  }

  if (_maxLevel <= 3)
  {
    return true;
  }

  return _maxLevel == 4 ||
         _horizontalMode == HorizontalWindowMode::RightEdge;
}

void RuntimeUIFBDMapV8::drawAddNode(bool selected,
                                    bool compact,
                                    int16_t x,
                                    int16_t y,
                                    int16_t width,
                                    int16_t height)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t border = selected ? COLOR_WARNING : COLOR_BORDER;

  tft.fillRect(x, y, width, height, COLOR_BACKGROUND);
  tft.drawRect(x, y, width, height, border);
  if (selected && width > 4 && height > 4)
  {
    tft.drawRect(x + 1, y + 1, width - 2, height - 2, border);
  }

  tft.setTextWrap(false);
  tft.setTextColor(selected ? COLOR_WARNING : COLOR_ACCENT,
                   COLOR_BACKGROUND);
  tft.setTextSize(compact ? 2 : 3);

  const int16_t textWidth = compact ? 12 : 18;
  const int16_t textHeight = compact ? 16 : 24;
  tft.setCursor(static_cast<int16_t>(x + (width - textWidth) / 2),
                static_cast<int16_t>(y + (height - textHeight) / 2));
  tft.print('+');
}

void RuntimeUIFBDMapV8::drawAddPreview()
{
  _addPreviewDrawn = false;
  if (_mode != Mode::Map || _addSelected || !addPreviewVisible())
  {
    return;
  }

  _addOriginIndex = _selectedIndex;
  const int16_t y = static_cast<int16_t>(
      addNodeY() + (NODE_H - ADD_PREVIEW_H) / 2);

  if (_maxLevel <= 3)
  {
    const int16_t x = static_cast<int16_t>(
        SLOT_X0 + static_cast<int16_t>(_maxLevel + 1U) * SLOT_STEP +
        (NODE_W - ADD_PREVIEW_W) / 2);
    drawAddNode(false,
                true,
                x,
                y,
                ADD_PREVIEW_W,
                ADD_PREVIEW_H);
  }
  else
  {
    const int16_t x = static_cast<int16_t>(MAP_X + MAP_W - ADD_EDGE_W);
    drawAddNode(false,
                true,
                x,
                y,
                ADD_EDGE_W,
                ADD_PREVIEW_H);
  }

  _previewMaxLevel = _maxLevel;
  _previewWindowMode = _horizontalMode;
  _previewCentralStart = _centralStartLevel;
  _previewViewportY = _viewportY;
  _addPreviewDrawn = true;
}

void RuntimeUIFBDMapV8::updateAddHeader(bool selected)
{
  updateTextField(JWPLC_Display.tft(),
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  selected ? "NUEVO BLOQUE" : "RIGHT: AGREGAR",
                  selected ? COLOR_WARNING : COLOR_MUTED,
                  COLOR_PANEL);
}

void RuntimeUIFBDMapV8::selectAddNode()
{
  if (!canAddBlock())
  {
    return;
  }

  _addOriginIndex = _selectedIndex;
  _addSelected = true;
  _addPreviewDrawn = false;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawAddSelectedMap();
}

void RuntimeUIFBDMapV8::leaveAddNode()
{
  _addSelected = false;
  _selectedIndex = _addOriginIndex < _model->blockCount()
                       ? _addOriginIndex
                       : static_cast<uint16_t>(_model->blockCount() - 1U);
  _mapSelectionCacheValid = false;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawMapStatic();
  drawMapFull();
  noteMapFullRendered();
  drawAddPreview();
}

void RuntimeUIFBDMapV8::drawAddSelectedMap()
{
  const uint16_t count = _model->blockCount();
  const uint16_t savedSelection = _selectedIndex;
  const uint8_t savedMaxLevel = _maxLevel;
  const HorizontalWindowMode savedWindow = _horizontalMode;
  const uint8_t savedStart = _centralStartLevel;

  const uint8_t virtualLevel = static_cast<uint8_t>(savedMaxLevel + 1U);
  _maxLevel = virtualLevel;
  _horizontalMode = virtualLevel <= 4
                        ? HorizontalWindowMode::LeftEdge
                        : HorizontalWindowMode::RightEdge;
  _centralStartLevel = virtualLevel <= 4
                           ? 1
                           : static_cast<uint8_t>(virtualLevel - 3U);
  _selectedIndex = count;

  drawMapStatic();
  drawMapFull();
  updateAddHeader(true);

  const int8_t slot = slotForLevel(virtualLevel);
  if (slot >= 0 && slot < static_cast<int8_t>(SLOT_COUNT))
  {
    const int16_t x = static_cast<int16_t>(
        SLOT_X0 + static_cast<int16_t>(slot) * SLOT_STEP);
    drawAddNode(true,
                false,
                x,
                addNodeY(),
                NODE_W,
                NODE_H);
  }

  _mapSelectionCache = count;
  _mapSelectionCacheValid = true;

  _selectedIndex = savedSelection;
  _maxLevel = savedMaxLevel;
  _horizontalMode = savedWindow;
  _centralStartLevel = savedStart;
}

void RuntimeUIFBDMapV8::drawAddSelectedLive()
{
  const uint16_t count = _model->blockCount();
  const uint16_t savedSelection = _selectedIndex;
  const uint8_t savedMaxLevel = _maxLevel;
  const HorizontalWindowMode savedWindow = _horizontalMode;
  const uint8_t savedStart = _centralStartLevel;

  const uint8_t virtualLevel = static_cast<uint8_t>(savedMaxLevel + 1U);
  _maxLevel = virtualLevel;
  _horizontalMode = virtualLevel <= 4
                        ? HorizontalWindowMode::LeftEdge
                        : HorizontalWindowMode::RightEdge;
  _centralStartLevel = virtualLevel <= 4
                           ? 1
                           : static_cast<uint8_t>(virtualLevel - 3U);
  _selectedIndex = count;

  drawMapLiveOptimized();
  _mapSelectionCache = count;
  _mapSelectionCacheValid = true;

  _selectedIndex = savedSelection;
  _maxLevel = savedMaxLevel;
  _horizontalMode = savedWindow;
  _centralStartLevel = savedStart;
}

void RuntimeUIFBDMapV8::refreshAddNode()
{
  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_ESC))
  {
    leaveAddNode();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    openWizard();
    return;
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastValueRefreshMs) < VALUE_REFRESH_MS)
  {
    return;
  }
  _lastValueRefreshMs = nowMs;
  drawAddSelectedLive();
}

const char *RuntimeUIFBDMapV8::wizardTypeName(WizardType type)
{
  switch (type)
  {
  case WizardType::DigitalInput:
    return "ENTRADA DI";
  case WizardType::Not:
    return "NOT";
  case WizardType::And2:
    return "AND 2";
  case WizardType::Ton:
    return "TON";
  case WizardType::DigitalOutput:
    return "SALIDA DO";
  default:
    return "?";
  }
}

LogicV2BlockType RuntimeUIFBDMapV8::wizardLogicType(WizardType type)
{
  switch (type)
  {
  case WizardType::DigitalInput:
    return LogicV2BlockType::DigitalInput;
  case WizardType::Not:
    return LogicV2BlockType::Not;
  case WizardType::And2:
    return LogicV2BlockType::And;
  case WizardType::Ton:
    return LogicV2BlockType::Ton;
  case WizardType::DigitalOutput:
    return LogicV2BlockType::DigitalOutput;
  default:
    return LogicV2BlockType::DigitalInput;
  }
}

void RuntimeUIFBDMapV8::openWizard()
{
  _wizardPage = WizardPage::Type;
  _wizardType = WizardType::DigitalInput;
  _wizardField = 0;
  _wizardApplying = false;
  _wizardError = false;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawWizardTypeScreen();
}

void RuntimeUIFBDMapV8::closeWizardToAdd()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _wizardPage = WizardPage::None;
  _wizardApplying = false;
  _wizardError = false;
  gateInputUntilRelease(false);
  drawAddSelectedMap();
}

void RuntimeUIFBDMapV8::refreshWizard()
{
  if (_wizardApplying && _applyCompleted)
  {
    handleWizardApplyCompleted();
    return;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  if (_wizardApplying)
  {
    return;
  }

  if (_wizardPage == WizardPage::Type)
  {
    handleWizardTypeInput();
  }
  else if (_wizardPage == WizardPage::Configure)
  {
    handleWizardConfigInput();
  }
}

void RuntimeUIFBDMapV8::handleWizardApplyCompleted()
{
  const bool success = _applySuccess;
  _applyCompleted = false;
  _awaitingApply = false;
  _wizardApplying = false;

  if (!success)
  {
    _editSession.cancel();
    _wizardError = true;
    gateInputUntilRelease(false);
    drawWizardConfigScreen();
    return;
  }

  _editSession.cancel();
  _wizardPage = WizardPage::None;
  _wizardError = false;
  _addSelected = false;
  _mode = Mode::Map;

  invalidateLayout();
  buildLayout();
  _selectedIndex = _wizardNewIndex < _model->blockCount()
                       ? _wizardNewIndex
                       : static_cast<uint16_t>(_model->blockCount() - 1U);
  normalizeSelection();
  ensureSelectionVisible();
  _mapSelectionCacheValid = false;
  _addPreviewDrawn = false;
  _lastValueRefreshMs = millis();

  gateInputUntilRelease(false);
  drawMapStatic();
  drawMapFull();
  noteMapFullRendered();
  drawAddPreview();
}

void RuntimeUIFBDMapV8::handleWizardTypeInput()
{
  bool changed = false;
  const uint8_t count = static_cast<uint8_t>(WizardType::Count);
  uint8_t index = static_cast<uint8_t>(_wizardType);

  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    index = index == 0 ? static_cast<uint8_t>(count - 1U)
                       : static_cast<uint8_t>(index - 1U);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    index = static_cast<uint8_t>((index + 1U) % count);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    initializeWizardConfig();
    _wizardPage = WizardPage::Configure;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawWizardConfigScreen();
    return;
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC) ||
           JWPLC_Buttons.pressed(BTN_LEFT))
  {
    closeWizardToAdd();
    return;
  }

  if (changed)
  {
    _wizardType = static_cast<WizardType>(index);
    JWPLC_Display.notifyActivity();
    drawWizardTypeScreen();
  }
}

uint8_t RuntimeUIFBDMapV8::wizardFieldCount() const
{
  switch (_wizardType)
  {
  case WizardType::DigitalInput:
  case WizardType::Not:
    return 1;
  case WizardType::And2:
  case WizardType::DigitalOutput:
    return 2;
  case WizardType::Ton:
    return 3;
  default:
    return 0;
  }
}

const char *RuntimeUIFBDMapV8::wizardFieldName(uint8_t field) const
{
  switch (_wizardType)
  {
  case WizardType::DigitalInput:
    return "RECURSO";
  case WizardType::Not:
    return "FUENTE";
  case WizardType::And2:
    return field == 0 ? "IN1" : "IN2";
  case WizardType::Ton:
    if (field == 0)
      return "FUENTE";
    return field == 1 ? "VALOR" : "UNIDAD";
  case WizardType::DigitalOutput:
    return field == 0 ? "FUENTE" : "RECURSO";
  default:
    return "?";
  }
}

bool RuntimeUIFBDMapV8::wizardFieldIsSource(uint8_t field,
                                            bool &secondSource) const
{
  secondSource = false;
  if (_wizardType == WizardType::Not ||
      _wizardType == WizardType::Ton)
  {
    return field == 0;
  }
  if (_wizardType == WizardType::DigitalOutput)
  {
    return field == 0;
  }
  if (_wizardType == WizardType::And2)
  {
    secondSource = field == 1;
    return field < 2;
  }
  return false;
}

bool RuntimeUIFBDMapV8::wizardFieldIsResource(uint8_t field) const
{
  return (_wizardType == WizardType::DigitalInput && field == 0) ||
         (_wizardType == WizardType::DigitalOutput && field == 1);
}

bool RuntimeUIFBDMapV8::wizardFieldIsTimeValue(uint8_t field) const
{
  return _wizardType == WizardType::Ton && field == 1;
}

bool RuntimeUIFBDMapV8::wizardFieldIsTimeUnit(uint8_t field) const
{
  return _wizardType == WizardType::Ton && field == 2;
}

void RuntimeUIFBDMapV8::initializeWizardConfig()
{
  _wizardField = 0;
  _wizardError = false;
  _wizardSourceA = _addOriginIndex < _model->blockCount()
                       ? _addOriginIndex
                       : JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  _wizardSourceB = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  _wizardTimeUnit = WizardTimeUnit::Seconds;
  _wizardTimeValue = WIZARD_DEFAULT_T_MS / 1000UL;
  if (_wizardTimeValue == 0)
  {
    _wizardTimeValue = 1;
  }

  if (_wizardType == WizardType::DigitalOutput)
  {
    _wizardResource = firstAvailableOutputResource();
  }
  else
  {
    _wizardResource = 0;
  }
}

void RuntimeUIFBDMapV8::handleWizardConfigInput()
{
  bool changed = false;
  const uint8_t fieldCount = wizardFieldCount();

  if (wizardFieldIsTimeValue(_wizardField))
  {
    const uint32_t multiplier = wizardUnitMultiplier(_wizardTimeUnit);
    const uint32_t maximum = multiplier == 0
                                 ? UINT32_MAX
                                 : UINT32_MAX / multiplier;
    if (JWPLC_Buttons.applyAxis(&_wizardTimeValue,
                                0,
                                maximum,
                                BTN_DOWN,
                                BTN_UP,
                                false,
                                true))
    {
      changed = true;
    }
  }

  if (!changed && JWPLC_Buttons.pressed(BTN_LEFT))
  {
    _wizardField = _wizardField == 0
                       ? static_cast<uint8_t>(fieldCount - 1U)
                       : static_cast<uint8_t>(_wizardField - 1U);
    changed = true;
  }
  else if (!changed && JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    _wizardField = static_cast<uint8_t>((_wizardField + 1U) % fieldCount);
    changed = true;
  }
  else if (!changed &&
           (JWPLC_Buttons.pressed(BTN_UP) ||
            JWPLC_Buttons.pressed(BTN_DOWN)))
  {
    const bool forward = JWPLC_Buttons.pressed(BTN_DOWN);
    bool secondSource = false;
    if (wizardFieldIsSource(_wizardField, secondSource))
    {
      const bool allowOpen = _wizardType == WizardType::And2;
      uint16_t &source = secondSource ? _wizardSourceB : _wizardSourceA;
      moveWizardSource(source, forward, allowOpen);
      changed = true;
    }
    else if (wizardFieldIsResource(_wizardField))
    {
      moveWizardResource(forward);
      changed = true;
    }
    else if (wizardFieldIsTimeUnit(_wizardField))
    {
      moveWizardTimeUnit(forward);
      changed = true;
    }
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    requestWizardCreate();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    _wizardPage = WizardPage::Type;
    _wizardError = false;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawWizardTypeScreen();
    return;
  }

  if (changed)
  {
    _wizardError = false;
    JWPLC_Display.notifyActivity();
    drawWizardConfigFields();
    drawWizardFooter("OK CREAR   ESC ATRAS", COLOR_MUTED);
  }
}

uint16_t RuntimeUIFBDMapV8::wizardSourceCandidateCount(bool allowOpen) const
{
  return static_cast<uint16_t>(
      _model->blockCount() + (allowOpen ? 3U : 2U));
}

uint16_t RuntimeUIFBDMapV8::wizardSourceCandidateAt(uint16_t candidate,
                                                    bool allowOpen) const
{
  if (allowOpen)
  {
    if (candidate == 0)
    {
      return JWPLC_LOGIC_V2_SOURCE_OPEN;
    }
    --candidate;
  }

  if (candidate == 0)
  {
    return JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  }
  if (candidate == 1)
  {
    return JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  }
  return static_cast<uint16_t>(candidate - 2U);
}

uint16_t RuntimeUIFBDMapV8::wizardSourceCandidateIndex(uint16_t source,
                                                       bool allowOpen) const
{
  const uint16_t offset = allowOpen ? 1U : 0U;
  if (allowOpen && source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    return 0;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    return offset;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    return static_cast<uint16_t>(offset + 1U);
  }
  if (source < _model->blockCount())
  {
    return static_cast<uint16_t>(offset + 2U + source);
  }
  return offset;
}

void RuntimeUIFBDMapV8::moveWizardSource(uint16_t &source,
                                         bool forward,
                                         bool allowOpen)
{
  const uint16_t count = wizardSourceCandidateCount(allowOpen);
  if (count == 0)
  {
    return;
  }

  uint16_t index = wizardSourceCandidateIndex(source, allowOpen);
  index = forward
              ? static_cast<uint16_t>((index + 1U) % count)
              : (index == 0 ? static_cast<uint16_t>(count - 1U)
                            : static_cast<uint16_t>(index - 1U));
  source = wizardSourceCandidateAt(index, allowOpen);
}

void RuntimeUIFBDMapV8::formatWizardSource(uint16_t source,
                                           char *destination,
                                           size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(destination, capacity, "ABIERTO");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(destination, capacity, "HI");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(destination, capacity, "LO");
    return;
  }

  const LogicV2BlockRecord *definition = _model->block(source);
  std::snprintf(destination,
                capacity,
                "B%02u %s",
                static_cast<unsigned>(source),
                definition != nullptr
                    ? _model->typeShort(definition->type)
                    : "?");
}

uint8_t RuntimeUIFBDMapV8::digitalInputCount() const
{
  const LogicV2EnginePrototype *engine = _model != nullptr
                                             ? _model->engine()
                                             : nullptr;
  return engine != nullptr ? engine->digitalInputCount() : 0;
}

uint8_t RuntimeUIFBDMapV8::digitalOutputCount() const
{
  const LogicV2EnginePrototype *engine = _model != nullptr
                                             ? _model->engine()
                                             : nullptr;
  return engine != nullptr ? engine->digitalOutputCount() : 0;
}

bool RuntimeUIFBDMapV8::outputResourceUsed(uint8_t resource) const
{
  const uint16_t count = _model->blockCount();
  for (uint16_t block = 0; block < count; ++block)
  {
    const LogicV2BlockRecord *definition = _model->block(block);
    if (definition != nullptr &&
        definition->type == LogicV2BlockType::DigitalOutput &&
        definition->resource == resource)
    {
      return true;
    }
  }
  return false;
}

uint8_t RuntimeUIFBDMapV8::firstAvailableOutputResource() const
{
  const uint8_t count = digitalOutputCount();
  for (uint8_t resource = 0; resource < count; ++resource)
  {
    if (!outputResourceUsed(resource))
    {
      return resource;
    }
  }
  return 0xFFU;
}

void RuntimeUIFBDMapV8::moveWizardResource(bool forward)
{
  if (_wizardType == WizardType::DigitalInput)
  {
    const uint8_t count = digitalInputCount();
    if (count == 0)
    {
      _wizardResource = 0xFFU;
      return;
    }
    if (_wizardResource >= count)
    {
      _wizardResource = 0;
      return;
    }
    _wizardResource = forward
                          ? static_cast<uint8_t>((_wizardResource + 1U) % count)
                          : (_wizardResource == 0
                                 ? static_cast<uint8_t>(count - 1U)
                                 : static_cast<uint8_t>(_wizardResource - 1U));
    return;
  }

  const uint8_t count = digitalOutputCount();
  if (count == 0)
  {
    _wizardResource = 0xFFU;
    return;
  }

  uint8_t candidate = _wizardResource < count
                          ? _wizardResource
                          : firstAvailableOutputResource();
  for (uint8_t step = 0; step < count; ++step)
  {
    candidate = forward
                    ? static_cast<uint8_t>((candidate + 1U) % count)
                    : (candidate == 0
                           ? static_cast<uint8_t>(count - 1U)
                           : static_cast<uint8_t>(candidate - 1U));
    if (!outputResourceUsed(candidate))
    {
      _wizardResource = candidate;
      return;
    }
  }
  _wizardResource = 0xFFU;
}

uint32_t RuntimeUIFBDMapV8::wizardUnitMultiplier(WizardTimeUnit unit)
{
  switch (unit)
  {
  case WizardTimeUnit::Milliseconds:
    return 1UL;
  case WizardTimeUnit::Seconds:
    return 1000UL;
  case WizardTimeUnit::Minutes:
    return 60000UL;
  case WizardTimeUnit::Hours:
  default:
    return 3600000UL;
  }
}

const char *RuntimeUIFBDMapV8::wizardUnitText(WizardTimeUnit unit)
{
  switch (unit)
  {
  case WizardTimeUnit::Milliseconds:
    return "ms";
  case WizardTimeUnit::Seconds:
    return "s";
  case WizardTimeUnit::Minutes:
    return "min";
  case WizardTimeUnit::Hours:
  default:
    return "h";
  }
}

uint32_t RuntimeUIFBDMapV8::wizardTimeMilliseconds() const
{
  const uint32_t multiplier = wizardUnitMultiplier(_wizardTimeUnit);
  if (multiplier != 0 && _wizardTimeValue > UINT32_MAX / multiplier)
  {
    return UINT32_MAX;
  }
  return _wizardTimeValue * multiplier;
}

void RuntimeUIFBDMapV8::moveWizardTimeValue(bool increase)
{
  const uint32_t multiplier = wizardUnitMultiplier(_wizardTimeUnit);
  const uint32_t maximum = multiplier == 0
                               ? UINT32_MAX
                               : UINT32_MAX / multiplier;
  if (increase)
  {
    if (_wizardTimeValue < maximum)
    {
      ++_wizardTimeValue;
    }
  }
  else if (_wizardTimeValue > 0)
  {
    --_wizardTimeValue;
  }
}

void RuntimeUIFBDMapV8::moveWizardTimeUnit(bool forward)
{
  const uint32_t milliseconds = wizardTimeMilliseconds();
  uint8_t unit = static_cast<uint8_t>(_wizardTimeUnit);
  unit = forward
             ? static_cast<uint8_t>((unit + 1U) % 4U)
             : (unit == 0 ? 3U : static_cast<uint8_t>(unit - 1U));
  _wizardTimeUnit = static_cast<WizardTimeUnit>(unit);

  const uint32_t multiplier = wizardUnitMultiplier(_wizardTimeUnit);
  _wizardTimeValue = multiplier == 0
                         ? 0
                         : static_cast<uint32_t>(
                               (static_cast<uint64_t>(milliseconds) +
                                multiplier - 1UL) /
                               multiplier);
}

void RuntimeUIFBDMapV8::formatWizardFieldValue(uint8_t field,
                                               char *destination,
                                               size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  bool secondSource = false;
  if (wizardFieldIsSource(field, secondSource))
  {
    formatWizardSource(secondSource ? _wizardSourceB : _wizardSourceA,
                       destination,
                       capacity);
    return;
  }

  if (wizardFieldIsResource(field))
  {
    if (_wizardResource == 0xFFU)
    {
      std::snprintf(destination, capacity, "SIN RECURSO");
    }
    else
    {
      std::snprintf(destination,
                    capacity,
                    _wizardType == WizardType::DigitalInput
                        ? "I0.%u"
                        : "Q0.%u",
                    static_cast<unsigned>(_wizardResource));
    }
    return;
  }

  if (wizardFieldIsTimeValue(field))
  {
    std::snprintf(destination,
                  capacity,
                  "%lu",
                  static_cast<unsigned long>(_wizardTimeValue));
    return;
  }

  if (wizardFieldIsTimeUnit(field))
  {
    std::snprintf(destination,
                  capacity,
                  "%s",
                  wizardUnitText(_wizardTimeUnit));
    return;
  }

  destination[0] = '\0';
}

void RuntimeUIFBDMapV8::requestWizardCreate()
{
  if (!_editSession.begin())
  {
    _wizardError = true;
    drawWizardFooter("ERROR AL ABRIR BORRADOR", COLOR_ERROR);
    return;
  }

  LogicV2InputLink inputs[2];
  uint8_t inputCount = 0;
  uint16_t resource = 0;
  uint32_t parameter = 0;

  const auto makeLink = [](uint16_t source) -> LogicV2InputLink
  {
    if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
      return LogicV2InputLink::open();
    if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
      return LogicV2InputLink::constantTrue();
    if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
      return LogicV2InputLink::constantFalse();
    return LogicV2InputLink::block(source);
  };

  switch (_wizardType)
  {
  case WizardType::DigitalInput:
    resource = _wizardResource;
    break;

  case WizardType::Not:
    inputs[0] = makeLink(_wizardSourceA);
    inputCount = 1;
    break;

  case WizardType::And2:
    inputs[0] = makeLink(_wizardSourceA);
    inputs[1] = makeLink(_wizardSourceB);
    inputCount = 2;
    break;

  case WizardType::Ton:
    inputs[0] = makeLink(_wizardSourceA);
    inputCount = 1;
    parameter = wizardTimeMilliseconds();
    break;

  case WizardType::DigitalOutput:
    inputs[0] = makeLink(_wizardSourceA);
    inputCount = 1;
    resource = _wizardResource;
    break;

  default:
    _editSession.cancel();
    _wizardError = true;
    drawWizardFooter("TIPO NO VALIDO", COLOR_ERROR);
    return;
  }

  uint16_t newIndex = 0;
  const bool prepared = _editSession.appendBlock(
      wizardLogicType(_wizardType),
      inputCount > 0 ? inputs : nullptr,
      inputCount,
      resource,
      parameter,
      &newIndex);
  const bool valid = prepared &&
                     _editSession.validate() == LogicV2PrototypeError::None;

  if (!valid)
  {
    _editSession.cancel();
    _wizardError = true;
    drawWizardFooter("CONFIGURACION NO VALIDA", COLOR_ERROR);
    return;
  }

  _wizardNewIndex = newIndex;
  _wizardApplying = true;
  _wizardError = false;
  _awaitingApply = true;
  _applyRequested = true;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawWizardFooter("APLICANDO CAMBIOS...", COLOR_WARNING);
}

void RuntimeUIFBDMapV8::drawWizardTypeScreen()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "NUEVO BLOQUE");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);

  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);

  const uint8_t count = static_cast<uint8_t>(WizardType::Count);
  for (uint8_t index = 0; index < count; ++index)
  {
    drawMenuButton(tft,
                   8,
                   static_cast<int16_t>(33 + index * 25),
                   132,
                   22,
                   wizardTypeName(static_cast<WizardType>(index)),
                   static_cast<uint8_t>(_wizardType) == index);
  }

  drawPanel(tft, 148, 34, 162, 122, "CREAR AL FINAL");
  drawFieldLabel(tft, 157, 57, "UP/DOWN TIPO");
  drawFieldLabel(tft, 157, 76, "OK CONFIGURAR");
  drawFieldLabel(tft, 157, 95, "ESC VOLVER");
  drawFieldLabel(tft, 157, 122, "RAM / SIN FRAM", COLOR_ACCENT);
}

void RuntimeUIFBDMapV8::drawWizardConfigScreen()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "CONFIGURAR");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);

  updateTextField(tft,
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  wizardTypeName(_wizardType),
                  COLOR_MUTED,
                  COLOR_PANEL);

  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);

  drawWizardConfigFields();
  drawWizardFooter(_wizardError
                       ? "CONFIGURACION NO VALIDA"
                       : "OK CREAR   ESC ATRAS",
                   _wizardError ? COLOR_ERROR : COLOR_MUTED);
}

void RuntimeUIFBDMapV8::drawWizardConfigFields()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint8_t count = wizardFieldCount();

  for (uint8_t field = 0; field < count; ++field)
  {
    char value[28];
    char label[48];
    formatWizardFieldValue(field, value, sizeof(value));
    std::snprintf(label,
                  sizeof(label),
                  "%s  <%s>",
                  wizardFieldName(field),
                  value);

    drawMenuButton(tft,
                   10,
                   static_cast<int16_t>(48 + field * 34),
                   300,
                   28,
                   label,
                   field == _wizardField);
  }

  for (uint8_t field = count; field < 3; ++field)
  {
    tft.fillRect(10,
                 static_cast<int16_t>(48 + field * 34),
                 300,
                 28,
                 COLOR_PANEL);
  }
}

void RuntimeUIFBDMapV8::drawWizardFooter(const char *text,
                                         uint16_t color)
{
  updateTextField(JWPLC_Display.tft(),
                  10,
                  156,
                  48,
                  text,
                  color,
                  COLOR_PANEL);
}
