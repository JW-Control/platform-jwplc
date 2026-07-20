#include "RuntimeUIFBDMap.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
LogicV2InputLink makeWizardLink(uint16_t source)
{
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    return LogicV2InputLink::open();
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    return LogicV2InputLink::constantTrue();
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    return LogicV2InputLink::constantFalse();
  }
  return LogicV2InputLink::block(source);
}
}

void RuntimeUIFBDMap::enterAddNode()
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    return;
  }
  _addNodeSelected = true;
  _addSourceBlock = _selectedIndex;
  _addSources[0] = _selectedIndex;
  _addSources[1] = _selectedIndex;
  JWPLC_Display.notifyActivity();
  drawMapFull(true);
  drawUnifiedHeader(false);
}

void RuntimeUIFBDMap::leaveAddNode()
{
  _addNodeSelected = false;
  _selectedIndex = _addSourceBlock;
  drawMapFull(true);
  drawUnifiedHeader(false);
  gateInputUntilRelease(false);
}

const char *RuntimeUIFBDMap::addTypeText(AddType type) const
{
  switch (type)
  {
  case AddType::DigitalInput:
    return "ENTRADA DI";
  case AddType::Not:
    return "NOT";
  case AddType::And2:
    return "AND 2";
  case AddType::Ton:
    return "TON";
  case AddType::DigitalOutput:
    return "SALIDA DO";
  case AddType::Count:
  default:
    return "?";
  }
}

LogicV2BlockType RuntimeUIFBDMap::selectedAddLogicType() const
{
  switch (_addType)
  {
  case AddType::DigitalInput:
    return LogicV2BlockType::DigitalInput;
  case AddType::Not:
    return LogicV2BlockType::Not;
  case AddType::And2:
    return LogicV2BlockType::And;
  case AddType::Ton:
    return LogicV2BlockType::Ton;
  case AddType::DigitalOutput:
    return LogicV2BlockType::DigitalOutput;
  case AddType::Count:
  default:
    return LogicV2BlockType::Not;
  }
}

uint8_t RuntimeUIFBDMap::addInputCount() const
{
  switch (_addType)
  {
  case AddType::DigitalInput:
    return 0;
  case AddType::And2:
    return 2;
  case AddType::Not:
  case AddType::Ton:
  case AddType::DigitalOutput:
  default:
    return 1;
  }
}

uint8_t RuntimeUIFBDMap::addConfigRowCount() const
{
  switch (_addType)
  {
  case AddType::DigitalInput:
  case AddType::Not:
    return 2;
  case AddType::And2:
  case AddType::Ton:
  case AddType::DigitalOutput:
  default:
    return 3;
  }
}

const char *RuntimeUIFBDMap::addConfigRowLabel(uint8_t row) const
{
  switch (_addType)
  {
  case AddType::DigitalInput:
    return row == 0 ? "RECURSO" : "CREAR";
  case AddType::Not:
    return row == 0 ? "FUENTE" : "CREAR";
  case AddType::And2:
    return row == 0 ? "IN1" : row == 1 ? "IN2" : "CREAR";
  case AddType::Ton:
    return row == 0 ? "FUENTE" : row == 1 ? "TIEMPO" : "CREAR";
  case AddType::DigitalOutput:
    return row == 0 ? "FUENTE" : row == 1 ? "RECURSO" : "CREAR";
  case AddType::Count:
  default:
    return "?";
  }
}

void RuntimeUIFBDMap::formatAddConfigRowValue(uint8_t row,
                                              char *destination,
                                              size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }
  const char *label = addConfigRowLabel(row);
  if (std::strcmp(label, "CREAR") == 0)
  {
    std::snprintf(destination, capacity, "OK");
    return;
  }
  if (std::strcmp(label, "RECURSO") == 0)
  {
    std::snprintf(destination,
                  capacity,
                  "%c0.%u",
                  _addType == AddType::DigitalInput ? 'I' : 'Q',
                  static_cast<unsigned>(_addResource));
    return;
  }
  if (std::strcmp(label, "TIEMPO") == 0)
  {
    formatTon(_addTonMajor,
              _addTonMinor,
              _addTonBase,
              destination,
              capacity);
    return;
  }
  uint8_t sourceIndex = 0;
  if (_addType == AddType::And2 && row == 1)
  {
    sourceIndex = 1;
  }
  char id[12];
  char type[12];
  formatSource(_addSources[sourceIndex],
               id,
               sizeof(id),
               type,
               sizeof(type));
  std::snprintf(destination, capacity, "%s", id);
}

void RuntimeUIFBDMap::handleAddType()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    setView(View::Map, false);
    leaveAddNode();
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    _addTypeIndex = _addTypeIndex == 0
                        ? static_cast<uint8_t>(AddType::Count) - 1U
                        : static_cast<uint8_t>(_addTypeIndex - 1U);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    _addTypeIndex = static_cast<uint8_t>(
        (_addTypeIndex + 1U) % static_cast<uint8_t>(AddType::Count));
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    _addType = static_cast<AddType>(_addTypeIndex);
    _addConfigRow = 0;
    _addSources[0] = _addSourceBlock;
    _addSources[1] = _addSourceBlock;
    _addResource = 0;
    _addTonMajor = 1;
    _addTonMinor = 0;
    _addTonBase = TonBase::Seconds;
    _addTonField = TonField::Major;

    if (_addType == AddType::DigitalInput ||
        _addType == AddType::DigitalOutput)
    {
      for (uint8_t candidate = 0; candidate < 8; ++candidate)
      {
        bool used = false;
        for (uint16_t block = 0; block < _model->blockCount(); ++block)
        {
          const LogicV2BlockRecord *definition = _model->block(block);
          if (definition == nullptr)
          {
            continue;
          }
          const bool sameType =
              (_addType == AddType::DigitalInput &&
               definition->type == LogicV2BlockType::DigitalInput) ||
              (_addType == AddType::DigitalOutput &&
               definition->type == LogicV2BlockType::DigitalOutput);
          if (sameType && definition->resource == candidate)
          {
            used = true;
            break;
          }
        }
        if (!used)
        {
          _addResource = candidate;
          break;
        }
      }
    }

    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }

  if (changed)
  {
    _addType = static_cast<AddType>(_addTypeIndex);
    JWPLC_Display.notifyActivity();
    drawAddTypeFull(false);
    drawUnifiedHeader(false);
  }
}

void RuntimeUIFBDMap::handleAddConfig()
{
  const uint8_t rowCount = addConfigRowCount();
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    setView(View::AddType, true);
    gateInputUntilRelease(false);
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    _addConfigRow = _addConfigRow == 0
                        ? static_cast<uint8_t>(rowCount - 1U)
                        : static_cast<uint8_t>(_addConfigRow - 1U);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    _addConfigRow = static_cast<uint8_t>((_addConfigRow + 1U) % rowCount);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const char *label = addConfigRowLabel(_addConfigRow);
    JWPLC_Display.notifyActivity();
    if (std::strcmp(label, "CREAR") == 0)
    {
      requestAddBlock();
      return;
    }
    if (std::strcmp(label, "RECURSO") == 0)
    {
      _addResourceBackup = _addResource;
      setView(View::AddResource, true);
      gateInputUntilRelease(false);
      return;
    }
    if (std::strcmp(label, "TIEMPO") == 0)
    {
      _addTonField = TonField::Major;
      setView(View::AddTon, true);
      gateInputUntilRelease(false);
      return;
    }
    _addEditingSourceIndex =
        (_addType == AddType::And2 && _addConfigRow == 1) ? 1 : 0;
    _addSourceCandidate = sourceCandidateIndex(
        _addSources[_addEditingSourceIndex], true);
    setView(View::AddSource, true);
    gateInputUntilRelease(false);
    return;
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawAddConfigFull(false);
  }
}

void RuntimeUIFBDMap::handleAddSource()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    _addSources[_addEditingSourceIndex] =
        sourceCandidateAt(_addSourceCandidate, true);
    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    moveSourceCandidate(false, true);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    moveSourceCandidate(true, true);
    changed = true;
  }
  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawAddSourceFull(false);
  }
}

void RuntimeUIFBDMap::handleAddResource()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    _addResource = _addResourceBackup;
    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }
  uint32_t resource = _addResource;
  const bool changed = JWPLC_Buttons.applyAxis(
      &resource,
      0UL,
      7UL,
      BTN_DOWN,
      BTN_UP,
      true,
      true);
  if (changed)
  {
    _addResource = static_cast<uint8_t>(resource);
    JWPLC_Display.notifyActivity();
    drawAddResourceFull(false);
  }
}

void RuntimeUIFBDMap::handleAddTon()
{
  if (JWPLC_Buttons.pressed(BTN_ESC) || JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    setView(View::AddConfig, true);
    gateInputUntilRelease(false);
    return;
  }

  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    uint8_t field = static_cast<uint8_t>(_addTonField);
    field = field == 0 ? 2U : static_cast<uint8_t>(field - 1U);
    _addTonField = static_cast<TonField>(field);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    _addTonField = static_cast<TonField>(
        (static_cast<uint8_t>(_addTonField) + 1U) % 3U);
    changed = true;
  }
  else if (_addTonField == TonField::Base)
  {
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (!up && !down)
    {
      return;
    }
    uint8_t base = static_cast<uint8_t>(_addTonBase);
    base = down
               ? static_cast<uint8_t>((base + 1U) % 3U)
               : (base == 0 ? 2U : static_cast<uint8_t>(base - 1U));
    _addTonBase = static_cast<TonBase>(base);
    if (_addTonMinor > tonMinorMaximum(_addTonBase))
    {
      _addTonMinor = tonMinorMaximum(_addTonBase);
    }
    changed = true;
  }
  else if (_addTonField == TonField::Major)
  {
    changed = JWPLC_Buttons.applyAxis(
        &_addTonMajor,
        0UL,
        99UL,
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }
  else if (_addTonField == TonField::Minor)
  {
    changed = JWPLC_Buttons.applyAxis(
        &_addTonMinor,
        0UL,
        tonMinorMaximum(_addTonBase),
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    drawAddTonFull(false);
  }
}

void RuntimeUIFBDMap::requestAddBlock()
{
  if (!_editSession.begin())
  {
    _feedback = Feedback::Invalid;
    drawAddConfigFull(false);
    return;
  }

  LogicV2InputLink links[2];
  const uint8_t inputCount = addInputCount();
  for (uint8_t input = 0; input < inputCount; ++input)
  {
    links[input] = makeWizardLink(_addSources[input]);
  }

  uint16_t resource = 0;
  uint32_t parameter = 0;
  if (_addType == AddType::DigitalInput ||
      _addType == AddType::DigitalOutput)
  {
    resource = _addResource;
  }
  if (_addType == AddType::Ton)
  {
    resource = tonResource(_addTonBase);
    parameter = encodeTonMilliseconds(_addTonMajor,
                                      _addTonMinor,
                                      _addTonBase);
    if (parameter < TON_MINIMUM_MS)
    {
      _editSession.cancel();
      _feedback = Feedback::Invalid;
      drawAddConfigFull(false);
      return;
    }
  }

  const bool prepared = _editSession.appendBlock(
      selectedAddLogicType(),
      inputCount > 0 ? links : nullptr,
      inputCount,
      resource,
      parameter,
      &_newBlockIndex);
  const bool valid = prepared &&
      _editSession.validate() == LogicV2PrototypeError::None;
  if (!valid)
  {
    _editSession.cancel();
    _feedback = Feedback::Invalid;
    drawAddConfigFull(false);
    return;
  }

  _feedback = Feedback::Applying;
  _awaitingApply = true;
  _applyContext = ApplyContext::AddBlock;
  _applyRequested = true;
  gateInputUntilRelease(false);
  drawAddConfigFull(false);
}

void RuntimeUIFBDMap::drawAddTypeFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  static constexpr int16_t x = 45;
  static constexpr int16_t y0 = 34;
  static constexpr int16_t w = 230;
  static constexpr int16_t h = 22;
  static constexpr int16_t step = 25;
  for (uint8_t index = 0; index < static_cast<uint8_t>(AddType::Count); ++index)
  {
    drawMenuButton(tft,
                   x,
                   static_cast<int16_t>(y0 + index * step),
                   w,
                   h,
                   addTypeText(static_cast<AddType>(index)),
                   index == _addTypeIndex);
  }
  updateTextField(tft,
                  78,
                  158,
                  28,
                  "OK ELEGIR   ESC MAPA",
                  COLOR_MUTED,
                  COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMap::drawAddConfigFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint8_t count = addConfigRowCount();
  const int16_t y0 = count == 2 ? 55 : 42;
  const int16_t step = 34;
  for (uint8_t row = 0; row < count; ++row)
  {
    char value[20];
    char line[32];
    formatAddConfigRowValue(row, value, sizeof(value));
    std::snprintf(line,
                  sizeof(line),
                  "%s  <%s>",
                  addConfigRowLabel(row),
                  value);
    drawMenuButton(tft,
                   35,
                   static_cast<int16_t>(y0 + row * step),
                   250,
                   28,
                   line,
                   row == _addConfigRow);
  }

  const char *footer = "UP/DOWN FILA   OK ENTRAR";
  uint16_t color = COLOR_MUTED;
  if (_feedback == Feedback::Invalid)
  {
    footer = "CONFIGURACION NO VALIDA";
    color = COLOR_ERROR;
  }
  else if (_feedback == Feedback::Applying)
  {
    footer = "APLICANDO CAMBIOS...";
    color = COLOR_WARNING;
  }
  else if (_feedback == Feedback::ApplyFailed)
  {
    footer = "ERROR AL APLICAR";
    color = COLOR_ERROR;
  }
  updateTextField(tft, 60, 158, 32, footer, color, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMap::drawAddSourceFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  const uint16_t source = sourceCandidateAt(_addSourceCandidate, true);
  char id[12];
  char type[14];
  formatSource(source, id, sizeof(id), type, sizeof(type));
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(85, 50, 150, 60, COLOR_BACKGROUND);
  tft.drawRect(85, 50, 150, 60, COLOR_WARNING);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(100, 60);
  tft.print(id);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(100, 90);
  tft.print(type);
  updateTextField(tft,
                  45,
                  130,
                  38,
                  "UP/DOWN FUENTE   OK ACEPTAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateTextField(tft,
                  78,
                  154,
                  28,
                  "ESC CANCELAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMap::drawAddResourceFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  char value[20];
  std::snprintf(value,
                sizeof(value),
                "%c0.%u",
                _addType == AddType::DigitalInput ? 'I' : 'Q',
                static_cast<unsigned>(_addResource));
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawMenuButton(tft, 70, 60, 180, 50, value, true);
  updateTextField(tft,
                  45,
                  130,
                  38,
                  "UP/DOWN RECURSO   OK ACEPTAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateTextField(tft,
                  78,
                  154,
                  28,
                  "ESC CANCELAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMap::drawAddTonFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const char *labels[3] = {
      tonMajorLabel(_addTonBase),
      tonMinorLabel(_addTonBase),
      "BASE"};
  char values[3][10];
  std::snprintf(values[0], sizeof(values[0]), "<%02u>",
                static_cast<unsigned>(_addTonMajor));
  std::snprintf(values[1], sizeof(values[1]), "<%02u>",
                static_cast<unsigned>(_addTonMinor));
  std::snprintf(values[2], sizeof(values[2]), "<%s>", tonBaseText(_addTonBase));
  const int16_t xs[3] = {10, 108, 206};
  const int16_t ws[3] = {94, 94, 104};
  for (uint8_t field = 0; field < 3; ++field)
  {
    char full[24];
    std::snprintf(full, sizeof(full), "%s %s", labels[field], values[field]);
    drawMenuButton(tft,
                   xs[field],
                   70,
                   ws[field],
                   38,
                   full,
                   field == static_cast<uint8_t>(_addTonField));
  }
  updateTextField(tft,
                  40,
                  130,
                  40,
                  "LEFT/RIGHT CAMPO   UP/DOWN VALOR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateTextField(tft,
                  58,
                  154,
                  34,
                  "OK/ESC VOLVER A CONFIGURAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}
