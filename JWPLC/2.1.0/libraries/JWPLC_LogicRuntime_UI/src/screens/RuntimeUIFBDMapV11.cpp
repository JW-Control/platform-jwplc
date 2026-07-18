#include "RuntimeUIFBDMapV11.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapV11::RuntimeUIFBDMapV11()
    : RuntimeUIFBDMapV10(),
      _configView(ConfigView::Main),
      _mainFocus(MainFocus::Sources),
      _sourceInputIndex(0),
      _sourceBackup(JWPLC_LOGIC_V2_SOURCE_CONST_FALSE),
      _parameterIndex(0),
      _parameterField(ParameterField::Value),
      _resourceBackup(0),
      _timeUnitBackup(WizardTimeUnit::Seconds),
      _timeValueBackup(1)
{
}

void RuntimeUIFBDMapV11::resetV11State()
{
  _configView = ConfigView::Main;
  _mainFocus = MainFocus::Sources;
  _sourceInputIndex = 0;
  _sourceBackup = JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  _parameterIndex = 0;
  _parameterField = ParameterField::Value;
  _resourceBackup = 0;
  _timeUnitBackup = WizardTimeUnit::Seconds;
  _timeValueBackup = 1;
}

void RuntimeUIFBDMapV11::attach(RuntimeUIV2ReadModel &model)
{
  resetV11State();
  RuntimeUIFBDMapV10::attach(model);
}

void RuntimeUIFBDMapV11::detach()
{
  restoreWizardRepeatProfile();
  resetV11State();
  RuntimeUIFBDMapV10::detach();
}

void RuntimeUIFBDMapV11::enter()
{
  restoreWizardRepeatProfile();
  resetAddState();
  resetV11State();
  _unsafePreviewClean = false;

  // Se omite RuntimeUIFBDMapV8::enter(), porque esa revisión dibuja el preview
  // compacto antes de conocer si hay espacio. El nodo + solo aparecerá al
  // seleccionarlo explícitamente con RIGHT.
  RuntimeUIFBDMapV7::enter();
  _addPreviewDrawn = true;
}

void RuntimeUIFBDMapV11::exit()
{
  restoreWizardRepeatProfile();
  resetV11State();
  RuntimeUIFBDMapV10::exit();
}

void RuntimeUIFBDMapV11::forceRedraw()
{
  RuntimeUIFBDMapV10::forceRedraw();
  _addPreviewDrawn = true;
}

void RuntimeUIFBDMapV11::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  if (_model == nullptr || !_model->isAttached())
  {
    RuntimeUIFBDMapV10::refresh(io, rtc);
    return;
  }

  if (_wizardPage == WizardPage::Type)
  {
    updateHeaderStateIfNeeded(false);
    if (consumeInputReleaseGate())
    {
      return;
    }
    handleTypeScreenV11();
    return;
  }

  if (_wizardPage == WizardPage::Configure)
  {
    updateHeaderStateIfNeeded(false);

    if (_wizardApplying && _applyCompleted)
    {
      handleApplyCompletedV11();
      return;
    }

    if (consumeInputReleaseGate() || _wizardApplying)
    {
      return;
    }

    switch (_configView)
    {
    case ConfigView::Main:
      handleMainInput();
      break;
    case ConfigView::SourceList:
      handleSourceListInput();
      break;
    case ConfigView::SourceEdit:
      handleSourceEditInput();
      break;
    case ConfigView::ParameterList:
      handleParameterListInput();
      break;
    case ConfigView::ParameterEdit:
      handleParameterEditInput();
      break;
    }
    return;
  }

  if (_addSelected && _mode == Mode::Map && handleAddBackV11())
  {
    return;
  }

  // Nunca se dibuja un preview compacto. RIGHT sigue siendo interceptado por
  // RuntimeUIFBDMapV9 y abre la columna virtual completa.
  _addPreviewDrawn = true;
  RuntimeUIFBDMapV10::refresh(io, rtc);
  if (_wizardPage == WizardPage::None && !_addSelected)
  {
    _addPreviewDrawn = true;
  }
}

bool RuntimeUIFBDMapV11::handleAddBackV11()
{
  if (_inputReleaseGate)
  {
    return false;
  }

  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool escape = JWPLC_Buttons.pressed(BTN_ESC);
  if (!left && !escape)
  {
    return false;
  }

  _addSelected = false;
  _selectedIndex = _addOriginIndex < _model->blockCount()
                       ? _addOriginIndex
                       : static_cast<uint16_t>(_model->blockCount() - 1U);
  _mapSelectionCacheValid = false;
  _addPreviewDrawn = true;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawMapStatic();
  drawMapFull();
  noteMapFullRendered();
  return true;
}

bool RuntimeUIFBDMapV11::handleTypeScreenV11()
{
  if (_wizardApplying)
  {
    return false;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    closeWizardToAdd();
    _addPreviewDrawn = true;
    JWPLC_Display.notifyActivity();
    return true;
  }

  // ESC es la única salida de NUEVO BLOQUE.
  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    JWPLC_Display.notifyActivity();
    return true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    initializeWizardConfig();
    enterConfigureMain();
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const uint8_t count = static_cast<uint8_t>(WizardType::Count);
  const WizardType previous = _wizardType;
  uint8_t index = static_cast<uint8_t>(_wizardType);
  index = up
              ? (index == 0 ? static_cast<uint8_t>(count - 1U)
                            : static_cast<uint8_t>(index - 1U))
              : static_cast<uint8_t>((index + 1U) % count);
  _wizardType = static_cast<WizardType>(index);
  _wizardError = false;
  JWPLC_Display.notifyActivity();
  drawWizardTypeChoice(previous, false);
  drawWizardTypeChoice(_wizardType, true);
  return true;
}

void RuntimeUIFBDMapV11::enterConfigureMain()
{
  restoreWizardRepeatProfile();
  _wizardPage = WizardPage::Configure;
  _wizardError = false;
  _wizardApplying = false;
  _configView = ConfigView::Main;
  _sourceInputIndex = 0;
  _parameterIndex = 0;
  _parameterField = ParameterField::Value;
  _mainFocus = hasSourceGroup()
                   ? MainFocus::Sources
                   : (hasParameterGroup() ? MainFocus::Parameters
                                          : MainFocus::Create);
  normalizeMainFocus();
  drawMainScreen();
}

bool RuntimeUIFBDMapV11::hasSourceGroup() const
{
  return sourceInputCount() > 0;
}

uint8_t RuntimeUIFBDMapV11::sourceInputCount() const
{
  switch (_wizardType)
  {
  case WizardType::Not:
  case WizardType::Ton:
  case WizardType::DigitalOutput:
    return 1;
  case WizardType::And2:
    return 2;
  default:
    return 0;
  }
}

uint16_t &RuntimeUIFBDMapV11::sourceReference(uint8_t inputIndex)
{
  return inputIndex == 0 ? _wizardSourceA : _wizardSourceB;
}

uint16_t RuntimeUIFBDMapV11::sourceReference(uint8_t inputIndex) const
{
  return inputIndex == 0 ? _wizardSourceA : _wizardSourceB;
}

const char *RuntimeUIFBDMapV11::sourceInputName(uint8_t inputIndex) const
{
  if (_wizardType == WizardType::And2)
  {
    return inputIndex == 0 ? "IN1" : "IN2";
  }
  return "FUENTE";
}

bool RuntimeUIFBDMapV11::hasParameterGroup() const
{
  return parameterCount() > 0;
}

uint8_t RuntimeUIFBDMapV11::parameterCount() const
{
  switch (_wizardType)
  {
  case WizardType::DigitalInput:
  case WizardType::Ton:
  case WizardType::DigitalOutput:
    return 1;
  default:
    return 0;
  }
}

const char *RuntimeUIFBDMapV11::parameterName(uint8_t parameterIndex) const
{
  (void)parameterIndex;
  return _wizardType == WizardType::Ton ? "T" : "RECURSO";
}

void RuntimeUIFBDMapV11::formatParameterValue(
    uint8_t parameterIndex,
    char *destination,
    size_t capacity) const
{
  (void)parameterIndex;
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  if (_wizardType == WizardType::Ton)
  {
    std::snprintf(destination,
                  capacity,
                  "%lu %s",
                  static_cast<unsigned long>(_wizardTimeValue),
                  wizardUnitText(_wizardTimeUnit));
    return;
  }

  if (_wizardResource == 0xFFU)
  {
    std::snprintf(destination, capacity, "SIN RECURSO");
    return;
  }

  std::snprintf(destination,
                capacity,
                _wizardType == WizardType::DigitalInput
                    ? "I0.%u"
                    : "Q0.%u",
                static_cast<unsigned>(_wizardResource));
}

bool RuntimeUIFBDMapV11::mainFocusSelectable(MainFocus focus) const
{
  switch (focus)
  {
  case MainFocus::Sources:
    return hasSourceGroup();
  case MainFocus::Parameters:
    return hasParameterGroup();
  case MainFocus::Create:
  default:
    return true;
  }
}

void RuntimeUIFBDMapV11::normalizeMainFocus()
{
  if (mainFocusSelectable(_mainFocus))
  {
    return;
  }
  _mainFocus = hasSourceGroup()
                   ? MainFocus::Sources
                   : (hasParameterGroup() ? MainFocus::Parameters
                                          : MainFocus::Create);
}

void RuntimeUIFBDMapV11::moveMainFocus(bool forward)
{
  uint8_t index = static_cast<uint8_t>(_mainFocus);
  for (uint8_t step = 0; step < 3; ++step)
  {
    index = forward
                ? static_cast<uint8_t>((index + 1U) % 3U)
                : (index == 0 ? 2U : static_cast<uint8_t>(index - 1U));
    const MainFocus candidate = static_cast<MainFocus>(index);
    if (mainFocusSelectable(candidate))
    {
      _mainFocus = candidate;
      return;
    }
  }
}

void RuntimeUIFBDMapV11::drawDisabledButton(int16_t x,
                                            int16_t y,
                                            int16_t width,
                                            int16_t height,
                                            const char *label)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(x, y, width, height, COLOR_PANEL);
  tft.drawRect(x, y, width, height, COLOR_MUTED);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  const int16_t textWidth = static_cast<int16_t>(std::strlen(label) * 6U);
  tft.setCursor(static_cast<int16_t>(x + (width - textWidth) / 2),
                static_cast<int16_t>(y + (height - 8) / 2));
  tft.print(label);
}

void RuntimeUIFBDMapV11::drawMainGroup(MainFocus focus)
{
  const uint8_t index = static_cast<uint8_t>(focus);
  const int16_t x = static_cast<int16_t>(
      GROUP_X0 + index * (GROUP_W + GROUP_GAP));

  const char *label = "CREAR";
  bool enabled = true;
  if (focus == MainFocus::Sources)
  {
    enabled = hasSourceGroup();
    label = enabled ? "FUENTE" : "SIN FUENTE";
  }
  else if (focus == MainFocus::Parameters)
  {
    enabled = hasParameterGroup();
    label = enabled ? "PARAMETROS" : "SIN PARAMETROS";
  }

  if (!enabled)
  {
    drawDisabledButton(x, GROUP_Y, GROUP_W, GROUP_H, label);
    return;
  }

  drawMenuButton(JWPLC_Display.tft(),
                 x,
                 GROUP_Y,
                 GROUP_W,
                 GROUP_H,
                 label,
                 _mainFocus == focus);
}

void RuntimeUIFBDMapV11::drawConfigHeader(const char *subtitle)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "CONFIGURAR");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);
  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
  drawFieldLabel(tft, 10, 30, subtitle, COLOR_ACCENT, COLOR_PANEL);
}

void RuntimeUIFBDMapV11::drawConfigFooter(const char *text,
                                          uint16_t color)
{
  drawWizardFooter(text, color);
}

void RuntimeUIFBDMapV11::drawMainScreen()
{
  char subtitle[28];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "TIPO: %s",
                wizardTypeName(_wizardType));
  drawConfigHeader(subtitle);
  drawMainGroup(MainFocus::Sources);
  drawMainGroup(MainFocus::Parameters);
  drawMainGroup(MainFocus::Create);
  drawFixedContextForCurrent();
  drawConfigFooter(_mainFocus == MainFocus::Create
                       ? "OK CREAR   ESC ATRAS"
                       : "OK ABRIR   ESC ATRAS");
}

bool RuntimeUIFBDMapV11::handleMainInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    restoreWizardRepeatProfile();
    _wizardPage = WizardPage::Type;
    _wizardError = false;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawWizardTypeScreen();
    return true;
  }

  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
  if (left || right)
  {
    const MainFocus previous = _mainFocus;
    moveMainFocus(right);
    JWPLC_Display.notifyActivity();
    drawMainGroup(previous);
    drawMainGroup(_mainFocus);
    drawFixedContextForCurrent();
    drawConfigFooter(_mainFocus == MainFocus::Create
                         ? "OK CREAR   ESC ATRAS"
                         : "OK ABRIR   ESC ATRAS");
    return true;
  }

  if (!JWPLC_Buttons.pressed(BTN_OK))
  {
    return false;
  }

  JWPLC_Display.notifyActivity();
  if (_mainFocus == MainFocus::Create)
  {
    restoreWizardRepeatProfile();
    requestWizardCreate();
    return true;
  }

  if (_mainFocus == MainFocus::Sources)
  {
    _sourceInputIndex = 0;
    if (sourceInputCount() > 1)
    {
      _configView = ConfigView::SourceList;
      drawSourceListScreen();
    }
    else
    {
      beginSourceEdit();
    }
    gateInputUntilRelease(false);
    return true;
  }

  _parameterIndex = 0;
  _configView = ConfigView::ParameterList;
  drawParameterListScreen();
  gateInputUntilRelease(false);
  return true;
}

void RuntimeUIFBDMapV11::drawSourceListRow(uint8_t inputIndex,
                                           bool selected)
{
  char source[24];
  char label[44];
  formatWizardSource(sourceReference(inputIndex),
                     source,
                     sizeof(source));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                sourceInputName(inputIndex),
                source);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 static_cast<int16_t>(LIST_Y + inputIndex * LIST_STEP),
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 selected);
}

void RuntimeUIFBDMapV11::drawSourceListScreen()
{
  char subtitle[28];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "FUENTES: %s",
                wizardTypeName(_wizardType));
  drawConfigHeader(subtitle);
  for (uint8_t input = 0; input < sourceInputCount(); ++input)
  {
    drawSourceListRow(input, input == _sourceInputIndex);
  }
  drawFieldLabel(JWPLC_Display.tft(),
                 10,
                 112,
                 "UP/DOWN ENTRADA   OK ELEGIR",
                 COLOR_MUTED,
                 COLOR_PANEL);
  drawConfigFooter("OK ELEGIR   ESC ATRAS");
}

bool RuntimeUIFBDMapV11::handleSourceListInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    _configView = ConfigView::Main;
    drawMainScreen();
    gateInputUntilRelease(false);
    return true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    beginSourceEdit();
    gateInputUntilRelease(false);
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const uint8_t previous = _sourceInputIndex;
  const uint8_t count = sourceInputCount();
  _sourceInputIndex = up
                          ? (_sourceInputIndex == 0
                                 ? static_cast<uint8_t>(count - 1U)
                                 : static_cast<uint8_t>(_sourceInputIndex - 1U))
                          : static_cast<uint8_t>((_sourceInputIndex + 1U) % count);
  JWPLC_Display.notifyActivity();
  drawSourceListRow(previous, false);
  drawSourceListRow(_sourceInputIndex, true);
  return true;
}

void RuntimeUIFBDMapV11::beginSourceEdit()
{
  _sourceBackup = sourceReference(_sourceInputIndex);
  _configView = ConfigView::SourceEdit;
  drawSourceEditScreen();
}

void RuntimeUIFBDMapV11::drawSourceEditRow()
{
  char source[24];
  char label[44];
  formatWizardSource(sourceReference(_sourceInputIndex),
                     source,
                     sizeof(source));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                sourceInputName(_sourceInputIndex),
                source);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 LIST_Y,
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 true);
}

void RuntimeUIFBDMapV11::drawSourceEditScreen()
{
  char subtitle[28];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "FUENTE: %s",
                sourceInputName(_sourceInputIndex));
  drawConfigHeader(subtitle);
  drawSourceEditRow();
  drawFixedContext(sourceReference(_sourceInputIndex));
  drawConfigFooter("OK ACEPTAR   ESC CANCELAR");
}

bool RuntimeUIFBDMapV11::handleSourceEditInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    returnFromSourceEdit(false);
    return true;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    returnFromSourceEdit(true);
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const bool allowOpen = _wizardType == WizardType::And2;
  moveWizardSource(sourceReference(_sourceInputIndex), down, allowOpen);
  _wizardError = false;
  JWPLC_Display.notifyActivity();
  drawSourceEditRow();
  drawFixedContext(sourceReference(_sourceInputIndex));
  return true;
}

void RuntimeUIFBDMapV11::returnFromSourceEdit(bool accept)
{
  if (!accept)
  {
    sourceReference(_sourceInputIndex) = _sourceBackup;
  }
  _configView = sourceInputCount() > 1
                    ? ConfigView::SourceList
                    : ConfigView::Main;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  if (_configView == ConfigView::SourceList)
  {
    drawSourceListScreen();
  }
  else
  {
    drawMainScreen();
  }
}

void RuntimeUIFBDMapV11::drawParameterListRow(uint8_t parameterIndex,
                                              bool selected)
{
  char value[24];
  char label[44];
  formatParameterValue(parameterIndex, value, sizeof(value));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                parameterName(parameterIndex),
                value);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 static_cast<int16_t>(LIST_Y + parameterIndex * LIST_STEP),
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 selected);
}

void RuntimeUIFBDMapV11::drawParameterListScreen()
{
  char subtitle[30];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "PARAMETROS: %s",
                wizardTypeName(_wizardType));
  drawConfigHeader(subtitle);
  for (uint8_t parameter = 0; parameter < parameterCount(); ++parameter)
  {
    drawParameterListRow(parameter, parameter == _parameterIndex);
  }
  drawFieldLabel(JWPLC_Display.tft(),
                 10,
                 112,
                 "UP/DOWN PARAMETRO   OK EDITAR",
                 COLOR_MUTED,
                 COLOR_PANEL);
  drawConfigFooter("OK EDITAR   ESC ATRAS");
}

bool RuntimeUIFBDMapV11::handleParameterListInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    _configView = ConfigView::Main;
    drawMainScreen();
    gateInputUntilRelease(false);
    return true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    beginParameterEdit();
    gateInputUntilRelease(false);
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const uint8_t previous = _parameterIndex;
  const uint8_t count = parameterCount();
  _parameterIndex = up
                        ? (_parameterIndex == 0
                               ? static_cast<uint8_t>(count - 1U)
                               : static_cast<uint8_t>(_parameterIndex - 1U))
                        : static_cast<uint8_t>((_parameterIndex + 1U) % count);
  JWPLC_Display.notifyActivity();
  drawParameterListRow(previous, false);
  drawParameterListRow(_parameterIndex, true);
  return true;
}

void RuntimeUIFBDMapV11::beginParameterEdit()
{
  restoreWizardRepeatProfile();
  _parameterField = ParameterField::Value;
  _resourceBackup = _wizardResource;
  _timeUnitBackup = _wizardTimeUnit;
  _timeValueBackup = _wizardTimeValue;
  _configView = ConfigView::ParameterEdit;
  drawParameterEditScreen();
}

void RuntimeUIFBDMapV11::drawParameterEditValue()
{
  char value[24];
  char label[40];
  if (_wizardType == WizardType::Ton)
  {
    std::snprintf(value,
                  sizeof(value),
                  "%lu",
                  static_cast<unsigned long>(_wizardTimeValue));
    std::snprintf(label, sizeof(label), "VALOR  <%s>", value);
    drawMenuButton(JWPLC_Display.tft(),
                   10,
                   LIST_Y,
                   145,
                   LIST_ROW_H,
                   label,
                   _parameterField == ParameterField::Value);
    return;
  }

  formatParameterValue(_parameterIndex, value, sizeof(value));
  std::snprintf(label, sizeof(label), "RECURSO  <%s>", value);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 LIST_Y,
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 true);
}

void RuntimeUIFBDMapV11::drawParameterEditUnit()
{
  if (_wizardType != WizardType::Ton)
  {
    return;
  }

  char label[32];
  std::snprintf(label,
                sizeof(label),
                "UNIDAD  <%s>",
                wizardUnitText(_wizardTimeUnit));
  drawMenuButton(JWPLC_Display.tft(),
                 165,
                 LIST_Y,
                 145,
                 LIST_ROW_H,
                 label,
                 _parameterField == ParameterField::Unit);
}

void RuntimeUIFBDMapV11::drawParameterEditFields()
{
  drawParameterEditValue();
  drawParameterEditUnit();
}

void RuntimeUIFBDMapV11::drawParameterEditScreen()
{
  char subtitle[30];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "PARAMETRO: %s",
                parameterName(_parameterIndex));
  drawConfigHeader(subtitle);
  drawParameterEditFields();
  if (_wizardType == WizardType::Ton)
  {
    drawFixedContext(contextualSource());
  }
  else
  {
    char value[24];
    char message[48];
    formatParameterValue(_parameterIndex, value, sizeof(value));
    std::snprintf(message,
                  sizeof(message),
                  "%s - SIN POSICION FBD",
                  value);
    drawFixedContextMessage(message);
  }
  drawConfigFooter("OK ACEPTAR   ESC CANCELAR");
}

bool RuntimeUIFBDMapV11::handleParameterEditInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    returnFromParameterEdit(false);
    return true;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    returnFromParameterEdit(true);
    return true;
  }

  if (_wizardType == WizardType::Ton)
  {
    const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
    const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
    if (left || right)
    {
      const ParameterField previous = _parameterField;
      _parameterField = _parameterField == ParameterField::Value
                            ? ParameterField::Unit
                            : ParameterField::Value;
      restoreWizardRepeatProfile();
      _wizardField = _parameterField == ParameterField::Value ? 1 : 2;
      JWPLC_Display.notifyActivity();
      if (previous == ParameterField::Value ||
          _parameterField == ParameterField::Value)
      {
        drawParameterEditValue();
      }
      drawParameterEditUnit();
      return true;
    }

    if (_parameterField == ParameterField::Value)
    {
      _wizardField = 1;
      configureWizardRepeatProfile();
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
        JWPLC_Display.notifyActivity();
        drawParameterEditValue();
        return true;
      }
      return false;
    }

    restoreWizardRepeatProfile();
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (!up && !down)
    {
      return false;
    }
    moveWizardTimeUnit(down);
    JWPLC_Display.notifyActivity();
    drawParameterEditValue();
    drawParameterEditUnit();
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }
  moveWizardResource(down);
  JWPLC_Display.notifyActivity();
  drawParameterEditValue();
  char value[24];
  char message[48];
  formatParameterValue(_parameterIndex, value, sizeof(value));
  std::snprintf(message,
                sizeof(message),
                "%s - SIN POSICION FBD",
                value);
  drawFixedContextMessage(message);
  return true;
}

void RuntimeUIFBDMapV11::returnFromParameterEdit(bool accept)
{
  restoreWizardRepeatProfile();
  if (!accept)
  {
    _wizardResource = _resourceBackup;
    _wizardTimeUnit = _timeUnitBackup;
    _wizardTimeValue = _timeValueBackup;
  }
  _configView = ConfigView::ParameterList;
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);
  drawParameterListScreen();
}

uint16_t RuntimeUIFBDMapV11::contextualSource() const
{
  if (!hasSourceGroup())
  {
    return JWPLC_LOGIC_V2_SOURCE_OPEN;
  }
  const uint8_t input = _sourceInputIndex < sourceInputCount()
                            ? _sourceInputIndex
                            : 0;
  return sourceReference(input);
}

void RuntimeUIFBDMapV11::drawFixedContextForCurrent()
{
  if (!hasSourceGroup())
  {
    if (hasParameterGroup())
    {
      char value[24];
      char message[48];
      formatParameterValue(0, value, sizeof(value));
      std::snprintf(message,
                    sizeof(message),
                    "%s - SIN POSICION FBD",
                    value);
      drawFixedContextMessage(message);
    }
    else
    {
      drawFixedContextMessage("SIN CONTEXTO FBD");
    }
    return;
  }
  drawFixedContext(contextualSource());
}

void RuntimeUIFBDMapV11::drawFixedContextMessage(const char *message,
                                                  uint16_t color)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(FIXED_CONTEXT_X,
               FIXED_CONTEXT_Y,
               FIXED_CONTEXT_W,
               FIXED_CONTEXT_H,
               COLOR_PANEL);
  tft.drawRect(FIXED_CONTEXT_X,
               FIXED_CONTEXT_Y,
               FIXED_CONTEXT_W,
               FIXED_CONTEXT_H,
               COLOR_BORDER);
  drawFieldLabel(tft,
                 FIXED_CONTEXT_X + 5,
                 FIXED_CONTEXT_Y + 5,
                 message,
                 color,
                 COLOR_PANEL);
}

void RuntimeUIFBDMapV11::drawFixedContext(uint16_t source)
{
  if (source >= _model->blockCount())
  {
    char sourceText[24];
    char message[48];
    formatWizardSource(source, sourceText, sizeof(sourceText));
    std::snprintf(message,
                  sizeof(message),
                  "%s - SIN POSICION FBD",
                  sourceText);
    drawFixedContextMessage(message);
    return;
  }

  if (!_layoutValid)
  {
    buildLayout();
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(FIXED_CONTEXT_X,
               FIXED_CONTEXT_Y,
               FIXED_CONTEXT_W,
               FIXED_CONTEXT_H,
               COLOR_PANEL);
  tft.drawRect(FIXED_CONTEXT_X,
               FIXED_CONTEXT_Y,
               FIXED_CONTEXT_W,
               FIXED_CONTEXT_H,
               COLOR_BORDER);

  const LogicV2BlockRecord *definition = _model->block(source);
  char info[48];
  std::snprintf(info,
                sizeof(info),
                "B%02u %s   F%02u C%02u",
                static_cast<unsigned>(source),
                definition != nullptr
                    ? _model->typeShort(definition->type)
                    : "?",
                static_cast<unsigned>(_lanes[source] + 1U),
                static_cast<unsigned>(_levels[source] + 1U));
  drawFieldLabel(tft,
                 FIXED_CONTEXT_X + 5,
                 FIXED_CONTEXT_Y + 4,
                 info,
                 COLOR_WARNING,
                 COLOR_PANEL);
  drawMiniFBD(source,
              FIXED_CONTEXT_X + 4,
              FIXED_CONTEXT_Y + 17,
              FIXED_CONTEXT_W - 8,
              FIXED_CONTEXT_H - 21);
}

void RuntimeUIFBDMapV11::handleApplyCompletedV11()
{
  restoreWizardRepeatProfile();

  const bool success = _applySuccess;
  _applyCompleted = false;
  _awaitingApply = false;
  _wizardApplying = false;

  if (!success)
  {
    _editSession.cancel();
    _wizardError = true;
    gateInputUntilRelease(false);
    drawMainScreen();
    drawConfigFooter("CONFIGURACION NO VALIDA", COLOR_ERROR);
    return;
  }

  _editSession.cancel();
  _wizardPage = WizardPage::None;
  _wizardError = false;
  _addSelected = false;
  _mode = Mode::Map;
  resetV11State();

  invalidateLayout();
  buildLayout();
  _selectedIndex = _wizardNewIndex < _model->blockCount()
                       ? _wizardNewIndex
                       : static_cast<uint16_t>(_model->blockCount() - 1U);
  normalizeSelection();
  ensureSelectionVisible();
  _mapSelectionCacheValid = false;
  _addPreviewDrawn = true;
  _lastValueRefreshMs = millis();

  gateInputUntilRelease(false);
  drawMapStatic();
  drawMapFull();
  noteMapFullRendered();
}
