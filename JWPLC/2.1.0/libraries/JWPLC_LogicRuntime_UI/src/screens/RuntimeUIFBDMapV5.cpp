#include "RuntimeUIFBDMapV5.h"

#include <cstdio>
#include <cstdint>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t PARAM_VALUE_X = 10;
static constexpr int16_t PARAM_UNIT_X = 165;
static constexpr int16_t PARAM_FIELD_Y = 103;
static constexpr int16_t PARAM_FIELD_W = 145;
static constexpr int16_t PARAM_FIELD_H = 38;
}

RuntimeUIFBDMapV5::RuntimeUIFBDMapV5()
    : RuntimeUIFBDMapV4(),
      _detailFocus(DetailFocus::Inputs),
      _detailParameterIndex(0),
      _parameterEditorActive(false),
      _parameterFullRedraw(false),
      _parameterEditFocus(ParameterEditFocus::Value),
      _parameterUnit(TimeUnit::Milliseconds),
      _parameterValue(0)
{
}

void RuntimeUIFBDMapV5::resetExtensionState()
{
  _detailFocus = DetailFocus::Inputs;
  _detailParameterIndex = 0;
  _parameterEditorActive = false;
  _parameterFullRedraw = false;
  _parameterEditFocus = ParameterEditFocus::Value;
  _parameterUnit = TimeUnit::Milliseconds;
  _parameterValue = 0;
}

void RuntimeUIFBDMapV5::attach(RuntimeUIV2ReadModel &model)
{
  resetExtensionState();
  RuntimeUIFBDMapV4::attach(model);
}

void RuntimeUIFBDMapV5::detach()
{
  if (_parameterEditorActive && _editSession.active())
  {
    _editSession.cancel();
  }
  resetExtensionState();
  RuntimeUIFBDMapV4::detach();
}

void RuntimeUIFBDMapV5::enter()
{
  resetExtensionState();
  RuntimeUIFBDMapV4::enter();
}

void RuntimeUIFBDMapV5::exit()
{
  if (_parameterEditorActive && _editSession.active())
  {
    _editSession.cancel();
  }
  resetExtensionState();
  RuntimeUIFBDMapV4::exit();
}

void RuntimeUIFBDMapV5::forceRedraw()
{
  RuntimeUIFBDMapV4::forceRedraw();
  _parameterFullRedraw = true;
}

bool RuntimeUIFBDMapV5::selectedBlockHasParameters() const
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  return definition != nullptr && definition->type == LogicV2BlockType::Ton;
}

uint8_t RuntimeUIFBDMapV5::selectedParameterCount() const
{
  return selectedBlockHasParameters() ? TON_PARAMETER_COUNT : 0;
}

void RuntimeUIFBDMapV5::normalizeDetailFocus()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    _detailFocus = DetailFocus::Inputs;
    _detailInputIndex = 0;
    _detailParameterIndex = 0;
    return;
  }

  if (definition->inputCount == 0 && selectedParameterCount() > 0)
  {
    _detailFocus = DetailFocus::Parameters;
  }
  else if (_detailFocus == DetailFocus::Parameters &&
           selectedParameterCount() == 0)
  {
    _detailFocus = DetailFocus::Inputs;
  }

  if (definition->inputCount == 0)
  {
    _detailInputIndex = 0;
  }
  else if (_detailInputIndex >= definition->inputCount)
  {
    _detailInputIndex = static_cast<uint8_t>(definition->inputCount - 1U);
  }

  const uint8_t parameterCount = selectedParameterCount();
  if (parameterCount == 0)
  {
    _detailParameterIndex = 0;
  }
  else if (_detailParameterIndex >= parameterCount)
  {
    _detailParameterIndex = static_cast<uint8_t>(parameterCount - 1U);
  }
}

void RuntimeUIFBDMapV5::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  if (_parameterEditorActive)
  {
    refreshParameterEditor();
    return;
  }

  if (_mode == Mode::Detail)
  {
    refreshDetailV5();
    return;
  }

  const Mode previousMode = _mode;
  RuntimeUIFBDMapV4::refresh(io, rtc);

  if (_mode == Mode::Detail && previousMode != Mode::Detail)
  {
    _detailFocus = DetailFocus::Inputs;
    _detailParameterIndex = 0;
    normalizeDetailFocus();
    drawDetailV5(true);
    _fullRedraw = false;
  }
}

void RuntimeUIFBDMapV5::refreshDetailV5()
{
  if (_model == nullptr || !_model->isAttached())
  {
    return;
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
    normalizeDetailFocus();
    _fullRedraw = true;
  }

  if (_fullRedraw)
  {
    drawDetailStatic();
    drawDetailV5(true);
    _fullRedraw = false;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  handleDetailInputV5();
  if (_mode != Mode::Detail || _parameterEditorActive)
  {
    return;
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastDetailRefreshMs) <
      DETAIL_REFRESH_MS)
  {
    return;
  }
  _lastDetailRefreshMs = nowMs;

  if (valuesChanged())
  {
    drawDetailV5(false);
  }
  else if (selectedBlockHasParameters())
  {
    // Ta cambia aunque el booleano Q del TON todavía no haya cambiado.
    // Solo se actualiza la pequeña región de parámetros para evitar parpadeo.
    drawTonParameterPanel(_detailFocus == DetailFocus::Parameters);
  }
}

void RuntimeUIFBDMapV5::handleDetailInputV5()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  bool changed = false;

  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    if (_detailFocus == DetailFocus::Parameters &&
        definition->inputCount > 0)
    {
      _detailFocus = DetailFocus::Inputs;
      changed = true;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (_detailFocus == DetailFocus::Inputs &&
        selectedParameterCount() > 0)
    {
      _detailFocus = DetailFocus::Parameters;
      changed = true;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    if (_detailFocus == DetailFocus::Inputs &&
        definition->inputCount > 0)
    {
      _detailInputIndex = _detailInputIndex == 0
                              ? static_cast<uint8_t>(
                                    definition->inputCount - 1U)
                              : static_cast<uint8_t>(
                                    _detailInputIndex - 1U);
      changed = true;
    }
    else if (_detailFocus == DetailFocus::Parameters)
    {
      const uint8_t count = selectedParameterCount();
      if (count > 1)
      {
        _detailParameterIndex = _detailParameterIndex == 0
                                    ? static_cast<uint8_t>(count - 1U)
                                    : static_cast<uint8_t>(
                                          _detailParameterIndex - 1U);
        changed = true;
      }
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    if (_detailFocus == DetailFocus::Inputs &&
        definition->inputCount > 0)
    {
      _detailInputIndex = static_cast<uint8_t>(
          (_detailInputIndex + 1U) % definition->inputCount);
      changed = true;
    }
    else if (_detailFocus == DetailFocus::Parameters)
    {
      const uint8_t count = selectedParameterCount();
      if (count > 1)
      {
        _detailParameterIndex = static_cast<uint8_t>(
            (_detailParameterIndex + 1U) % count);
        changed = true;
      }
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    if (_detailFocus == DetailFocus::Inputs &&
        definition->inputCount > 0)
    {
      if (beginInputEdit())
      {
        JWPLC_Display.notifyActivity();
        _mode = Mode::EditInput;
        gateInputUntilRelease(false);
        drawInputEditStatic();
        drawInputEdit();
        return;
      }
    }
    else if (_detailFocus == DetailFocus::Parameters &&
             beginParameterEdit())
    {
      JWPLC_Display.notifyActivity();
      gateInputUntilRelease(false);
      drawParameterEditStatic();
      drawParameterEdit();
      return;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::Map;
    gateInputUntilRelease(true);
    drawMapStatic();
    drawMapFull();
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawDetailV5(true);
  }
}

void RuntimeUIFBDMapV5::updateDetailHeader()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  char headerInfo[28];
  if (_detailFocus == DetailFocus::Parameters &&
      selectedParameterCount() > 0)
  {
    std::snprintf(headerInfo,
                  sizeof(headerInfo),
                  "B%02u %s PARAM T",
                  static_cast<unsigned>(_selectedIndex),
                  _model->typeShort(definition->type));
  }
  else if (definition->inputCount > 0)
  {
    const char *role =
        _model->inputRole(definition->type, _detailInputIndex);
    if (role != nullptr && role[0] != '\0')
    {
      std::snprintf(headerInfo,
                    sizeof(headerInfo),
                    "B%02u %s %s",
                    static_cast<unsigned>(_selectedIndex),
                    _model->typeShort(definition->type),
                    role);
    }
    else
    {
      std::snprintf(headerInfo,
                    sizeof(headerInfo),
                    "B%02u %s IN%u/%u",
                    static_cast<unsigned>(_selectedIndex),
                    _model->typeShort(definition->type),
                    static_cast<unsigned>(_detailInputIndex + 1U),
                    static_cast<unsigned>(definition->inputCount));
    }
  }
  else
  {
    std::snprintf(headerInfo,
                  sizeof(headerInfo),
                  "B%02u %s",
                  static_cast<unsigned>(_selectedIndex),
                  _model->typeShort(definition->type));
  }

  updateTextField(JWPLC_Display.tft(),
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  headerInfo,
                  COLOR_MUTED,
                  COLOR_PANEL);
}

void RuntimeUIFBDMapV5::drawDetailV5(bool force)
{
  RuntimeUIFBDMapV4::drawDetail(force);
  normalizeDetailFocus();

  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  if (_detailFocus == DetailFocus::Parameters &&
      definition->inputCount > 0)
  {
    const uint8_t pageStart = detailPageStart();
    if (_detailInputIndex >= pageStart &&
        _detailInputIndex <
            static_cast<uint8_t>(pageStart + DETAIL_INPUTS_PER_PAGE))
    {
      drawDetailSource(
          _detailInputIndex,
          static_cast<uint8_t>(_detailInputIndex - pageStart),
          false);
    }
  }

  if (definition->type == LogicV2BlockType::Ton)
  {
    drawTonParameterPanel(_detailFocus == DetailFocus::Parameters);
  }

  updateDetailHeader();
}

void RuntimeUIFBDMapV5::drawTonParameterPanel(bool selected)
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

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(TON_PANEL_X,
               TON_PANEL_Y,
               TON_PANEL_W,
               TON_PANEL_H,
               COLOR_BACKGROUND);

  if (selected)
  {
    tft.drawRect(TON_PANEL_X,
                 TON_PANEL_Y,
                 TON_PANEL_W,
                 14,
                 COLOR_WARNING);
    tft.drawRect(TON_PANEL_X + 1,
                 TON_PANEL_Y + 1,
                 TON_PANEL_W - 2,
                 12,
                 COLOR_WARNING);
  }

  char configured[18];
  char elapsed[18];
  formatDurationCompact(definition->parameter,
                        configured,
                        sizeof(configured));
  formatDurationCompact(_model->tonElapsedMs(_selectedIndex, millis()),
                        elapsed,
                        sizeof(elapsed));

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(selected ? COLOR_WARNING : COLOR_TEXT,
                   COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X + 4, TON_PANEL_Y + 4);
  tft.print("T ");
  tft.print(configured);

  const bool timing = _model->tonTiming(_selectedIndex);
  const bool active = _model->blockValue(_selectedIndex);
  tft.setTextColor((timing || active) ? COLOR_OK : COLOR_MUTED,
                   COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X + 4, TON_PANEL_Y + 19);
  tft.print("Ta ");
  tft.print(elapsed);
}

bool RuntimeUIFBDMapV5::beginParameterEdit()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr ||
      definition->type != LogicV2BlockType::Ton ||
      !_editSession.begin())
  {
    return false;
  }

  _parameterUnit = preferredUnit(definition->parameter);
  const uint32_t multiplier = unitMultiplier(_parameterUnit);
  _parameterValue = multiplier == 0
                        ? definition->parameter
                        : definition->parameter / multiplier;
  _parameterEditFocus = ParameterEditFocus::Value;
  _editFeedback = EditFeedback::None;
  _awaitingApply = false;
  _parameterEditorActive = true;
  _parameterFullRedraw = true;
  return true;
}

void RuntimeUIFBDMapV5::cancelParameterEdit()
{
  _editSession.cancel();
  _awaitingApply = false;
  _editFeedback = EditFeedback::None;
  _parameterEditorActive = false;
  _parameterFullRedraw = false;
  _detailFocus = DetailFocus::Parameters;
  gateInputUntilRelease(false);
  _fullRedraw = true;
}

void RuntimeUIFBDMapV5::refreshParameterEditor()
{
  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  if (_applyCompleted)
  {
    handleParameterApplyCompleted();
    if (!_parameterEditorActive)
    {
      refreshDetailV5();
      return;
    }
  }

  if (_parameterFullRedraw || _fullRedraw)
  {
    drawParameterEditStatic();
    drawParameterEdit();
    _parameterFullRedraw = false;
    _fullRedraw = false;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  handleParameterEditInput();
}

void RuntimeUIFBDMapV5::handleParameterApplyCompleted()
{
  const bool success = _applySuccess;
  _applyCompleted = false;
  _awaitingApply = false;

  if (success)
  {
    _editSession.cancel();
    _editFeedback = EditFeedback::None;
    _parameterEditorActive = false;
    _detailFocus = DetailFocus::Parameters;
    invalidateLayout();
    buildLayout();
    normalizeSelection();
    ensureSelectionVisible();
    normalizeDetailFocus();
    _lastDetailRefreshMs = millis();
    gateInputUntilRelease(false);
    _fullRedraw = true;
  }
  else
  {
    _editFeedback = EditFeedback::ApplyFailed;
    gateInputUntilRelease(false);
    _parameterFullRedraw = true;
  }
}

void RuntimeUIFBDMapV5::handleParameterEditInput()
{
  if (_awaitingApply)
  {
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    if (_parameterEditFocus != ParameterEditFocus::Value)
    {
      _parameterEditFocus = ParameterEditFocus::Value;
      changed = true;
    }
    _editFeedback = EditFeedback::None;
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (_parameterEditFocus != ParameterEditFocus::Unit)
    {
      _parameterEditFocus = ParameterEditFocus::Unit;
      changed = true;
    }
    _editFeedback = EditFeedback::None;
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    if (_parameterEditFocus == ParameterEditFocus::Value)
    {
      moveParameterValue(true);
    }
    else
    {
      moveParameterUnit(false);
    }
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    if (_parameterEditFocus == ParameterEditFocus::Value)
    {
      moveParameterValue(false);
    }
    else
    {
      moveParameterUnit(true);
    }
    _editFeedback = EditFeedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelParameterEdit();
    return;
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const bool prepared = _editSession.setBlockParameter(
        _selectedIndex,
        parameterMilliseconds());
    const bool valid =
        prepared &&
        _editSession.validate() == LogicV2PrototypeError::None;

    JWPLC_Display.notifyActivity();
    if (!valid)
    {
      _editFeedback = EditFeedback::InvalidDraft;
      drawParameterEdit();
      return;
    }

    _awaitingApply = true;
    _editFeedback = EditFeedback::Applying;
    _applyRequested = true;
    gateInputUntilRelease(false);
    drawParameterEdit();
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawParameterEdit();
  }
}

void RuntimeUIFBDMapV5::drawParameterEditStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "EDITAR T");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);
  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMapV5::drawParameterEdit()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(MAP_X, MAP_Y, MAP_W, MAP_H, COLOR_PANEL);

  char headerInfo[24];
  std::snprintf(headerInfo,
                sizeof(headerInfo),
                "B%02u TON PARAM T",
                static_cast<unsigned>(_selectedIndex));
  updateTextField(tft,
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  headerInfo,
                  COLOR_MUTED,
                  COLOR_PANEL);

  char current[18];
  char elapsed[18];
  formatDurationCompact(definition->parameter,
                        current,
                        sizeof(current));
  formatDurationCompact(_model->tonElapsedMs(_selectedIndex, millis()),
                        elapsed,
                        sizeof(elapsed));

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
  tft.setCursor(22, 47);
  tft.print("TON");
  tft.setCursor(22, 66);
  tft.print("T ACTUAL  ");
  tft.print(current);
  tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  tft.setCursor(22, 83);
  tft.print("Ta LECTURA ");
  tft.print(elapsed);

  char valueField[24];
  std::snprintf(valueField,
                sizeof(valueField),
                "VALOR <%lu>",
                static_cast<unsigned long>(_parameterValue));
  drawMenuButton(tft,
                 PARAM_VALUE_X,
                 PARAM_FIELD_Y,
                 PARAM_FIELD_W,
                 PARAM_FIELD_H,
                 valueField,
                 _parameterEditFocus == ParameterEditFocus::Value);

  char unitField[24];
  std::snprintf(unitField,
                sizeof(unitField),
                "UNIDAD <%s>",
                unitText(_parameterUnit));
  drawMenuButton(tft,
                 PARAM_UNIT_X,
                 PARAM_FIELD_Y,
                 PARAM_FIELD_W,
                 PARAM_FIELD_H,
                 unitField,
                 _parameterEditFocus == ParameterEditFocus::Unit);

  const char *status = "OK GUARDAR   ESC CANCELAR";
  uint16_t statusColor = COLOR_MUTED;
  switch (_editFeedback)
  {
  case EditFeedback::InvalidDraft:
    status = "PARAMETRO NO VALIDO";
    statusColor = COLOR_ERROR;
    break;

  case EditFeedback::Applying:
    status = "APLICANDO CAMBIOS...";
    statusColor = COLOR_WARNING;
    break;

  case EditFeedback::ApplyFailed:
    status = "ERROR AL APLICAR";
    statusColor = COLOR_ERROR;
    break;

  case EditFeedback::None:
  default:
    break;
  }

  updateTextField(tft,
                  78,
                  154,
                  28,
                  status,
                  statusColor,
                  COLOR_PANEL);
}

uint32_t RuntimeUIFBDMapV5::unitMultiplier(TimeUnit unit)
{
  switch (unit)
  {
  case TimeUnit::Seconds:
    return 1000UL;
  case TimeUnit::Minutes:
    return 60000UL;
  case TimeUnit::Hours:
    return 3600000UL;
  case TimeUnit::Milliseconds:
  default:
    return 1UL;
  }
}

const char *RuntimeUIFBDMapV5::unitText(TimeUnit unit)
{
  switch (unit)
  {
  case TimeUnit::Seconds:
    return "s";
  case TimeUnit::Minutes:
    return "min";
  case TimeUnit::Hours:
    return "h";
  case TimeUnit::Milliseconds:
  default:
    return "ms";
  }
}

RuntimeUIFBDMapV5::TimeUnit
RuntimeUIFBDMapV5::preferredUnit(uint32_t milliseconds)
{
  if (milliseconds > 0 && milliseconds % 3600000UL == 0)
  {
    return TimeUnit::Hours;
  }
  if (milliseconds > 0 && milliseconds % 60000UL == 0)
  {
    return TimeUnit::Minutes;
  }
  if (milliseconds > 0 && milliseconds % 1000UL == 0)
  {
    return TimeUnit::Seconds;
  }
  return TimeUnit::Milliseconds;
}

void RuntimeUIFBDMapV5::formatDurationCompact(
    uint32_t milliseconds,
    char *destination,
    size_t capacity)
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  if (milliseconds > 0 && milliseconds % 3600000UL == 0)
  {
    std::snprintf(destination,
                  capacity,
                  "%luh",
                  static_cast<unsigned long>(
                      milliseconds / 3600000UL));
    return;
  }

  if (milliseconds > 0 && milliseconds % 60000UL == 0)
  {
    std::snprintf(destination,
                  capacity,
                  "%lumin",
                  static_cast<unsigned long>(
                      milliseconds / 60000UL));
    return;
  }

  if (milliseconds >= 1000UL)
  {
    std::snprintf(destination,
                  capacity,
                  "%lu.%03lus",
                  static_cast<unsigned long>(milliseconds / 1000UL),
                  static_cast<unsigned long>(milliseconds % 1000UL));
    return;
  }

  std::snprintf(destination,
                capacity,
                "%lums",
                static_cast<unsigned long>(milliseconds));
}

uint32_t RuntimeUIFBDMapV5::parameterMilliseconds() const
{
  const uint64_t milliseconds =
      static_cast<uint64_t>(_parameterValue) *
      static_cast<uint64_t>(unitMultiplier(_parameterUnit));
  return milliseconds > UINT32_MAX
             ? UINT32_MAX
             : static_cast<uint32_t>(milliseconds);
}

void RuntimeUIFBDMapV5::moveParameterValue(bool increase)
{
  const uint32_t multiplier = unitMultiplier(_parameterUnit);
  const uint32_t maximum =
      multiplier == 0 ? UINT32_MAX : UINT32_MAX / multiplier;

  if (increase)
  {
    if (_parameterValue < maximum)
    {
      ++_parameterValue;
    }
  }
  else if (_parameterValue > 0)
  {
    --_parameterValue;
  }
}

void RuntimeUIFBDMapV5::moveParameterUnit(bool forward)
{
  uint8_t value = static_cast<uint8_t>(_parameterUnit);
  if (forward)
  {
    value = static_cast<uint8_t>((value + 1U) % 4U);
  }
  else
  {
    value = value == 0 ? 3U : static_cast<uint8_t>(value - 1U);
  }
  _parameterUnit = static_cast<TimeUnit>(value);

  const uint32_t multiplier = unitMultiplier(_parameterUnit);
  const uint32_t maximum =
      multiplier == 0 ? UINT32_MAX : UINT32_MAX / multiplier;
  if (_parameterValue > maximum)
  {
    _parameterValue = maximum;
  }
}
