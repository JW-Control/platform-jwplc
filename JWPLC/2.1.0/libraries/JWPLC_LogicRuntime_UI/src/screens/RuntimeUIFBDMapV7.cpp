#include "RuntimeUIFBDMapV7.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapV7::RuntimeUIFBDMapV7()
    : RuntimeUIFBDMapV6(),
      _fastRepeatActive(false),
      _fastRepeatUnitValid(false),
      _fastRepeatUnit(TimeUnit::Milliseconds),
      _mapSelectionCacheValid(false),
      _mapSelectionCache(0),
      _detailCacheValid(false),
      _detailCacheBlock(0),
      _detailCachePage(0),
      _detailCacheBlockValue(false),
      _detailCacheInputs{},
      _detailCacheTonTiming(false),
      _detailCacheTonActive(false),
      _detailCacheTonElapsed{},
      _lastEditorElapsedRefreshMs(0),
      _editorElapsedCacheValid(false),
      _editorElapsedColorOn(false),
      _editorElapsedCache{}
{
}

void RuntimeUIFBDMapV7::resetOptimizedState()
{
  _mapSelectionCacheValid = false;
  _mapSelectionCache = 0;

  _detailCacheValid = false;
  _detailCacheBlock = 0;
  _detailCachePage = 0;
  _detailCacheBlockValue = false;
  std::memset(_detailCacheInputs, 0, sizeof(_detailCacheInputs));
  _detailCacheTonTiming = false;
  _detailCacheTonActive = false;
  _detailCacheTonElapsed[0] = '\0';

  _lastEditorElapsedRefreshMs = 0;
  _editorElapsedCacheValid = false;
  _editorElapsedColorOn = false;
  _editorElapsedCache[0] = '\0';
}

void RuntimeUIFBDMapV7::attach(RuntimeUIV2ReadModel &model)
{
  restoreDefaultRepeatProfile();
  resetOptimizedState();
  RuntimeUIFBDMapV6::attach(model);
}

void RuntimeUIFBDMapV7::detach()
{
  restoreDefaultRepeatProfile();
  resetOptimizedState();
  RuntimeUIFBDMapV6::detach();
}

void RuntimeUIFBDMapV7::enter()
{
  restoreDefaultRepeatProfile();
  resetOptimizedState();
  RuntimeUIFBDMapV6::enter();
  noteMapFullRendered();
}

void RuntimeUIFBDMapV7::exit()
{
  restoreDefaultRepeatProfile();
  resetOptimizedState();
  RuntimeUIFBDMapV6::exit();
}

void RuntimeUIFBDMapV7::forceRedraw()
{
  restoreDefaultRepeatProfile();
  resetOptimizedState();
  RuntimeUIFBDMapV6::forceRedraw();
}

void RuntimeUIFBDMapV7::configureFastRepeatProfile()
{
  if (!_parameterEditorActive ||
      _parameterEditFocus != ParameterEditFocus::Value)
  {
    return;
  }

  if (_fastRepeatActive &&
      _fastRepeatUnitValid &&
      _fastRepeatUnit == _parameterUnit)
  {
    return;
  }

  int16_t step1 = 1;
  int16_t step2 = 1;
  int16_t step3 = 1;
  int16_t step4 = 1;

  switch (_parameterUnit)
  {
  case TimeUnit::Milliseconds:
    step2 = 10;
    step3 = 100;
    step4 = 1000;
    break;

  case TimeUnit::Seconds:
    step2 = 2;
    step3 = 10;
    step4 = 30;
    break;

  case TimeUnit::Minutes:
    step2 = 1;
    step3 = 2;
    step4 = 5;
    break;

  case TimeUnit::Hours:
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

  _fastRepeatActive = true;
  _fastRepeatUnitValid = true;
  _fastRepeatUnit = _parameterUnit;
}

void RuntimeUIFBDMapV7::restoreDefaultRepeatProfile()
{
  if (!_fastRepeatActive)
  {
    return;
  }

  // Perfil normal configurado por JWPLC_GlobalPeripherals para navegación.
  JWPLC_Buttons.setRepeatInitialDelay(220);
  JWPLC_Buttons.setRepeatProfile(
      6, 12, 20,
      1, 1, 1, 1,
      120, 90, 70, 50);
  JWPLC_Buttons.clearPendingRepeats();

  _fastRepeatActive = false;
  _fastRepeatUnitValid = false;
}

void RuntimeUIFBDMapV7::refresh(const JWPLC_IOState *io,
                                const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_model == nullptr || !_model->isAttached())
  {
    return;
  }

  if (_parameterEditorActive)
  {
    refreshParameterEditorOptimized();
    return;
  }

  restoreDefaultRepeatProfile();

  if (_mode == Mode::Map)
  {
    refreshMapOptimized();
    return;
  }

  if (_mode == Mode::Detail)
  {
    refreshDetailOptimized();
    return;
  }

  // EDITAR IN mantiene el flujo transaccional probado de v0.5.1.
  RuntimeUIFBDMapV5::refresh(io, rtc);
}

void RuntimeUIFBDMapV7::noteMapFullRendered()
{
  _mapSelectionCache = _selectedIndex;
  _mapSelectionCacheValid = true;
  _detailCacheValid = false;
}

void RuntimeUIFBDMapV7::refreshMapOptimized()
{
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
    drawMapStatic();
    drawMapFull();
    noteMapFullRendered();
    _fullRedraw = false;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  handleMapInputOptimized();
  if (_mode != Mode::Map)
  {
    return;
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastValueRefreshMs) < VALUE_REFRESH_MS)
  {
    return;
  }
  _lastValueRefreshMs = nowMs;

  drawMapLiveOptimized();
}

void RuntimeUIFBDMapV7::handleMapInputOptimized()
{
  const uint16_t previousSelection = _selectedIndex;
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
      noteMapFullRendered();
    }
    else
    {
      _mapSelectionCache = previousSelection;
      _mapSelectionCacheValid = true;
      drawMapHeaderInfo();
      drawMapLiveOptimized();
    }
  }

  if (JWPLC_Buttons.pressed(BTN_OK) &&
      _model->blockCount() > 0)
  {
    JWPLC_Display.notifyActivity();
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    _mode = Mode::Detail;
    _detailInputIndex = 0;
    _detailFocus = DetailFocus::Inputs;
    _detailParameterIndex = 0;
    normalizeDetailFocus();
    _lastDetailRefreshMs = millis();
    gateInputUntilRelease(false);
    drawDetailStatic();
    drawDetailV5(true);
    captureDetailCache();
  }
}

void RuntimeUIFBDMapV7::drawMapLiveOptimized()
{
  const uint16_t count = _model->blockCount();
  if (count == 0)
  {
    return;
  }

  if (!_valueCacheValid)
  {
    drawMapFull();
    noteMapFullRendered();
    return;
  }

  bool valueChanged[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS] = {};
  bool redrawNode[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS] = {};
  bool redrawHint[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS] = {};
  bool anyChange = false;

  for (uint16_t block = 0; block < count; ++block)
  {
    const bool current = _model->blockValue(block);
    if (_valueCache[block] != current)
    {
      valueChanged[block] = true;
      redrawNode[block] = true;
      redrawHint[block] = true;
      anyChange = true;
    }
  }

  if (!_mapSelectionCacheValid ||
      _mapSelectionCache != _selectedIndex)
  {
    if (_mapSelectionCacheValid && _mapSelectionCache < count)
    {
      redrawNode[_mapSelectionCache] = true;
      redrawHint[_mapSelectionCache] = true;
    }
    redrawNode[_selectedIndex] = true;
    redrawHint[_selectedIndex] = true;
    anyChange = true;
  }

  if (!anyChange)
  {
    return;
  }

  // Primero recolorea únicamente los enlaces cuyo bloque fuente cambió.
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

  // Después repone solo los nodos afectados, manteniéndolos sobre los cables.
  for (uint16_t block = 0; block < count; ++block)
  {
    if (redrawNode[block])
    {
      drawNode(block);
    }
  }

  const auto range = visibleGridRange();
  for (uint16_t block = 0; block < count; ++block)
  {
    if (!redrawHint[block])
    {
      continue;
    }

    bool leftSide = false;
    if (shouldDrawEdgeHint(block, range, leftSide))
    {
      const bool active = _model->blockValue(block);
      const bool selected = block == _selectedIndex;
      const uint16_t border = selected
                                  ? COLOR_WARNING
                                  : (active ? COLOR_OK : COLOR_BORDER);
      drawEdgeHint(block,
                   leftSide,
                   screenY(block),
                   border,
                   active);
    }
  }

  cacheValues();
  _mapSelectionCache = _selectedIndex;
  _mapSelectionCacheValid = true;
}

void RuntimeUIFBDMapV7::refreshDetailOptimized()
{
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
    captureDetailCache();
    _fullRedraw = false;
  }

  updateHeaderStateIfNeeded(false);

  if (consumeInputReleaseGate())
  {
    return;
  }

  handleDetailInputOptimized();
  if (_mode != Mode::Detail || _parameterEditorActive)
  {
    return;
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastDetailRefreshMs) < DETAIL_REFRESH_MS)
  {
    return;
  }
  _lastDetailRefreshMs = nowMs;

  drawDetailLiveOptimized();
}

void RuntimeUIFBDMapV7::handleDetailInputOptimized()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const DetailFocus previousFocus = _detailFocus;
  const uint8_t previousInput = _detailInputIndex;
  const uint8_t previousPage = detailPageStart();
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
                              ? static_cast<uint8_t>(definition->inputCount - 1U)
                              : static_cast<uint8_t>(_detailInputIndex - 1U);
      changed = true;
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
      _editorElapsedCacheValid = false;
      _lastEditorElapsedRefreshMs = 0;
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
    noteMapFullRendered();
    return;
  }

  if (!changed)
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  const uint8_t currentPage = detailPageStart();
  if (currentPage != previousPage)
  {
    drawDetailV5(true);
    captureDetailCache();
    return;
  }

  if (previousFocus == DetailFocus::Inputs &&
      previousInput < definition->inputCount)
  {
    drawDetailSource(
        previousInput,
        static_cast<uint8_t>(previousInput - previousPage),
        _detailFocus == DetailFocus::Inputs &&
            previousInput == _detailInputIndex);
  }

  if (_detailFocus == DetailFocus::Inputs &&
      _detailInputIndex < definition->inputCount &&
      (previousFocus != DetailFocus::Inputs ||
       previousInput != _detailInputIndex))
  {
    drawDetailSource(
        _detailInputIndex,
        static_cast<uint8_t>(_detailInputIndex - currentPage),
        true);
  }

  if (definition->type == LogicV2BlockType::Ton &&
      previousFocus != _detailFocus)
  {
    drawTonParameterPanel(_detailFocus == DetailFocus::Parameters);
  }

  updateDetailHeader();
  captureDetailCache();
}

void RuntimeUIFBDMapV7::captureDetailCache()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    _detailCacheValid = false;
    return;
  }

  _detailCacheBlock = _selectedIndex;
  _detailCachePage = detailPageStart();
  _detailCacheBlockValue = _model->blockValue(_selectedIndex);

  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t input = static_cast<uint8_t>(_detailCachePage + row);
    _detailCacheInputs[row] = input < definition->inputCount
                                  ? _model->inputValue(_selectedIndex, input)
                                  : false;
  }

  if (definition->type == LogicV2BlockType::Ton)
  {
    _detailCacheTonTiming = _model->tonTiming(_selectedIndex);
    _detailCacheTonActive = _model->blockValue(_selectedIndex);
    formatDurationCompact(
        _model->tonElapsedMs(_selectedIndex, millis()),
        _detailCacheTonElapsed,
        sizeof(_detailCacheTonElapsed));
  }
  else
  {
    _detailCacheTonTiming = false;
    _detailCacheTonActive = false;
    _detailCacheTonElapsed[0] = '\0';
  }

  _detailCacheValid = true;
}

void RuntimeUIFBDMapV7::drawDetailLiveOptimized()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const uint8_t page = detailPageStart();
  if (!_detailCacheValid ||
      _detailCacheBlock != _selectedIndex ||
      _detailCachePage != page)
  {
    drawDetailV5(true);
    captureDetailCache();
    return;
  }

  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t input = static_cast<uint8_t>(page + row);
    if (input >= definition->inputCount)
    {
      continue;
    }

    const bool current = _model->inputValue(_selectedIndex, input);
    if (_detailCacheInputs[row] == current)
    {
      continue;
    }

    drawDetailSource(
        input,
        row,
        _detailFocus == DetailFocus::Inputs &&
            input == _detailInputIndex);
    drawDetailWireLive(input, row);
    drawDetailInputPinLive(input, row);
    _detailCacheInputs[row] = current;
  }

  const bool blockValue = _model->blockValue(_selectedIndex);
  if (_detailCacheBlockValue != blockValue)
  {
    drawDetailBlockStateLive();
    _detailCacheBlockValue = blockValue;
  }

  if (definition->type == LogicV2BlockType::Ton)
  {
    drawTonDetailElapsedLive(false);
  }
}

void RuntimeUIFBDMapV7::drawDetailWireLive(uint8_t inputIndex,
                                           uint8_t visibleRow)
{
  const bool active = _model->inputValue(_selectedIndex, inputIndex);
  const uint16_t color = active ? COLOR_OK : COLOR_MUTED;
  const int16_t sourceY = static_cast<int16_t>(
      34 + visibleRow * DETAIL_SOURCE_STEP + DETAIL_SOURCE_H / 2);
  const int16_t destinationY = static_cast<int16_t>(
      DETAIL_BLOCK_Y + 17 + visibleRow * 18);
  const int16_t sourceX = static_cast<int16_t>(
      DETAIL_SOURCE_X + DETAIL_SOURCE_W);
  const int16_t destinationX = DETAIL_BLOCK_X;
  const int16_t routeX = static_cast<int16_t>(
      sourceX + 46 + visibleRow * 7);

  drawOrthogonalWireClipped(sourceX,
                            sourceY,
                            destinationX,
                            destinationY,
                            routeX,
                            color);
}

void RuntimeUIFBDMapV7::drawDetailInputPinLive(uint8_t inputIndex,
                                               uint8_t visibleRow)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  const LogicV2InputLink *input = _model->inputLink(_selectedIndex, inputIndex);
  if (definition == nullptr || input == nullptr)
  {
    return;
  }

  const int16_t pinY = static_cast<int16_t>(
      DETAIL_BLOCK_Y + 17 + visibleRow * 18);
  const bool active = _model->inputValue(_selectedIndex, inputIndex);
  const uint16_t pinColor = active ? COLOR_OK : COLOR_MUTED;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

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

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(pinColor, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + 7, pinY - 3);
  tft.print(pinLabel);
}

void RuntimeUIFBDMapV7::drawDetailBlockStateLive()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const bool active = _model->blockValue(_selectedIndex);
  const uint16_t border = active ? COLOR_OK : COLOR_BORDER;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  tft.drawRect(DETAIL_BLOCK_X,
               DETAIL_BLOCK_Y,
               DETAIL_BLOCK_W,
               DETAIL_BLOCK_H,
               border);

  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT,
                   COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 19,
                DETAIL_BLOCK_Y + 32);
  tft.print(detailSymbol(definition->type));

  if (definition->type == LogicV2BlockType::DigitalInput ||
      definition->type == LogicV2BlockType::DigitalOutput)
  {
    char resource[10];
    formatMapLine2(resource, sizeof(resource), _selectedIndex);
    tft.setTextSize(1);
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED,
                     COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 14,
                  DETAIL_BLOCK_Y + 67);
    tft.print(resource);
  }

  tft.fillCircle(DETAIL_BLOCK_X + DETAIL_BLOCK_W,
                 DETAIL_BLOCK_Y + DETAIL_BLOCK_H / 2,
                 2,
                 active ? COLOR_OK : COLOR_MUTED);
}

void RuntimeUIFBDMapV7::drawTonDetailElapsedLive(bool force)
{
  if (!_model->isTon(_selectedIndex))
  {
    return;
  }

  char elapsed[18];
  formatDurationCompact(
      _model->tonElapsedMs(_selectedIndex, millis()),
      elapsed,
      sizeof(elapsed));

  const bool timing = _model->tonTiming(_selectedIndex);
  const bool active = _model->blockValue(_selectedIndex);
  const bool colorOn = timing || active;

  if (!force &&
      _detailCacheValid &&
      _detailCacheTonTiming == timing &&
      _detailCacheTonActive == active &&
      std::strcmp(_detailCacheTonElapsed, elapsed) == 0)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(TON_PANEL_X + 3,
               TON_PANEL_Y + 16,
               TON_PANEL_W - 6,
               13,
               COLOR_BACKGROUND);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(colorOn ? COLOR_OK : COLOR_MUTED,
                   COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X + 4, TON_PANEL_Y + 19);
  tft.print("Ta ");
  tft.print(elapsed);

  _detailCacheTonTiming = timing;
  _detailCacheTonActive = active;
  std::snprintf(_detailCacheTonElapsed,
                sizeof(_detailCacheTonElapsed),
                "%s",
                elapsed);
}

void RuntimeUIFBDMapV7::refreshParameterEditorOptimized()
{
  configureFastRepeatProfile();

  if (_parameterEditFocus == ParameterEditFocus::Value)
  {
    applyParameterValueAxisForExtension();
  }
  else
  {
    JWPLC_Buttons.clearPendingRepeats();
  }

  refreshParameterEditor();

  if (!_parameterEditorActive)
  {
    restoreDefaultRepeatProfile();
    _editorElapsedCacheValid = false;
    _detailCacheValid = false;
    return;
  }

  if (_parameterEditFocus == ParameterEditFocus::Value)
  {
    configureFastRepeatProfile();
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastEditorElapsedRefreshMs) <
      EDITOR_TA_REFRESH_MS)
  {
    return;
  }

  _lastEditorElapsedRefreshMs = nowMs;
  refreshEditorElapsedLive(false);
}

void RuntimeUIFBDMapV7::refreshEditorElapsedLive(bool force)
{
  if (!_parameterEditorActive ||
      !_model->isTon(_selectedIndex))
  {
    return;
  }

  char elapsed[18];
  formatDurationCompact(
      _model->tonElapsedMs(_selectedIndex, millis()),
      elapsed,
      sizeof(elapsed));

  const bool timing = _model->tonTiming(_selectedIndex);
  const bool active = _model->blockValue(_selectedIndex);
  const bool colorOn = timing || active;

  if (!force &&
      _editorElapsedCacheValid &&
      _editorElapsedColorOn == colorOn &&
      std::strcmp(_editorElapsedCache, elapsed) == 0)
  {
    return;
  }

  updateTextField(JWPLC_Display.tft(),
                  88,
                  83,
                  18,
                  elapsed,
                  colorOn ? COLOR_OK : COLOR_MUTED,
                  COLOR_PANEL);

  _editorElapsedCacheValid = true;
  _editorElapsedColorOn = colorOn;
  std::snprintf(_editorElapsedCache,
                sizeof(_editorElapsedCache),
                "%s",
                elapsed);
}
