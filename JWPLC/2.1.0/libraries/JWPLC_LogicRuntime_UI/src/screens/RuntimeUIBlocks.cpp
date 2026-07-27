#include "RuntimeUIBlocks.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  static constexpr int16_t ROW_X = 8;
  static constexpr int16_t ROW_W = 304;
  static constexpr int16_t ROW_Y[5] = {68, 80, 92, 104, 116};

  static constexpr int16_t COMMAND_Y = 135;
  static constexpr int16_t COMMAND_H = 20;
  static constexpr int16_t COMMAND_X[2] = {4, 163};
  static constexpr int16_t COMMAND_W = 153;
  static const char *const COMMAND_LABELS[2] = {"DETALLE", "VOLVER"};
}

RuntimeUIBlocks::RuntimeUIBlocks()
    : _runtime(nullptr),
      _cache{},
      _mode(Mode::List),
      _fullRedraw(true),
      _selectedIndex(0),
      _topIndex(0),
      _selectedCommand(0),
      _detailValueValid(false),
      _detailValue(false),
      _lastValueRefreshMs(0),
      _requestedView(RuntimeUIView::None)
{
  invalidateCache();
}

void RuntimeUIBlocks::attach(JWPLC_LogicRuntime &runtime)
{
  _runtime = &runtime;
  forceRedraw();
}

void RuntimeUIBlocks::enter()
{
  _mode = Mode::List;
  _selectedIndex = 0;
  _topIndex = 0;
  _selectedCommand = 0;
  _requestedView = RuntimeUIView::None;
  _detailValueValid = false;
  _lastValueRefreshMs = millis();

  normalizeSelection();
  invalidateCache();
  drawCurrentMode();
  _fullRedraw = false;
}

void RuntimeUIBlocks::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_runtime == nullptr)
  {
    return;
  }

  const uint16_t currentBlockCount = _runtime->blockCount();
  if (_cache.valid && currentBlockCount != _cache.blockCount)
  {
    normalizeSelection();
    forceRedraw();
  }

  if (_fullRedraw)
  {
    invalidateCache();
    drawCurrentMode();
    _fullRedraw = false;
  }

  if (_mode == Mode::List)
  {
    handleListInput();
  }
  else
  {
    handleDetailInput();
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - _lastValueRefreshMs) >= VALUE_REFRESH_MS)
  {
    _lastValueRefreshMs = now;
    if (_mode == Mode::List)
    {
      updateListValues(false);
    }
    else
    {
      updateDetailFields(false);
    }
  }
}

void RuntimeUIBlocks::exit()
{
  _requestedView = RuntimeUIView::None;
  _detailValueValid = false;
}

void RuntimeUIBlocks::forceRedraw()
{
  _fullRedraw = true;
  _detailValueValid = false;
  invalidateCache();
}

RuntimeUIView RuntimeUIBlocks::takeRequestedView()
{
  const RuntimeUIView requested = _requestedView;
  _requestedView = RuntimeUIView::None;
  return requested;
}

void RuntimeUIBlocks::invalidateCache()
{
  std::memset(&_cache, 0, sizeof(_cache));
  _cache.valid = false;
}

void RuntimeUIBlocks::drawCurrentMode()
{
  if (_mode == Mode::Detail)
  {
    drawDetailStatic();
    updateDetailFields(true);
    return;
  }

  drawListStatic();
  drawListRows(true);
  drawListCommands();
  updateListValues(true);
}

void RuntimeUIBlocks::drawListStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "BLOQUES");
  updateHeaderState(tft, runtimeStateText(), runtimeStateColor());

  drawPanel(tft, 4, 28, 312, 103, "PROGRAMA");
  drawFieldLabel(tft, 12, 43, "Nombre:");
  drawFieldLabel(tft,
                 12,
                 56,
                 "#  TIPO      A    B    RECURSO   V",
                 COLOR_ACCENT,
                 COLOR_PANEL);

  char programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1] = {};
  copyProgramName(programName, sizeof(programName));
  updateTextField(tft, 56, 43, 42, programName);
  drawFooter(tft, "UP/DN: bloque  L/R: accion  ESC: IDLE");
}

void RuntimeUIBlocks::drawListRows(bool force)
{
  const uint16_t blockCount = _runtime ? _runtime->blockCount() : 0;

  if (blockCount == 0)
  {
    Adafruit_ST7789 &tft = JWPLC_Display.tft();
    tft.fillRect(ROW_X, ROW_Y[0], ROW_W, 58, COLOR_PANEL);
    updateTextField(tft,
                    18,
                    88,
                    45,
                    "SIN PROGRAMA CARGADO",
                    COLOR_WARNING,
                    COLOR_PANEL);
    _selectedCommand = 1;
  }
  else
  {
    for (uint8_t row = 0; row < VISIBLE_ROWS; ++row)
    {
      const uint16_t blockIndex = static_cast<uint16_t>(_topIndex + row);
      drawListRow(row, blockIndex, blockIndex == _selectedIndex);
    }
  }

  if (force || !_cache.valid)
  {
    char currentName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1] = {};
    copyProgramName(currentName, sizeof(currentName));
    std::strncpy(_cache.programName,
                 currentName,
                 sizeof(_cache.programName) - 1);
    _cache.programName[sizeof(_cache.programName) - 1] = '\0';
  }

  _cache.hasProgram = blockCount > 0;
  _cache.blockCount = blockCount;
  _cache.topIndex = _topIndex;
  _cache.selectedIndex = _selectedIndex;
  _cache.valid = true;
}

void RuntimeUIBlocks::drawListRow(uint8_t visibleRow,
                                  uint16_t blockIndex,
                                  bool selected)
{
  if (visibleRow >= VISIBLE_ROWS)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t background = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t foreground = selected ? COLOR_ACCENT : COLOR_TEXT;
  tft.fillRect(ROW_X, ROW_Y[visibleRow], ROW_W, 10, background);

  const LogicBlockDefinition *block =
      _runtime ? _runtime->blockDefinition(blockIndex) : nullptr;
  if (block == nullptr)
  {
    _cache.valueValid[visibleRow] = false;
    return;
  }

  char sourceA[8];
  char sourceB[8];
  char resource[16];
  formatSource(sourceA, sizeof(sourceA), block->sourceA);
  formatSource(sourceB, sizeof(sourceB), block->sourceB);
  formatResource(resource, sizeof(resource), *block);

  const bool value = _runtime->blockValue(blockIndex);
  char line[64];
  std::snprintf(line,
                sizeof(line),
                "%02u %-9s %-4s %-4s %-9s %c",
                static_cast<unsigned int>(blockIndex),
                blockTypeName(block->type),
                sourceA,
                sourceB,
                resource,
                value ? '1' : '0');

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(foreground);
  tft.setCursor(12, ROW_Y[visibleRow] + 1);
  tft.print(line);

  _cache.values[visibleRow] = value;
  _cache.valueValid[visibleRow] = true;
}

void RuntimeUIBlocks::drawListCommands()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint8_t command = 0; command < 2; ++command)
  {
    drawMenuButton(tft,
                   COMMAND_X[command],
                   COMMAND_Y,
                   COMMAND_W,
                   COMMAND_H,
                   COMMAND_LABELS[command],
                   command == _selectedCommand);
  }
}

void RuntimeUIBlocks::redrawListCommand(uint8_t previousCommand,
                                        uint8_t currentCommand)
{
  if (previousCommand > 1 || currentCommand > 1)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawMenuButton(tft,
                 COMMAND_X[previousCommand],
                 COMMAND_Y,
                 COMMAND_W,
                 COMMAND_H,
                 COMMAND_LABELS[previousCommand],
                 false);
  drawMenuButton(tft,
                 COMMAND_X[currentCommand],
                 COMMAND_Y,
                 COMMAND_W,
                 COMMAND_H,
                 COMMAND_LABELS[currentCommand],
                 true);
}

void RuntimeUIBlocks::redrawListSelection(uint16_t previousIndex,
                                          uint16_t currentIndex,
                                          uint16_t previousTopIndex)
{
  if (previousTopIndex != _topIndex)
  {
    for (uint8_t row = 0; row < VISIBLE_ROWS; ++row)
    {
      _cache.valueValid[row] = false;
    }
    drawListRows(true);
    return;
  }

  if (previousIndex >= _topIndex &&
      previousIndex < static_cast<uint16_t>(_topIndex + VISIBLE_ROWS))
  {
    drawListRow(static_cast<uint8_t>(previousIndex - _topIndex),
                previousIndex,
                false);
  }

  if (currentIndex >= _topIndex &&
      currentIndex < static_cast<uint16_t>(_topIndex + VISIBLE_ROWS))
  {
    drawListRow(static_cast<uint8_t>(currentIndex - _topIndex),
                currentIndex,
                true);
  }

  _cache.selectedIndex = _selectedIndex;
}

void RuntimeUIBlocks::updateListValues(bool force)
{
  const uint16_t blockCount = _runtime ? _runtime->blockCount() : 0;
  if (blockCount == 0)
  {
    return;
  }

  for (uint8_t row = 0; row < VISIBLE_ROWS; ++row)
  {
    const uint16_t blockIndex = static_cast<uint16_t>(_topIndex + row);
    if (blockIndex >= blockCount)
    {
      continue;
    }

    const bool value = _runtime->blockValue(blockIndex);
    if (force || !_cache.valueValid[row] || value != _cache.values[row])
    {
      drawListRow(row, blockIndex, blockIndex == _selectedIndex);
    }
  }
}

void RuntimeUIBlocks::handleListInput()
{
  const uint16_t blockCount = _runtime->blockCount();

  if (blockCount > 0 && JWPLC_Buttons.pressed(BTN_UP))
  {
    const uint16_t previousIndex = _selectedIndex;
    const uint16_t previousTop = _topIndex;
    if (_selectedIndex > 0)
    {
      --_selectedIndex;
    }
    if (_selectedIndex < _topIndex)
    {
      _topIndex = _selectedIndex;
    }
    JWPLC_Display.notifyActivity();
    redrawListSelection(previousIndex, _selectedIndex, previousTop);
  }
  else if (blockCount > 0 && JWPLC_Buttons.pressed(BTN_DOWN))
  {
    const uint16_t previousIndex = _selectedIndex;
    const uint16_t previousTop = _topIndex;
    if (_selectedIndex + 1U < blockCount)
    {
      ++_selectedIndex;
    }
    if (_selectedIndex >= static_cast<uint16_t>(_topIndex + VISIBLE_ROWS))
    {
      _topIndex = static_cast<uint16_t>(_selectedIndex - VISIBLE_ROWS + 1U);
    }
    JWPLC_Display.notifyActivity();
    redrawListSelection(previousIndex, _selectedIndex, previousTop);
  }

  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    const uint8_t previousCommand = _selectedCommand;
    _selectedCommand = _selectedCommand == 0 ? 1 : 0;
    if (blockCount == 0)
    {
      _selectedCommand = 1;
    }
    JWPLC_Display.notifyActivity();
    redrawListCommand(previousCommand, _selectedCommand);
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    if (_selectedCommand == 0 && blockCount > 0)
    {
      _mode = Mode::Detail;
      _detailValueValid = false;
      invalidateCache();
      drawCurrentMode();
      _fullRedraw = false;
      return;
    }
    _requestedView = RuntimeUIView::Home;
  }
}

void RuntimeUIBlocks::drawDetailStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "DETALLE BLOQUE");
  updateHeaderState(tft, runtimeStateText(), runtimeStateColor());

  drawPanel(tft, 4, 28, 312, 123, "DEFINICION");
  drawFieldLabel(tft, 12, 43, "Bloque:");
  drawFieldLabel(tft, 12, 57, "Tipo:");
  drawFieldLabel(tft, 12, 71, "Fuente A:");
  drawFieldLabel(tft, 12, 85, "Fuente B:");
  drawFieldLabel(tft, 12, 99, "Recurso:");
  drawFieldLabel(tft, 12, 113, "Parametro:");
  drawFieldLabel(tft, 12, 127, "Flags:");
  drawFieldLabel(tft, 12, 141, "Valor:");

  drawFooter(tft, "UP/DN: otro bloque  OK: lista  ESC: IDLE");
}

void RuntimeUIBlocks::updateDetailFields(bool force)
{
  const LogicBlockDefinition *block =
      _runtime ? _runtime->blockDefinition(_selectedIndex) : nullptr;
  if (block == nullptr)
  {
    _mode = Mode::List;
    normalizeSelection();
    invalidateCache();
    drawCurrentMode();
    return;
  }

  if (force)
  {
    char blockPosition[24];
    std::snprintf(blockPosition,
                  sizeof(blockPosition),
                  "%u de %u",
                  static_cast<unsigned int>(_selectedIndex + 1U),
                  static_cast<unsigned int>(_runtime->blockCount()));
    updateTextField(JWPLC_Display.tft(), 68, 43, 38, blockPosition);
    updateTextField(JWPLC_Display.tft(),
                    68,
                    57,
                    38,
                    blockTypeName(block->type));

    char sourceA[16];
    char sourceB[16];
    char resource[24];
    char parameter[24];
    formatSource(sourceA, sizeof(sourceA), block->sourceA);
    formatSource(sourceB, sizeof(sourceB), block->sourceB);
    formatResource(resource, sizeof(resource), *block);
    formatParameter(parameter, sizeof(parameter), *block);

    updateTextField(JWPLC_Display.tft(), 68, 71, 38, sourceA);
    updateTextField(JWPLC_Display.tft(), 68, 85, 38, sourceB);
    updateTextField(JWPLC_Display.tft(), 68, 99, 38, resource);
    updateTextField(JWPLC_Display.tft(), 68, 113, 38, parameter);
    updateTextField(JWPLC_Display.tft(),
                    68,
                    127,
                    38,
                    block->isRetentive() ? "RETENTIVO" : "NINGUNO");
  }

  const bool value = _runtime->blockValue(_selectedIndex);
  if (force || !_detailValueValid || value != _detailValue)
  {
    updateTextField(JWPLC_Display.tft(),
                    68,
                    141,
                    38,
                    value ? "TRUE" : "FALSE",
                    value ? COLOR_OK : COLOR_TEXT,
                    COLOR_PANEL);
    _detailValue = value;
    _detailValueValid = true;
  }
}

void RuntimeUIBlocks::handleDetailInput()
{
  const uint16_t blockCount = _runtime->blockCount();

  if ((JWPLC_Buttons.pressed(BTN_UP) ||
       JWPLC_Buttons.pressed(BTN_LEFT)) &&
      _selectedIndex > 0)
  {
    --_selectedIndex;
    _detailValueValid = false;
    JWPLC_Display.notifyActivity();
    updateDetailFields(true);
  }
  else if ((JWPLC_Buttons.pressed(BTN_DOWN) ||
            JWPLC_Buttons.pressed(BTN_RIGHT)) &&
           _selectedIndex + 1U < blockCount)
  {
    ++_selectedIndex;
    _detailValueValid = false;
    JWPLC_Display.notifyActivity();
    updateDetailFields(true);
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::List;
    if (_selectedIndex < _topIndex)
    {
      _topIndex = _selectedIndex;
    }
    else if (_selectedIndex >= static_cast<uint16_t>(_topIndex + VISIBLE_ROWS))
    {
      _topIndex = static_cast<uint16_t>(_selectedIndex - VISIBLE_ROWS + 1U);
    }
    invalidateCache();
    drawCurrentMode();
    _fullRedraw = false;
  }
}

void RuntimeUIBlocks::normalizeSelection()
{
  const uint16_t blockCount = _runtime ? _runtime->blockCount() : 0;
  if (blockCount == 0)
  {
    _selectedIndex = 0;
    _topIndex = 0;
    _selectedCommand = 1;
    return;
  }

  if (_selectedIndex >= blockCount)
  {
    _selectedIndex = static_cast<uint16_t>(blockCount - 1U);
  }
  if (_topIndex > _selectedIndex)
  {
    _topIndex = _selectedIndex;
  }
  if (_selectedIndex >= static_cast<uint16_t>(_topIndex + VISIBLE_ROWS))
  {
    _topIndex = static_cast<uint16_t>(_selectedIndex - VISIBLE_ROWS + 1U);
  }
}

void RuntimeUIBlocks::copyProgramName(char *destination,
                                      size_t destinationCapacity) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  const LogicProgram *program = _runtime ? _runtime->program() : nullptr;
  const char *source = program && program->name
                           ? program->name
                           : "SIN PROGRAMA";
  std::strncpy(destination, source, destinationCapacity - 1);
  destination[destinationCapacity - 1] = '\0';
}

const char *RuntimeUIBlocks::blockTypeName(LogicBlockType type) const
{
  switch (type)
  {
  case LogicBlockType::DigitalInput:
    return "ENTRADA";
  case LogicBlockType::DigitalOutput:
    return "SALIDA";
  case LogicBlockType::Not:
    return "NOT";
  case LogicBlockType::And:
    return "AND";
  case LogicBlockType::Or:
    return "OR";
  case LogicBlockType::SetReset:
    return "SET/RESET";
  case LogicBlockType::Ton:
    return "TON";
  default:
    return "DESCONOCIDO";
  }
}

void RuntimeUIBlocks::formatSource(char *destination,
                                   size_t destinationCapacity,
                                   uint16_t source) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  if (source == JWPLC_LOGIC_NO_SOURCE)
  {
    std::snprintf(destination, destinationCapacity, "-");
  }
  else
  {
    std::snprintf(destination,
                  destinationCapacity,
                  "%u",
                  static_cast<unsigned int>(source));
  }
}

void RuntimeUIBlocks::formatResource(char *destination,
                                     size_t destinationCapacity,
                                     const LogicBlockDefinition &block) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  switch (block.type)
  {
  case LogicBlockType::DigitalInput:
    std::snprintf(destination,
                  destinationCapacity,
                  "I0.%u",
                  static_cast<unsigned int>(block.resource));
    break;
  case LogicBlockType::DigitalOutput:
    std::snprintf(destination,
                  destinationCapacity,
                  "Q0.%u",
                  static_cast<unsigned int>(block.resource));
    break;
  case LogicBlockType::Ton:
    std::snprintf(destination,
                  destinationCapacity,
                  "%lums",
                  static_cast<unsigned long>(block.parameter));
    break;
  case LogicBlockType::SetReset:
    std::snprintf(destination,
                  destinationCapacity,
                  "%s",
                  block.isRetentive() ? "RET" : "-");
    break;
  default:
    std::snprintf(destination, destinationCapacity, "-");
    break;
  }
}

void RuntimeUIBlocks::formatParameter(char *destination,
                                      size_t destinationCapacity,
                                      const LogicBlockDefinition &block) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return;
  }

  if (block.type == LogicBlockType::Ton)
  {
    std::snprintf(destination,
                  destinationCapacity,
                  "%lu ms",
                  static_cast<unsigned long>(block.parameter));
  }
  else
  {
    std::snprintf(destination, destinationCapacity, "-");
  }
}

uint16_t RuntimeUIBlocks::runtimeStateColor() const
{
  if (_runtime == nullptr)
  {
    return COLOR_MUTED;
  }

  switch (_runtime->state())
  {
  case JWPLCLogicRuntimeState::Running:
    return COLOR_OK;
  case JWPLCLogicRuntimeState::Fault:
    return COLOR_ERROR;
  case JWPLCLogicRuntimeState::Stopped:
    return COLOR_WARNING;
  case JWPLCLogicRuntimeState::Ready:
  default:
    return COLOR_ACCENT;
  }
}

const char *RuntimeUIBlocks::runtimeStateText() const
{
  return _runtime
             ? JWPLC_LogicRuntime::stateName(_runtime->state())
             : "SIN RUNTIME";
}