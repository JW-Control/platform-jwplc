#include "RuntimeUIFBDMap.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t SOURCE_X = 10;
static constexpr int16_t SOURCE_Y = 42;
static constexpr int16_t SOURCE_W = 84;
static constexpr int16_t SOURCE_H = 48;
static constexpr int16_t TARGET_X = 218;
static constexpr int16_t TARGET_Y = 42;
static constexpr int16_t TARGET_W = 88;
static constexpr int16_t TARGET_H = 48;
static constexpr int16_t SOURCE_FIELD_X = 10;
static constexpr int16_t SOURCE_FIELD_W = 145;
static constexpr int16_t LOGIC_FIELD_X = 165;
static constexpr int16_t LOGIC_FIELD_W = 145;

bool neutralValueFor(LogicV2BlockType type)
{
  return type == LogicV2BlockType::And ||
         type == LogicV2BlockType::Nand;
}
}

void RuntimeUIFBDMap::handleMapInput()
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    return;
  }

  if (_addNodeSelected)
  {
    if (JWPLC_Buttons.pressed(BTN_LEFT) ||
        JWPLC_Buttons.pressed(BTN_ESC))
    {
      JWPLC_Display.notifyActivity();
      leaveAddNode();
      return;
    }
    if (JWPLC_Buttons.pressed(BTN_OK) ||
        JWPLC_Buttons.pressed(BTN_RIGHT))
    {
      JWPLC_Display.notifyActivity();
      _addTypeIndex = 0;
      _addType = AddType::DigitalInput;
      setView(View::AddType, true);
      gateInputUntilRelease(false);
      return;
    }
    return;
  }

  const uint16_t previousSelection = _selectedIndex;
  bool changed = false;
  bool viewportChanged = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    changed = selectSource();
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    changed = selectConsumer();
    if (!changed)
    {
      enterAddNode();
      return;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    changed = selectVertical(false);
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    changed = selectVertical(true);
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    _detailInputIndex = 0;
    _detailFocus = DetailFocus::Inputs;
    _detailParameterIndex = 0;
    normalizeDetailFocus();
    setView(View::Detail, true);
    gateInputUntilRelease(false);
    return;
  }

  if (!changed)
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  viewportChanged = ensureSelectionVisible();
  if (viewportChanged)
  {
    drawMapFull(true);
  }
  else
  {
    // Reponer ambos nodos evita limpiar toda la vista durante navegación normal.
    drawNode(previousSelection);
    drawNode(_selectedIndex);
  }
  drawUnifiedHeader(false);
}

void RuntimeUIFBDMap::normalizeDetailFocus()
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

  const bool hasParameter = definition->type == LogicV2BlockType::Ton;
  if (definition->inputCount == 0 && hasParameter)
  {
    _detailFocus = DetailFocus::Parameters;
  }
  else if (_detailFocus == DetailFocus::Parameters && !hasParameter)
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
  _detailParameterIndex = 0;
}

void RuntimeUIFBDMap::handleDetailInput()
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
        definition->type == LogicV2BlockType::Ton)
    {
      _detailFocus = DetailFocus::Parameters;
      changed = true;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    if (_detailFocus == DetailFocus::Inputs && definition->inputCount > 0)
    {
      _detailInputIndex = _detailInputIndex == 0
                              ? static_cast<uint8_t>(definition->inputCount - 1U)
                              : static_cast<uint8_t>(_detailInputIndex - 1U);
      changed = true;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    if (_detailFocus == DetailFocus::Inputs && definition->inputCount > 0)
    {
      _detailInputIndex = static_cast<uint8_t>(
          (_detailInputIndex + 1U) % definition->inputCount);
      changed = true;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    if (_detailFocus == DetailFocus::Inputs && definition->inputCount > 0)
    {
      if (beginInputEdit())
      {
        JWPLC_Display.notifyActivity();
        setView(View::EditInput, true);
        gateInputUntilRelease(false);
      }
      return;
    }
    if (_detailFocus == DetailFocus::Parameters &&
        definition->type == LogicV2BlockType::Ton)
    {
      if (beginTonEdit())
      {
        JWPLC_Display.notifyActivity();
        setView(View::EditTon, true);
        gateInputUntilRelease(false);
      }
      return;
    }
  }
  else if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    setView(View::Map, true);
    gateInputUntilRelease(false);
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
    drawDetailFull(true);
  }
  else
  {
    if (previousFocus == DetailFocus::Inputs &&
        previousInput < definition->inputCount)
    {
      drawDetailSource(previousInput,
                       static_cast<uint8_t>(previousInput - previousPage),
                       _detailFocus == DetailFocus::Inputs &&
                           previousInput == _detailInputIndex);
    }
    if (_detailFocus == DetailFocus::Inputs &&
        _detailInputIndex < definition->inputCount &&
        (previousFocus != DetailFocus::Inputs ||
         previousInput != _detailInputIndex))
    {
      drawDetailSource(_detailInputIndex,
                       static_cast<uint8_t>(_detailInputIndex - currentPage),
                       true);
    }
    if (definition->type == LogicV2BlockType::Ton &&
        previousFocus != _detailFocus)
    {
      drawTonDetailPanel(_detailFocus == DetailFocus::Parameters, true);
    }
  }
  _detailCacheValid = false;
  drawUnifiedHeader(false);
}

bool RuntimeUIFBDMap::selectedInputAllowsOpen() const
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
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

uint16_t RuntimeUIFBDMap::sourceCandidateCount(bool forAdd) const
{
  const uint16_t maximumBlock = forAdd
                                    ? (_model != nullptr
                                           ? _model->blockCount()
                                           : 0)
                                    : _selectedIndex;
  const bool allowOpen = forAdd
                             ? (_addType == AddType::And2)
                             : selectedInputAllowsOpen();
  return static_cast<uint16_t>(maximumBlock + (allowOpen ? 3U : 2U));
}

uint16_t RuntimeUIFBDMap::sourceCandidateAt(uint16_t candidateIndex,
                                            bool forAdd) const
{
  const bool allowOpen = forAdd
                             ? (_addType == AddType::And2)
                             : selectedInputAllowsOpen();
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

uint16_t RuntimeUIFBDMap::sourceCandidateIndex(uint16_t source,
                                               bool forAdd) const
{
  const bool allowOpen = forAdd
                             ? (_addType == AddType::And2)
                             : selectedInputAllowsOpen();
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
  const uint16_t maximumBlock = forAdd
                                    ? (_model != nullptr
                                           ? _model->blockCount()
                                           : 0)
                                    : _selectedIndex;
  if (source < maximumBlock)
  {
    return static_cast<uint16_t>(specialOffset + 2U + source);
  }
  return specialOffset;
}

void RuntimeUIFBDMap::moveSourceCandidate(bool forward, bool forAdd)
{
  uint16_t &candidate = forAdd ? _addSourceCandidate : _editSourceCandidate;
  const uint16_t count = sourceCandidateCount(forAdd);
  if (count == 0)
  {
    candidate = 0;
    return;
  }
  candidate = forward
                  ? static_cast<uint16_t>((candidate + 1U) % count)
                  : (candidate == 0
                         ? static_cast<uint16_t>(count - 1U)
                         : static_cast<uint16_t>(candidate - 1U));
}

void RuntimeUIFBDMap::formatSource(uint16_t source,
                                   char *id,
                                   size_t idCapacity,
                                   char *type,
                                   size_t typeCapacity) const
{
  if (id == nullptr || idCapacity == 0 ||
      type == nullptr || typeCapacity == 0)
  {
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(id, idCapacity, "X");
    std::snprintf(type, typeCapacity, "ABIERTO");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(id, idCapacity, "HI");
    std::snprintf(type, typeCapacity, "CONST 1");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(id, idCapacity, "LO");
    std::snprintf(type, typeCapacity, "CONST 0");
    return;
  }
  std::snprintf(id, idCapacity, "B%02u", static_cast<unsigned>(source));
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(source) : nullptr;
  std::snprintf(type,
                typeCapacity,
                "%s",
                definition != nullptr ? _model->typeShort(definition->type) : "?");
}

bool RuntimeUIFBDMap::beginInputEdit()
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
  _editSourceCandidate = sourceCandidateIndex(input->source(), false);
  _editInverted = input->inverted();
  _editInputFocus = EditInputFocus::Source;
  _feedback = Feedback::None;
  _awaitingApply = false;
  return true;
}

void RuntimeUIFBDMap::cancelInputEdit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _feedback = Feedback::None;
  _awaitingApply = false;
  setView(View::Detail, true);
  gateInputUntilRelease(false);
}

void RuntimeUIFBDMap::handleEditInput()
{
  if (_awaitingApply)
  {
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    if (_editInputFocus != EditInputFocus::Source)
    {
      _editInputFocus = EditInputFocus::Source;
      changed = true;
    }
    _feedback = Feedback::None;
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (_editInputFocus != EditInputFocus::Logic)
    {
      _editInputFocus = EditInputFocus::Logic;
      changed = true;
    }
    _feedback = Feedback::None;
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    if (_editInputFocus == EditInputFocus::Source)
    {
      moveSourceCandidate(false, false);
    }
    else
    {
      _editInverted = !_editInverted;
    }
    _feedback = Feedback::None;
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    if (_editInputFocus == EditInputFocus::Source)
    {
      moveSourceCandidate(true, false);
    }
    else
    {
      _editInverted = !_editInverted;
    }
    _feedback = Feedback::None;
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
        sourceCandidateAt(_editSourceCandidate, false),
        _editInverted);
    const bool valid = prepared &&
        _editSession.validate() == LogicV2PrototypeError::None;
    JWPLC_Display.notifyActivity();
    if (!valid)
    {
      _feedback = Feedback::Invalid;
      drawEditInputFull(false);
      return;
    }
    _awaitingApply = true;
    _feedback = Feedback::Applying;
    _applyContext = ApplyContext::EditInput;
    _applyRequested = true;
    gateInputUntilRelease(false);
    drawEditInputFull(false);
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawEditInputFull(false);
    drawUnifiedHeader(false);
  }
}

void RuntimeUIFBDMap::drawEditInputFull(bool clearInterior)
{
  if (_model == nullptr)
  {
    return;
  }
  const LogicV2BlockRecord *target = _model->block(_selectedIndex);
  if (target == nullptr || _detailInputIndex >= target->inputCount)
  {
    return;
  }
  if (clearInterior)
  {
    clearMapArea();
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  char sourceId[12];
  char sourceType[14];
  const uint16_t source = sourceCandidateAt(_editSourceCandidate, false);
  formatSource(source,
               sourceId,
               sizeof(sourceId),
               sourceType,
               sizeof(sourceType));

  bool rawValue = false;
  bool sourceKnown = true;
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    rawValue = neutralValueFor(target->type);
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    rawValue = true;
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    rawValue = false;
  }
  else if (source < _model->blockCount())
  {
    rawValue = _model->blockValue(source);
  }
  else
  {
    sourceKnown = false;
  }
  const bool effectiveValue = sourceKnown
                                  ? (_editInverted ? !rawValue : rawValue)
                                  : false;
  const uint16_t sourceBorder = rawValue ? COLOR_OK : COLOR_BORDER;
  const uint16_t wireColor = effectiveValue ? COLOR_OK : COLOR_MUTED;

  tft.fillRect(SOURCE_X, SOURCE_Y, SOURCE_W, SOURCE_H, COLOR_BACKGROUND);
  tft.drawRect(SOURCE_X, SOURCE_Y, SOURCE_W, SOURCE_H, sourceBorder);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(SOURCE_X + 6, SOURCE_Y + 7);
  tft.print(sourceId);
  tft.setTextColor(rawValue ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(SOURCE_X + 6, SOURCE_Y + 26);
  tft.print(sourceType);

  tft.fillRect(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, COLOR_BACKGROUND);
  tft.drawRect(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, COLOR_BORDER);
  char targetId[8];
  formatBlockId(targetId, sizeof(targetId), _selectedIndex);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(TARGET_X + 8, TARGET_Y + 6);
  tft.print(targetId);
  tft.setTextSize(2);
  tft.setTextColor(effectiveValue ? COLOR_OK : COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(TARGET_X + 42, TARGET_Y + 19);
  tft.print(detailSymbol(target->type));
  const char *role = _model->inputRole(target->type, _detailInputIndex);
  char roleText[6];
  if (role != nullptr && role[0] != '\0')
  {
    std::snprintf(roleText, sizeof(roleText), "%s", role);
  }
  else
  {
    std::snprintf(roleText,
                  sizeof(roleText),
                  "%u",
                  static_cast<unsigned>(_detailInputIndex + 1U));
  }
  tft.setTextSize(1);
  tft.setTextColor(wireColor, COLOR_BACKGROUND);
  tft.setCursor(TARGET_X + 7, TARGET_Y + 28);
  tft.print(roleText);

  const int16_t wireY = static_cast<int16_t>(SOURCE_Y + SOURCE_H / 2);
  const int16_t sourceEnd = static_cast<int16_t>(SOURCE_X + SOURCE_W);
  tft.drawFastHLine(sourceEnd,
                    wireY,
                    static_cast<int16_t>(TARGET_X - sourceEnd),
                    wireColor);
  if (_editInverted)
  {
    tft.fillCircle(TARGET_X, wireY, 5, COLOR_PANEL);
    tft.drawCircle(TARGET_X, wireY, 4, COLOR_MUTED);
  }
  else
  {
    tft.fillCircle(TARGET_X, wireY, 2, wireColor);
  }

  char sourceField[24];
  std::snprintf(sourceField, sizeof(sourceField), "FUENTE <%s>", sourceId);
  drawMenuButton(tft,
                 SOURCE_FIELD_X,
                 FIELD_Y,
                 SOURCE_FIELD_W,
                 FIELD_H,
                 sourceField,
                 _editInputFocus == EditInputFocus::Source);
  char logicField[24];
  std::snprintf(logicField,
                sizeof(logicField),
                "LOGICA <%s>",
                _editInverted ? "NEGADA" : "NORMAL");
  drawMenuButton(tft,
                 LOGIC_FIELD_X,
                 FIELD_Y,
                 LOGIC_FIELD_W,
                 FIELD_H,
                 logicField,
                 _editInputFocus == EditInputFocus::Logic);

  const char *status = "OK GUARDAR   ESC CANCELAR";
  uint16_t statusColor = COLOR_MUTED;
  if (_feedback == Feedback::Invalid)
  {
    status = "CONFIGURACION NO VALIDA";
    statusColor = COLOR_ERROR;
  }
  else if (_feedback == Feedback::Applying)
  {
    status = "APLICANDO CAMBIOS...";
    statusColor = COLOR_WARNING;
  }
  else if (_feedback == Feedback::ApplyFailed)
  {
    status = "ERROR AL APLICAR";
    statusColor = COLOR_ERROR;
  }
  updateTextField(tft,
                  78,
                  154,
                  28,
                  status,
                  statusColor,
                  COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}
