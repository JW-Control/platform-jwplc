#include "RuntimeUIFBDMapV10.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapV10::RuntimeUIFBDMapV10()
    : RuntimeUIFBDMapV9(),
      _unsafePreviewClean(false),
      _wizardRepeatActive(false),
      _wizardRepeatUnitValid(false),
      _wizardRepeatUnit(WizardTimeUnit::Milliseconds)
{
}

void RuntimeUIFBDMapV10::attach(RuntimeUIV2ReadModel &model)
{
  restoreWizardRepeatProfile();
  _unsafePreviewClean = false;
  RuntimeUIFBDMapV9::attach(model);
}

void RuntimeUIFBDMapV10::detach()
{
  restoreWizardRepeatProfile();
  _unsafePreviewClean = false;
  RuntimeUIFBDMapV9::detach();
}

void RuntimeUIFBDMapV10::enter()
{
  restoreWizardRepeatProfile();
  _unsafePreviewClean = false;
  RuntimeUIFBDMapV9::enter();
  suppressUnsafeAddPreview(true);
}

void RuntimeUIFBDMapV10::exit()
{
  restoreWizardRepeatProfile();
  _unsafePreviewClean = false;
  RuntimeUIFBDMapV9::exit();
}

void RuntimeUIFBDMapV10::forceRedraw()
{
  _unsafePreviewClean = false;
  RuntimeUIFBDMapV9::forceRedraw();
}

bool RuntimeUIFBDMapV10::unsafeAddPreview() const
{
  return _model != nullptr &&
         _model->isAttached() &&
         _mode == Mode::Map &&
         _wizardPage == WizardPage::None &&
         !_addSelected &&
         _maxLevel >= 4;
}

void RuntimeUIFBDMapV10::suppressUnsafeAddPreview(bool forceCleanup)
{
  if (!unsafeAddPreview())
  {
    _unsafePreviewClean = false;
    return;
  }

  // Con cinco o más columnas no existe un espacio lateral seguro. El preview
  // compacto se omite y el nodo + solo aparece al seleccionarlo con RIGHT.
  _addPreviewDrawn = true;
  if (!forceCleanup && _unsafePreviewClean)
  {
    return;
  }

  drawMapFull();
  noteMapFullRendered();
  _addPreviewDrawn = true;
  _unsafePreviewClean = true;
}

void RuntimeUIFBDMapV10::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  if (_model == nullptr || !_model->isAttached())
  {
    RuntimeUIFBDMapV9::refresh(io, rtc);
    return;
  }

  if (_wizardPage != WizardPage::None)
  {
    updateHeaderStateIfNeeded(false);
  }

  if (_wizardPage == WizardPage::Configure &&
      _wizardApplying &&
      _applyCompleted)
  {
    handleWizardApplyCompletedV10();
    return;
  }

  if (_wizardPage == WizardPage::Type &&
      handleWizardTypeInputIncremental())
  {
    return;
  }

  if (_wizardPage == WizardPage::Configure &&
      handleWizardConfigInputIncremental())
  {
    return;
  }

  if (_addSelected &&
      _mode == Mode::Map &&
      handleAddBackWithoutPreview())
  {
    return;
  }

  const bool wasAddSelected = _addSelected;
  const WizardPage previousWizardPage = _wizardPage;
  const Mode previousMode = _mode;

  if (unsafeAddPreview())
  {
    _addPreviewDrawn = true;
  }

  RuntimeUIFBDMapV9::refresh(io, rtc);

  if (unsafeAddPreview())
  {
    const bool returnedToMap =
        wasAddSelected ||
        previousWizardPage != WizardPage::None ||
        previousMode != Mode::Map;
    suppressUnsafeAddPreview(returnedToMap);
  }
  else
  {
    _unsafePreviewClean = false;
  }
}

bool RuntimeUIFBDMapV10::handleAddBackWithoutPreview()
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
  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);

  drawMapStatic();
  drawMapFull();
  noteMapFullRendered();

  if (_maxLevel <= 3)
  {
    drawAddPreview();
    _unsafePreviewClean = false;
  }
  else
  {
    _addPreviewDrawn = true;
    _unsafePreviewClean = true;
  }
  return true;
}

void RuntimeUIFBDMapV10::drawWizardTypeChoice(WizardType type,
                                               bool selected)
{
  const uint8_t index = static_cast<uint8_t>(type);
  drawMenuButton(JWPLC_Display.tft(),
                 TYPE_BUTTON_X,
                 static_cast<int16_t>(TYPE_BUTTON_Y +
                                      index * TYPE_BUTTON_STEP),
                 TYPE_BUTTON_W,
                 TYPE_BUTTON_H,
                 wizardTypeName(type),
                 selected);
}

bool RuntimeUIFBDMapV10::handleWizardTypeInputIncremental()
{
  if (_wizardPage != WizardPage::Type ||
      _wizardApplying ||
      _inputReleaseGate)
  {
    return false;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    restoreWizardRepeatProfile();
    closeWizardToAdd();
    JWPLC_Display.notifyActivity();
    return true;
  }

  // LEFT ya no funciona como retroceso en NUEVO BLOQUE. ESC es la única salida.
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    JWPLC_Display.notifyActivity();
    return true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    initializeWizardConfig();
    _wizardPage = WizardPage::Configure;
    _wizardError = false;
    JWPLC_Display.notifyActivity();
    gateInputUntilRelease(false);
    drawWizardConfigScreenV10();
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const uint8_t count = static_cast<uint8_t>(WizardType::Count);
  const WizardType previous = _wizardType;
  uint8_t index = static_cast<uint8_t>(_wizardType);
  if (up)
  {
    index = index == 0 ? static_cast<uint8_t>(count - 1U)
                       : static_cast<uint8_t>(index - 1U);
  }
  else
  {
    index = static_cast<uint8_t>((index + 1U) % count);
  }

  _wizardType = static_cast<WizardType>(index);
  _wizardError = false;
  JWPLC_Display.notifyActivity();

  // No se reconstruye la pantalla: solo cambian el botón anterior y el nuevo.
  drawWizardTypeChoice(previous, false);
  drawWizardTypeChoice(_wizardType, true);
  return true;
}

int16_t RuntimeUIFBDMapV10::wizardConfigFieldY(uint8_t field) const
{
  const uint8_t count = wizardFieldCount();
  const int16_t firstY = count >= 3 ? 38 : 42;
  return static_cast<int16_t>(firstY + field * CONFIG_FIELD_STEP);
}

int16_t RuntimeUIFBDMapV10::wizardContextY() const
{
  const uint8_t count = wizardFieldCount();
  if (count <= 1)
  {
    return 72;
  }
  if (count == 2)
  {
    return 98;
  }
  return 116;
}

int16_t RuntimeUIFBDMapV10::wizardContextHeight() const
{
  const int16_t y = wizardContextY();
  return static_cast<int16_t>(CONTEXT_BOTTOM - y);
}

void RuntimeUIFBDMapV10::drawWizardConfigScreenV10()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "CONFIGURAR");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);

  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);

  char typeLabel[28];
  std::snprintf(typeLabel,
                sizeof(typeLabel),
                "TIPO: %s",
                wizardTypeName(_wizardType));
  drawFieldLabel(tft,
                 10,
                 30,
                 typeLabel,
                 COLOR_ACCENT,
                 COLOR_PANEL);

  drawWizardConfigFieldsV10();
  drawWizardContextPanel();
  drawWizardFooter(_wizardError
                       ? "CONFIGURACION NO VALIDA"
                       : "OK CREAR   ESC ATRAS",
                   _wizardError ? COLOR_ERROR : COLOR_MUTED);
}

void RuntimeUIFBDMapV10::drawWizardConfigFieldV10(uint8_t field)
{
  if (field >= wizardFieldCount())
  {
    return;
  }

  char value[28];
  char label[48];
  formatWizardFieldValue(field, value, sizeof(value));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                wizardFieldName(field),
                value);

  drawMenuButton(JWPLC_Display.tft(),
                 CONFIG_FIELD_X,
                 wizardConfigFieldY(field),
                 CONFIG_FIELD_W,
                 CONFIG_FIELD_H,
                 label,
                 field == _wizardField);
}

void RuntimeUIFBDMapV10::drawWizardConfigFieldsV10()
{
  const uint8_t count = wizardFieldCount();
  for (uint8_t field = 0; field < count; ++field)
  {
    drawWizardConfigFieldV10(field);
  }
}

void RuntimeUIFBDMapV10::clearWizardErrorFooterIfNeeded()
{
  if (!_wizardError)
  {
    return;
  }

  _wizardError = false;
  drawWizardFooter("OK CREAR   ESC ATRAS", COLOR_MUTED);
}

void RuntimeUIFBDMapV10::configureWizardRepeatProfile()
{
  if (_wizardPage != WizardPage::Configure ||
      !wizardFieldIsTimeValue(_wizardField))
  {
    return;
  }

  if (_wizardRepeatActive &&
      _wizardRepeatUnitValid &&
      _wizardRepeatUnit == _wizardTimeUnit)
  {
    return;
  }

  int16_t step1 = 1;
  int16_t step2 = 1;
  int16_t step3 = 1;
  int16_t step4 = 1;

  switch (_wizardTimeUnit)
  {
  case WizardTimeUnit::Milliseconds:
    step2 = 10;
    step3 = 100;
    step4 = 1000;
    break;
  case WizardTimeUnit::Seconds:
    step2 = 2;
    step3 = 10;
    step4 = 30;
    break;
  case WizardTimeUnit::Minutes:
    step2 = 1;
    step3 = 2;
    step4 = 5;
    break;
  case WizardTimeUnit::Hours:
  default:
    step2 = 1;
    step3 = 1;
    step4 = 2;
    break;
  }

  JWPLC_Buttons.setRepeatInitialDelay(170);
  JWPLC_Buttons.setRepeatProfile(
      10, 25, 50,
      step1, step2, step3, step4,
      120, 90, 70, 50);
  JWPLC_Buttons.clearPendingRepeats();

  _wizardRepeatActive = true;
  _wizardRepeatUnitValid = true;
  _wizardRepeatUnit = _wizardTimeUnit;
}

void RuntimeUIFBDMapV10::restoreWizardRepeatProfile()
{
  if (!_wizardRepeatActive)
  {
    return;
  }

  JWPLC_Buttons.setRepeatInitialDelay(220);
  JWPLC_Buttons.setRepeatProfile(
      6, 12, 20,
      1, 1, 1, 1,
      120, 90, 70, 50);
  JWPLC_Buttons.clearPendingRepeats();

  _wizardRepeatActive = false;
  _wizardRepeatUnitValid = false;
}

bool RuntimeUIFBDMapV10::handleWizardConfigInputIncremental()
{
  if (_wizardPage != WizardPage::Configure ||
      _wizardApplying ||
      _inputReleaseGate)
  {
    return false;
  }

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

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    restoreWizardRepeatProfile();
    requestWizardCreate();
    return true;
  }

  const uint8_t fieldCount = wizardFieldCount();
  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool right = JWPLC_Buttons.pressed(BTN_RIGHT);
  if (left || right)
  {
    const uint8_t previous = _wizardField;
    _wizardField = left
                       ? (_wizardField == 0
                              ? static_cast<uint8_t>(fieldCount - 1U)
                              : static_cast<uint8_t>(_wizardField - 1U))
                       : static_cast<uint8_t>((_wizardField + 1U) % fieldCount);

    restoreWizardRepeatProfile();
    clearWizardErrorFooterIfNeeded();
    JWPLC_Display.notifyActivity();
    drawWizardConfigFieldV10(previous);
    drawWizardConfigFieldV10(_wizardField);
    drawWizardContextPanel();
    return true;
  }

  if (wizardFieldIsTimeValue(_wizardField))
  {
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
      clearWizardErrorFooterIfNeeded();
      JWPLC_Display.notifyActivity();
      drawWizardConfigFieldV10(_wizardField);
      return true;
    }
    return false;
  }

  restoreWizardRepeatProfile();
  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const bool forward = down;
  bool contextChanged = false;
  bool secondSource = false;
  if (wizardFieldIsSource(_wizardField, secondSource))
  {
    const bool allowOpen = _wizardType == WizardType::And2;
    uint16_t &source = secondSource ? _wizardSourceB : _wizardSourceA;
    moveWizardSource(source, forward, allowOpen);
    contextChanged = true;
  }
  else if (wizardFieldIsResource(_wizardField))
  {
    moveWizardResource(forward);
    contextChanged = true;
  }
  else if (wizardFieldIsTimeUnit(_wizardField))
  {
    moveWizardTimeUnit(forward);
    // Al cambiar unidad también cambia el valor convertido.
    drawWizardConfigFieldV10(1);
  }
  else
  {
    return false;
  }

  clearWizardErrorFooterIfNeeded();
  JWPLC_Display.notifyActivity();
  drawWizardConfigFieldV10(_wizardField);
  if (contextChanged)
  {
    drawWizardContextPanel();
  }
  return true;
}

bool RuntimeUIFBDMapV10::wizardContextSource(uint16_t &source) const
{
  if (_wizardPage != WizardPage::Configure ||
      wizardFieldIsResource(_wizardField) ||
      _wizardType == WizardType::DigitalInput)
  {
    return false;
  }

  bool secondSource = false;
  if (wizardFieldIsSource(_wizardField, secondSource))
  {
    source = secondSource ? _wizardSourceB : _wizardSourceA;
    return true;
  }

  switch (_wizardType)
  {
  case WizardType::Not:
  case WizardType::Ton:
  case WizardType::DigitalOutput:
  case WizardType::And2:
    source = _wizardSourceA;
    return true;
  default:
    return false;
  }
}

void RuntimeUIFBDMapV10::drawWizardContextPanel()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const int16_t y = wizardContextY();
  const int16_t height = wizardContextHeight();

  tft.fillRect(CONTEXT_X, y, CONTEXT_W, height, COLOR_PANEL);
  tft.drawRect(CONTEXT_X, y, CONTEXT_W, height, COLOR_BORDER);

  char info[48];
  uint16_t source = JWPLC_LOGIC_V2_SOURCE_OPEN;
  const bool hasSource = wizardContextSource(source);

  if (wizardFieldIsResource(_wizardField))
  {
    char resource[20];
    formatWizardFieldValue(_wizardField, resource, sizeof(resource));
    std::snprintf(info,
                  sizeof(info),
                  "%s - SIN POSICION FBD",
                  resource);
    drawFieldLabel(tft,
                   CONTEXT_X + 5,
                   y + 4,
                   info,
                   COLOR_MUTED,
                   COLOR_PANEL);
    return;
  }

  if (!hasSource)
  {
    drawFieldLabel(tft,
                   CONTEXT_X + 5,
                   y + 4,
                   "SIN CONTEXTO FBD",
                   COLOR_MUTED,
                   COLOR_PANEL);
    return;
  }

  if (source >= _model->blockCount())
  {
    char sourceText[20];
    formatWizardSource(source, sourceText, sizeof(sourceText));
    std::snprintf(info,
                  sizeof(info),
                  "%s - SIN POSICION FBD",
                  sourceText);
    drawFieldLabel(tft,
                   CONTEXT_X + 5,
                   y + 4,
                   info,
                   COLOR_MUTED,
                   COLOR_PANEL);
    return;
  }

  if (!_layoutValid)
  {
    buildLayout();
  }

  const LogicV2BlockRecord *definition = _model->block(source);
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
                 CONTEXT_X + 5,
                 y + 4,
                 info,
                 COLOR_WARNING,
                 COLOR_PANEL);

  if (height > 20)
  {
    drawMiniFBD(source,
                CONTEXT_X + 4,
                y + 14,
                CONTEXT_W - 8,
                height - 18);
  }
}

void RuntimeUIFBDMapV10::drawMiniFBD(uint16_t selectedSource,
                                     int16_t x,
                                     int16_t y,
                                     int16_t width,
                                     int16_t height)
{
  const uint16_t count = _model->blockCount();
  if (count == 0 || selectedSource >= count || width < 20 || height < 7)
  {
    return;
  }

  uint8_t maxLane = 0;
  for (uint16_t block = 0; block < count; ++block)
  {
    if (_lanes[block] > maxLane)
    {
      maxLane = _lanes[block];
    }
  }

  uint8_t visibleColumns = static_cast<uint8_t>(width / 34);
  if (visibleColumns < 1)
    visibleColumns = 1;
  if (visibleColumns > 6)
    visibleColumns = 6;

  uint8_t visibleRows = static_cast<uint8_t>(height / 8);
  if (visibleRows < 1)
    visibleRows = 1;
  if (visibleRows > 4)
    visibleRows = 4;

  const uint8_t totalColumns = static_cast<uint8_t>(_maxLevel + 1U);
  const uint8_t totalRows = static_cast<uint8_t>(maxLane + 1U);
  if (visibleColumns > totalColumns)
    visibleColumns = totalColumns;
  if (visibleRows > totalRows)
    visibleRows = totalRows;

  uint8_t firstColumn = _levels[selectedSource] > visibleColumns / 2U
                            ? static_cast<uint8_t>(
                                  _levels[selectedSource] - visibleColumns / 2U)
                            : 0;
  if (static_cast<uint16_t>(firstColumn) + visibleColumns > totalColumns)
  {
    firstColumn = static_cast<uint8_t>(totalColumns - visibleColumns);
  }

  uint8_t firstRow = _lanes[selectedSource] > visibleRows / 2U
                         ? static_cast<uint8_t>(
                               _lanes[selectedSource] - visibleRows / 2U)
                         : 0;
  if (static_cast<uint16_t>(firstRow) + visibleRows > totalRows)
  {
    firstRow = static_cast<uint8_t>(totalRows - visibleRows);
  }

  const int16_t cellW = static_cast<int16_t>(width / visibleColumns);
  const int16_t cellH = static_cast<int16_t>(height / visibleRows);
  int16_t miniW = static_cast<int16_t>(cellW - 5);
  int16_t miniH = static_cast<int16_t>(cellH - 3);
  if (miniW > 18)
    miniW = 18;
  if (miniH > 8)
    miniH = 8;
  if (miniW < 5)
    miniW = 5;
  if (miniH < 4)
    miniH = 4;

  const auto blockX = [&](uint16_t block) -> int16_t
  {
    return static_cast<int16_t>(
        x + (_levels[block] - firstColumn) * cellW +
        (cellW - miniW) / 2);
  };
  const auto blockY = [&](uint16_t block) -> int16_t
  {
    return static_cast<int16_t>(
        y + (_lanes[block] - firstRow) * cellH +
        (cellH - miniH) / 2);
  };
  const auto visible = [&](uint16_t block) -> bool
  {
    return _levels[block] >= firstColumn &&
           _levels[block] < static_cast<uint8_t>(firstColumn + visibleColumns) &&
           _lanes[block] >= firstRow &&
           _lanes[block] < static_cast<uint8_t>(firstRow + visibleRows);
  };

  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  // Enlaces primero; los mini bloques se reponen encima.
  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    if (!visible(consumer))
    {
      continue;
    }

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
      if (source >= count || !visible(source))
      {
        continue;
      }

      tft.drawLine(static_cast<int16_t>(blockX(source) + miniW),
                   static_cast<int16_t>(blockY(source) + miniH / 2),
                   blockX(consumer),
                   static_cast<int16_t>(blockY(consumer) + miniH / 2),
                   COLOR_MUTED);
    }
  }

  for (uint16_t block = 0; block < count; ++block)
  {
    if (!visible(block))
    {
      continue;
    }

    const bool selected = block == selectedSource;
    const bool active = _model->blockValue(block);
    const uint16_t border = selected
                                ? COLOR_WARNING
                                : (active ? COLOR_OK : COLOR_BORDER);
    const int16_t bx = blockX(block);
    const int16_t by = blockY(block);

    tft.fillRect(bx, by, miniW, miniH, COLOR_PANEL);
    tft.drawRect(bx, by, miniW, miniH, border);
    if (selected && miniW > 6 && miniH > 5)
    {
      tft.drawRect(bx + 1, by + 1, miniW - 2, miniH - 2, border);
    }
  }
}

void RuntimeUIFBDMapV10::handleWizardApplyCompletedV10()
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
    drawWizardConfigScreenV10();
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

  if (_maxLevel <= 3)
  {
    drawAddPreview();
    _unsafePreviewClean = false;
  }
  else
  {
    _addPreviewDrawn = true;
    _unsafePreviewClean = true;
  }
}
