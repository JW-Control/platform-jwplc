#include "RuntimeUIFBDMapUnified.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

bool RuntimeUIFBDMapUnified::selectedBlockHasParameter() const
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  return definition != nullptr && definition->type == LogicV2BlockType::Ton;
}

void RuntimeUIFBDMapUnified::normalizeDetailFocus()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr)
  {
    _detailFocus = DetailFocus::Inputs;
    _detailInputIndex = 0;
    return;
  }

  if (definition->inputCount == 0)
  {
    _detailInputIndex = 0;
    _detailFocus = selectedBlockHasParameter()
                       ? DetailFocus::Parameters
                       : DetailFocus::Inputs;
  }
  else if (_detailInputIndex >= definition->inputCount)
  {
    _detailInputIndex =
        static_cast<uint8_t>(definition->inputCount - 1U);
  }

  if (_detailFocus == DetailFocus::Parameters &&
      !selectedBlockHasParameter())
  {
    _detailFocus = DetailFocus::Inputs;
  }
}

uint8_t RuntimeUIFBDMapUnified::detailPageStart() const
{
  return static_cast<uint8_t>(
      (_detailInputIndex / DETAIL_INPUTS_PER_PAGE) *
      DETAIL_INPUTS_PER_PAGE);
}

void RuntimeUIFBDMapUnified::handleDetailInput()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
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
        selectedBlockHasParameter())
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
      _detailInputIndex =
          _detailInputIndex == 0
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
    // Preview MAPA/DETALLE: los editores se activarán tras migrarlos a Unified.
    JWPLC_Display.notifyActivity();
    return;
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    transitionTo(View::Map);
    return;
  }

  if (!changed)
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  normalizeDetailFocus();
  _headerDirty = true;

  const uint8_t currentPage = detailPageStart();
  if (currentPage != previousPage)
  {
    _contentDirty = true;
    invalidateDetailCache();
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
    drawTonPanel(true);
  }

  captureDetailCache();
}

void RuntimeUIFBDMapUnified::renderDetail(bool force)
{
  if (_model == nullptr)
  {
    return;
  }

  if (force)
  {
    clearContentArea();
    drawDetailFull();
    return;
  }

  drawDetailLive();
}

void RuntimeUIFBDMapUnified::drawDetailFull()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  normalizeDetailFocus();
  drawDetailWires();

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    if (inputIndex < definition->inputCount)
    {
      drawDetailSource(
          inputIndex,
          row,
          _detailFocus == DetailFocus::Inputs &&
              inputIndex == _detailInputIndex);
    }
  }

  drawDetailBlock();
  if (definition->type == LogicV2BlockType::Ton)
  {
    drawTonPanel(true);
  }

  captureDetailCache();
}

void RuntimeUIFBDMapUnified::drawDetailLive()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  if (!_detailCacheValid ||
      _detailCacheBlock != _selectedIndex ||
      _detailCacheInput != _detailInputIndex ||
      _detailCacheFocus != _detailFocus)
  {
    clearContentArea();
    drawDetailFull();
    return;
  }

  bool inputChanged = false;
  for (uint8_t input = 0; input < definition->inputCount; ++input)
  {
    const bool value = _model->inputValue(_selectedIndex, input);
    if (_detailInputValueCache[input] != value)
    {
      inputChanged = true;
      break;
    }
  }

  const bool blockValue = _model->blockValue(_selectedIndex);
  const bool blockChanged = blockValue != _detailCacheBlockValue;

  if (inputChanged)
  {
    drawDetailWires();
    const uint8_t pageStart = detailPageStart();
    for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
    {
      const uint8_t inputIndex =
          static_cast<uint8_t>(pageStart + row);
      if (inputIndex < definition->inputCount)
      {
        drawDetailSource(
            inputIndex,
            row,
            _detailFocus == DetailFocus::Inputs &&
                inputIndex == _detailInputIndex);
      }
    }
  }

  if (blockChanged)
  {
    drawDetailBlock();
  }

  if (definition->type == LogicV2BlockType::Ton)
  {
    const uint32_t nowMs = millis();
    if (blockChanged ||
        static_cast<uint32_t>(nowMs - _lastDetailLiveMs) >=
            DETAIL_LIVE_REFRESH_MS)
    {
      _lastDetailLiveMs = nowMs;
      drawTonPanel(blockChanged);
    }
  }

  if (inputChanged || blockChanged)
  {
    captureDetailCache();
  }
}

void RuntimeUIFBDMapUnified::drawDetailWires()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    if (inputIndex >= definition->inputCount)
    {
      continue;
    }

    const bool active = _model->inputValue(_selectedIndex, inputIndex);
    const uint16_t color = active ? COLOR_OK : COLOR_MUTED;
    const int16_t sourceY = static_cast<int16_t>(
        34 + row * DETAIL_SOURCE_STEP + DETAIL_SOURCE_H / 2);
    const int16_t destinationY = static_cast<int16_t>(
        DETAIL_BLOCK_Y + 17 + row * 18);
    const int16_t sourceX =
        static_cast<int16_t>(DETAIL_SOURCE_X + DETAIL_SOURCE_W);
    const int16_t routeX =
        static_cast<int16_t>(sourceX + 46 + row * 7);

    drawOrthogonalWireClipped(
        sourceX,
        sourceY,
        DETAIL_BLOCK_X,
        destinationY,
        routeX,
        color);
  }
}

void RuntimeUIFBDMapUnified::drawDetailSource(
    uint8_t inputIndex,
    uint8_t visibleRow,
    bool selected)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  const LogicV2InputLink *input =
      _model->inputLink(_selectedIndex, inputIndex);
  if (definition == nullptr || input == nullptr)
  {
    return;
  }

  const int16_t y =
      static_cast<int16_t>(34 + visibleRow * DETAIL_SOURCE_STEP);
  const bool active = _model->inputValue(_selectedIndex, inputIndex);
  const uint16_t border =
      selected ? COLOR_WARNING : (active ? COLOR_OK : COLOR_BORDER);

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(
      DETAIL_SOURCE_X, y, DETAIL_SOURCE_W, DETAIL_SOURCE_H, COLOR_BACKGROUND);
  tft.drawRect(
      DETAIL_SOURCE_X, y, DETAIL_SOURCE_W, DETAIL_SOURCE_H, border);
  if (selected)
  {
    tft.drawRect(DETAIL_SOURCE_X + 1,
                 y + 1,
                 DETAIL_SOURCE_W - 2,
                 DETAIL_SOURCE_H - 2,
                 border);
  }

  char sourceId[12];
  char sourceType[12];
  const uint16_t source = input->source();

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(sourceId, sizeof(sourceId), "X");
    std::snprintf(sourceType, sizeof(sourceType), "ABIERTO");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(sourceId, sizeof(sourceId), "HI");
    std::snprintf(sourceType, sizeof(sourceType), "CONST 1");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(sourceId, sizeof(sourceId), "LO");
    std::snprintf(sourceType, sizeof(sourceType), "CONST 0");
  }
  else
  {
    formatBlockId(sourceId, sizeof(sourceId), source);
    formatMapLine2(sourceType, sizeof(sourceType), source);
  }

  const char *role = _model->inputRole(definition->type, inputIndex);
  char roleText[8];
  if (role != nullptr && role[0] != '\0')
  {
    std::snprintf(roleText, sizeof(roleText), "%s", role);
  }
  else
  {
    std::snprintf(roleText,
                  sizeof(roleText),
                  "%u",
                  static_cast<unsigned>(inputIndex + 1U));
  }

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_SOURCE_X + 4, y + 3);
  tft.print(roleText);
  tft.setCursor(DETAIL_SOURCE_X + 20, y + 3);
  tft.print(sourceId);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_SOURCE_X + 20, y + 13);
  tft.print(sourceType);
}

void RuntimeUIFBDMapUnified::drawDetailBlock()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  const bool active = _model->blockValue(_selectedIndex);
  const uint16_t border = active ? COLOR_OK : COLOR_BORDER;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  tft.fillRect(DETAIL_BLOCK_X,
               DETAIL_BLOCK_Y,
               DETAIL_BLOCK_W,
               DETAIL_BLOCK_H,
               COLOR_BACKGROUND);
  tft.drawRect(DETAIL_BLOCK_X,
               DETAIL_BLOCK_Y,
               DETAIL_BLOCK_W,
               DETAIL_BLOCK_H,
               border);

  if (definition->inputCount > 0)
  {
    tft.drawFastVLine(DETAIL_BLOCK_X + DETAIL_GUTTER_W,
                      DETAIL_BLOCK_Y + 2,
                      DETAIL_BLOCK_H - 4,
                      COLOR_BORDER);
  }

  char blockId[8];
  formatBlockId(blockId, sizeof(blockId), _selectedIndex);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 8,
                DETAIL_BLOCK_Y + 7);
  tft.print(blockId);

  tft.setTextSize(2);
  tft.setTextColor(active ? COLOR_OK : COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 19,
                DETAIL_BLOCK_Y + 32);
  tft.print(detailSymbol(definition->type));

  if (definition->type == LogicV2BlockType::DigitalInput ||
      definition->type == LogicV2BlockType::DigitalOutput)
  {
    char resource[10];
    formatMapLine2(resource, sizeof(resource), _selectedIndex);
    tft.setTextSize(1);
    tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + DETAIL_GUTTER_W + 14,
                  DETAIL_BLOCK_Y + 67);
    tft.print(resource);
  }

  const uint8_t pageStart = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t inputIndex = static_cast<uint8_t>(pageStart + row);
    if (inputIndex >= definition->inputCount)
    {
      continue;
    }

    const LogicV2InputLink *input =
        _model->inputLink(_selectedIndex, inputIndex);
    if (input == nullptr)
    {
      continue;
    }

    const int16_t pinY = static_cast<int16_t>(
        DETAIL_BLOCK_Y + 17 + row * 18);
    const bool inputActive =
        _model->inputValue(_selectedIndex, inputIndex);
    const uint16_t pinColor = inputActive ? COLOR_OK : COLOR_MUTED;

    if (input->inverted())
    {
      tft.fillCircle(DETAIL_BLOCK_X + 1,
                     pinY,
                     4,
                     COLOR_BACKGROUND);
      tft.drawCircle(DETAIL_BLOCK_X + 1,
                     pinY,
                     3,
                     COLOR_MUTED);
    }
    else
    {
      tft.fillCircle(DETAIL_BLOCK_X, pinY, 1, pinColor);
    }

    const char *role = _model->inputRole(definition->type, inputIndex);
    char pinLabel[6];
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

    tft.setTextSize(1);
    tft.setTextColor(pinColor, COLOR_BACKGROUND);
    tft.setCursor(DETAIL_BLOCK_X + 7, pinY - 3);
    tft.print(pinLabel);
  }

  tft.fillCircle(
      DETAIL_BLOCK_X + DETAIL_BLOCK_W,
      DETAIL_BLOCK_Y + DETAIL_BLOCK_H / 2,
      2,
      active ? COLOR_OK : COLOR_MUTED);
}

void RuntimeUIFBDMapUnified::drawTonPanelFrame(bool selected)
{
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
                 TON_PANEL_H,
                 COLOR_WARNING);
    tft.drawRect(TON_PANEL_X + 1,
                 TON_PANEL_Y + 1,
                 TON_PANEL_W - 2,
                 TON_PANEL_H - 2,
                 COLOR_WARNING);
  }
}

void RuntimeUIFBDMapUnified::drawTonTextLine(
    int16_t y,
    const char *prefix,
    const char *value,
    uint16_t color)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(TON_PANEL_X + 3,
               y,
               TON_PANEL_W - 6,
               8,
               COLOR_BACKGROUND);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(color, COLOR_BACKGROUND);
  tft.setCursor(TON_PANEL_X + 4, y);
  tft.print(prefix);
  tft.print(value);
}

void RuntimeUIFBDMapUnified::drawTonPanel(bool force)
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr || definition->type != LogicV2BlockType::Ton)
  {
    return;
  }

  const TonBase base = tonBaseFromResource(definition->resource);
  char configured[16];
  char elapsed[16];
  formatMillisecondsInBase(
      definition->parameter, base, configured, sizeof(configured));
  formatMillisecondsInBase(
      _model->tonElapsedMs(_selectedIndex, millis()),
      base,
      elapsed,
      sizeof(elapsed));

  const bool colorOn =
      _model->tonTiming(_selectedIndex) ||
      _model->blockValue(_selectedIndex);
  const bool selected = _detailFocus == DetailFocus::Parameters;

  const bool frameChanged =
      force || !_detailCacheValid ||
      _detailTonSelectedCache != selected;
  const bool configuredChanged =
      force || !_detailCacheValid ||
      std::strcmp(_detailTonConfiguredCache, configured) != 0 ||
      frameChanged;
  const bool elapsedChanged =
      force || !_detailCacheValid ||
      std::strcmp(_detailTonElapsedCache, elapsed) != 0 ||
      _detailTonColorCache != colorOn ||
      frameChanged;

  if (frameChanged)
  {
    drawTonPanelFrame(selected);
  }
  if (configuredChanged)
  {
    drawTonTextLine(TON_PANEL_Y + 4,
                    "T ",
                    configured,
                    selected ? COLOR_WARNING : COLOR_TEXT);
  }
  if (elapsedChanged)
  {
    drawTonTextLine(TON_PANEL_Y + 19,
                    "Ta ",
                    elapsed,
                    colorOn ? COLOR_OK : COLOR_MUTED);
  }

  std::snprintf(_detailTonConfiguredCache,
                sizeof(_detailTonConfiguredCache),
                "%s",
                configured);
  std::snprintf(_detailTonElapsedCache,
                sizeof(_detailTonElapsedCache),
                "%s",
                elapsed);
  _detailTonColorCache = colorOn;
  _detailTonSelectedCache = selected;
}

void RuntimeUIFBDMapUnified::captureDetailCache()
{
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    invalidateDetailCache();
    return;
  }

  _detailCacheBlock = _selectedIndex;
  _detailCacheInput = _detailInputIndex;
  _detailCacheFocus = _detailFocus;
  _detailCacheBlockValue = _model->blockValue(_selectedIndex);

  for (uint8_t input = 0;
       input < JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK;
       ++input)
  {
    _detailInputValueCache[input] =
        input < definition->inputCount
            ? _model->inputValue(_selectedIndex, input)
            : false;
  }

  _detailCacheValid = true;
}

RuntimeUIFBDMapUnified::TonBase
RuntimeUIFBDMapUnified::tonBaseFromResource(uint16_t resource)
{
  switch (resource & 0x0003U)
  {
  case 1:
    return TonBase::Minutes;
  case 2:
    return TonBase::Hours;
  case 0:
  default:
    return TonBase::Seconds;
  }
}

void RuntimeUIFBDMapUnified::formatMillisecondsInBase(
    uint32_t milliseconds,
    TonBase base,
    char *destination,
    size_t capacity)
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  uint32_t major = 0;
  uint32_t minor = 0;
  const char *suffix = "s";

  switch (base)
  {
  case TonBase::Minutes:
  {
    const uint32_t totalSeconds = milliseconds / 1000UL;
    major = totalSeconds / 60UL;
    minor = totalSeconds % 60UL;
    suffix = "m";
    break;
  }
  case TonBase::Hours:
  {
    const uint32_t totalMinutes = milliseconds / 60000UL;
    major = totalMinutes / 60UL;
    minor = totalMinutes % 60UL;
    suffix = "h";
    break;
  }
  case TonBase::Seconds:
  default:
  {
    const uint32_t totalCentiseconds = milliseconds / 10UL;
    major = totalCentiseconds / 100UL;
    minor = totalCentiseconds % 100UL;
    suffix = "s";
    break;
  }
  }

  if (major > 99UL)
  {
    major = 99UL;
  }

  std::snprintf(destination,
                capacity,
                "%02lu:%02lu%s",
                static_cast<unsigned long>(major),
                static_cast<unsigned long>(minor),
                suffix);
}

const char *RuntimeUIFBDMapUnified::detailSymbol(
    LogicV2BlockType type) const
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

const char *RuntimeUIFBDMapUnified::stateText() const
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

uint16_t RuntimeUIFBDMapUnified::stateColor() const
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
