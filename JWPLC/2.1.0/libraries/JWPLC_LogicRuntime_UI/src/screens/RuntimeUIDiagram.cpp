#include "RuntimeUIDiagram.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  static constexpr int16_t GRAPH_X = 8;
  static constexpr int16_t GRAPH_Y = 54;
  static constexpr int16_t GRAPH_W = 304;
  static constexpr int16_t GRAPH_H = 74;

  static constexpr int16_t MAIN_X = 120;
  static constexpr int16_t MAIN_Y = 67;
  static constexpr int16_t MAIN_W = 80;
  static constexpr int16_t MAIN_H = 56;

  static constexpr int16_t MINI_W = 88;
  static constexpr int16_t MINI_H = 26;
  static constexpr int16_t LEFT_X = 10;
  static constexpr int16_t RIGHT_X = 222;
  static constexpr int16_t TOP_Y = 58;
  static constexpr int16_t BOTTOM_Y = 98;

  static constexpr int16_t COMMAND_Y = 135;
  static constexpr int16_t COMMAND_H = 20;
  static constexpr int16_t COMMAND_X[3] = {4, 110, 216};
  static constexpr int16_t COMMAND_W = 100;
  static const char *const COMMAND_LABELS[3] = {
      "DETALLE",
      "LISTA",
      "VOLVER"};

  void drawWireSegment(Adafruit_ST7789 &tft,
                       int16_t x0,
                       int16_t y0,
                       int16_t x1,
                       int16_t y1,
                       uint16_t color,
                       bool active)
  {
    if (x0 == x1)
    {
      const int16_t top = y0 < y1 ? y0 : y1;
      const int16_t height = static_cast<int16_t>(y0 < y1 ? y1 - y0 + 1 : y0 - y1 + 1);
      tft.drawFastVLine(x0, top, height, color);
      if (active)
      {
        tft.drawFastVLine(x0 + 1, top, height, color);
      }
      return;
    }

    const int16_t left = x0 < x1 ? x0 : x1;
    const int16_t width = static_cast<int16_t>(x0 < x1 ? x1 - x0 + 1 : x0 - x1 + 1);
    tft.drawFastHLine(left, y0, width, color);
    if (active)
    {
      tft.drawFastHLine(left, y0 + 1, width, color);
    }
  }
}

RuntimeUIDiagram::RuntimeUIDiagram()
    : _runtime(nullptr),
      _cache{},
      _mode(Mode::Graph),
      _fullRedraw(true),
      _selectedIndex(0),
      _selectedCommand(0),
      _detailValueValid(false),
      _detailValue(false),
      _lastValueRefreshMs(0),
      _requestedView(RuntimeUIView::None)
{
  invalidateCache();
}

void RuntimeUIDiagram::attach(JWPLC_LogicRuntime &runtime)
{
  _runtime = &runtime;
  forceRedraw();
}

void RuntimeUIDiagram::enter()
{
  _mode = Mode::Graph;
  _selectedCommand = 0;
  _requestedView = RuntimeUIView::None;
  _detailValueValid = false;
  _lastValueRefreshMs = millis();

  normalizeSelection();
  invalidateCache();
  drawCurrentMode();
  _fullRedraw = false;
}

void RuntimeUIDiagram::refresh(const JWPLC_IOState *io,
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

  const JWPLCLogicRuntimeState currentState = _runtime->state();
  if (!_cache.valid || currentState != _cache.runtimeState)
  {
    updateHeaderState(JWPLC_Display.tft(),
                      runtimeStateText(),
                      runtimeStateColor());
    _cache.runtimeState = currentState;
  }

  if (_mode == Mode::Graph)
  {
    handleGraphInput();
  }
  else
  {
    handleDetailInput();
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - _lastValueRefreshMs) >= VALUE_REFRESH_MS)
  {
    _lastValueRefreshMs = now;
    if (_mode == Mode::Graph)
    {
      updateGraphValues(false);
    }
    else
    {
      updateDetailFields(false);
    }
  }
}

void RuntimeUIDiagram::exit()
{
  _requestedView = RuntimeUIView::None;
  _detailValueValid = false;
}

void RuntimeUIDiagram::forceRedraw()
{
  _fullRedraw = true;
  _detailValueValid = false;
  invalidateCache();
}

RuntimeUIView RuntimeUIDiagram::takeRequestedView()
{
  const RuntimeUIView requested = _requestedView;
  _requestedView = RuntimeUIView::None;
  return requested;
}

void RuntimeUIDiagram::invalidateCache()
{
  std::memset(&_cache, 0, sizeof(_cache));
  _cache.valid = false;
}

void RuntimeUIDiagram::normalizeSelection()
{
  const uint16_t blockCount = _runtime ? _runtime->blockCount() : 0;
  if (blockCount == 0)
  {
    _selectedIndex = 0;
    _selectedCommand = 2;
    return;
  }

  if (_selectedIndex >= blockCount)
  {
    _selectedIndex = static_cast<uint16_t>(blockCount - 1U);
  }

  if (_selectedCommand >= COMMAND_COUNT)
  {
    _selectedCommand = 0;
  }
}

void RuntimeUIDiagram::drawCurrentMode()
{
  if (_mode == Mode::Detail)
  {
    drawDetailStatic();
    updateDetailFields(true);
    _cache.runtimeState = _runtime
                              ? _runtime->state()
                              : JWPLCLogicRuntimeState::Stopped;
    _cache.blockCount = _runtime ? _runtime->blockCount() : 0;
    _cache.valid = true;
    return;
  }

  drawGraphStatic();
  drawGraph(true);
  drawGraphCommands();
}

void RuntimeUIDiagram::drawGraphStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "DIAGRAMA");
  updateHeaderState(tft, runtimeStateText(), runtimeStateColor());

  drawPanel(tft, 4, 28, 312, 103, "FLUJO LOGICO");
  drawFieldLabel(tft, 12, 43, "Programa:");

  char programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1] = {};
  copyProgramName(programName, sizeof(programName));
  updateTextField(tft, 68, 43, 40, programName);

  drawFooter(tft, "UP/DN: bloque  L/R: accion  ESC: IDLE");
}

void RuntimeUIDiagram::drawGraph(bool clearViewport)
{
  if (_runtime == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  if (clearViewport)
  {
    tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, COLOR_PANEL);
  }

  const uint16_t blockCount = _runtime->blockCount();
  if (blockCount == 0)
  {
    drawNoProgram();
    _cache.runtimeState = _runtime->state();
    _cache.blockCount = 0;
    _cache.valid = true;
    return;
  }

  normalizeSelection();
  const LogicBlockDefinition *block =
      _runtime->blockDefinition(_selectedIndex);
  if (block == nullptr)
  {
    drawNoProgram();
    return;
  }

  const bool selectedValue = _runtime->blockValue(_selectedIndex);
  const bool sourceAValid =
      expectsSourceA(block->type) &&
      block->sourceA != JWPLC_LOGIC_NO_SOURCE &&
      _runtime->blockDefinition(block->sourceA) != nullptr;
  const bool sourceBValid =
      expectsSourceB(block->type) &&
      block->sourceB != JWPLC_LOGIC_NO_SOURCE &&
      _runtime->blockDefinition(block->sourceB) != nullptr;

  const bool sourceAValue =
      sourceAValid ? _runtime->blockValue(block->sourceA) : selectedValue;
  const bool sourceBValue =
      sourceBValid ? _runtime->blockValue(block->sourceB) : false;

  uint16_t targets[2] = {
      JWPLC_LOGIC_NO_SOURCE,
      JWPLC_LOGIC_NO_SOURCE};
  const uint8_t targetCount = collectTargets(_selectedIndex, targets);

  const char *portA = block->type == LogicBlockType::SetReset ? "S" : "A";
  const char *portB = block->type == LogicBlockType::SetReset ? "R" : "B";

  // Conexiones primero para que los nodos oculten correctamente los extremos.
  if (block->type == LogicBlockType::DigitalInput)
  {
    drawLeftConnection(TOP_Y + MINI_H / 2,
                       MAIN_Y + 16,
                       selectedValue,
                       "I");
  }
  else
  {
    if (sourceAValid)
    {
      drawLeftConnection(TOP_Y + MINI_H / 2,
                         MAIN_Y + 16,
                         sourceAValue,
                         portA);
    }
    if (sourceBValid)
    {
      drawLeftConnection(BOTTOM_Y + MINI_H / 2,
                         MAIN_Y + 40,
                         sourceBValue,
                         portB);
    }
  }

  if (block->type == LogicBlockType::DigitalOutput)
  {
    drawRightConnection(MAIN_Y + MAIN_H / 2,
                        TOP_Y + MINI_H / 2,
                        selectedValue);
  }
  else if (targetCount == 0)
  {
    drawRightConnection(MAIN_Y + MAIN_H / 2,
                        TOP_Y + MINI_H / 2,
                        selectedValue);
  }
  else
  {
    drawRightConnection(MAIN_Y + MAIN_H / 2,
                        TOP_Y + MINI_H / 2,
                        selectedValue);
    if (targetCount >= 2)
    {
      drawRightConnection(MAIN_Y + MAIN_H / 2,
                          BOTTOM_Y + MINI_H / 2,
                          selectedValue);
    }
  }

  // Fuentes o recurso físico de entrada.
  if (block->type == LogicBlockType::DigitalInput)
  {
    char resource[16];
    formatResource(resource, sizeof(resource), *block);
    drawEndpoint(LEFT_X,
                 TOP_Y,
                 resource,
                 "ENTRADA FISICA",
                 selectedValue);
  }
  else
  {
    if (sourceAValid)
    {
      drawMiniBlock(LEFT_X,
                    TOP_Y,
                    block->sourceA,
                    sourceAValue);
    }
    if (sourceBValid)
    {
      drawMiniBlock(LEFT_X,
                    BOTTOM_Y,
                    block->sourceB,
                    sourceBValue);
    }
  }

  // Destinos o recurso físico de salida.
  if (block->type == LogicBlockType::DigitalOutput)
  {
    char resource[16];
    formatResource(resource, sizeof(resource), *block);
    drawEndpoint(RIGHT_X,
                 TOP_Y,
                 resource,
                 "SALIDA FISICA",
                 selectedValue);
  }
  else if (targetCount == 0)
  {
    drawEndpoint(RIGHT_X,
                 TOP_Y,
                 "FIN",
                 "SIN DESTINO",
                 selectedValue);
  }
  else
  {
    drawMiniBlock(RIGHT_X,
                  TOP_Y,
                  targets[0],
                  _runtime->blockValue(targets[0]));
    if (targetCount >= 2)
    {
      drawMiniBlock(RIGHT_X,
                    BOTTOM_Y,
                    targets[1],
                    _runtime->blockValue(targets[1]));
    }
    if (targetCount > 2)
    {
      char more[12];
      std::snprintf(more,
                    sizeof(more),
                    "+%u mas",
                    static_cast<unsigned int>(targetCount - 2U));
      updateTextField(tft,
                      264,
                      119,
                      7,
                      more,
                      COLOR_ACCENT,
                      COLOR_PANEL);
    }
  }

  drawMainBlock(*block, selectedValue);

  _cache.runtimeState = _runtime->state();
  _cache.blockCount = blockCount;
  _cache.selectedIndex = _selectedIndex;
  _cache.selectedValue = selectedValue;

  _cache.sourceA.valid = sourceAValid;
  _cache.sourceA.index = sourceAValid
                             ? block->sourceA
                             : JWPLC_LOGIC_NO_SOURCE;
  _cache.sourceA.value = sourceAValue;

  _cache.sourceB.valid = sourceBValid;
  _cache.sourceB.index = sourceBValid
                             ? block->sourceB
                             : JWPLC_LOGIC_NO_SOURCE;
  _cache.sourceB.value = sourceBValue;

  _cache.targetCount = targetCount;
  for (uint8_t target = 0; target < 2; ++target)
  {
    const bool valid = target < targetCount &&
                       targets[target] != JWPLC_LOGIC_NO_SOURCE;
    _cache.targets[target].valid = valid;
    _cache.targets[target].index = valid
                                       ? targets[target]
                                       : JWPLC_LOGIC_NO_SOURCE;
    _cache.targets[target].value =
        valid ? _runtime->blockValue(targets[target]) : false;
  }

  _cache.valid = true;
}

void RuntimeUIDiagram::drawNoProgram()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, COLOR_PANEL);
  updateTextField(tft,
                  78,
                  82,
                  28,
                  "SIN PROGRAMA CARGADO",
                  COLOR_WARNING,
                  COLOR_PANEL);
  updateTextField(tft,
                  56,
                  101,
                  36,
                  "Use PROGRAMA > PREPARAR",
                  COLOR_MUTED,
                  COLOR_PANEL);
  _selectedCommand = 2;
}

void RuntimeUIDiagram::drawGraphCommands()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t blockCount = _runtime ? _runtime->blockCount() : 0;
  if (blockCount == 0)
  {
    _selectedCommand = 2;
  }

  for (uint8_t command = 0; command < COMMAND_COUNT; ++command)
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

void RuntimeUIDiagram::redrawGraphCommand(uint8_t previousCommand,
                                          uint8_t currentCommand)
{
  if (previousCommand >= COMMAND_COUNT || currentCommand >= COMMAND_COUNT)
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

void RuntimeUIDiagram::handleGraphInput()
{
  const uint16_t blockCount = _runtime->blockCount();

  if (blockCount > 0 && JWPLC_Buttons.pressed(BTN_UP))
  {
    _selectedIndex = _selectedIndex == 0
                         ? static_cast<uint16_t>(blockCount - 1U)
                         : static_cast<uint16_t>(_selectedIndex - 1U);
    JWPLC_Display.notifyActivity();
    drawGraph(true);
  }
  else if (blockCount > 0 && JWPLC_Buttons.pressed(BTN_DOWN))
  {
    _selectedIndex = static_cast<uint16_t>(
        (_selectedIndex + 1U) % blockCount);
    JWPLC_Display.notifyActivity();
    drawGraph(true);
  }

  if (JWPLC_Buttons.pressed(BTN_LEFT) ||
      JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    if (blockCount == 0)
    {
      _selectedCommand = 2;
    }
    else
    {
      const uint8_t previousCommand = _selectedCommand;
      if (JWPLC_Buttons.pressed(BTN_LEFT))
      {
        _selectedCommand = _selectedCommand == 0
                               ? COMMAND_COUNT - 1
                               : static_cast<uint8_t>(_selectedCommand - 1U);
      }
      else
      {
        _selectedCommand = static_cast<uint8_t>(
            (_selectedCommand + 1U) % COMMAND_COUNT);
      }
      JWPLC_Display.notifyActivity();
      redrawGraphCommand(previousCommand, _selectedCommand);
    }
  }

  if (!JWPLC_Buttons.pressed(BTN_OK))
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  switch (_selectedCommand)
  {
  case 0:
    if (blockCount > 0)
    {
      _mode = Mode::Detail;
      _detailValueValid = false;
      invalidateCache();
      drawCurrentMode();
      _fullRedraw = false;
    }
    break;
  case 1:
    _requestedView = RuntimeUIView::Blocks;
    break;
  case 2:
  default:
    _requestedView = RuntimeUIView::Home;
    break;
  }
}

void RuntimeUIDiagram::updateGraphValues(bool force)
{
  if (_runtime == nullptr)
  {
    return;
  }

  const uint16_t blockCount = _runtime->blockCount();
  if (blockCount == 0)
  {
    if (force || !_cache.valid || _cache.blockCount != 0)
    {
      drawGraph(true);
      drawGraphCommands();
    }
    return;
  }

  const bool selectedValue = _runtime->blockValue(_selectedIndex);
  bool changed = force || !_cache.valid ||
                 selectedValue != _cache.selectedValue;

  if (_cache.sourceA.valid)
  {
    changed = changed ||
              _runtime->blockValue(_cache.sourceA.index) !=
                  _cache.sourceA.value;
  }
  if (_cache.sourceB.valid)
  {
    changed = changed ||
              _runtime->blockValue(_cache.sourceB.index) !=
                  _cache.sourceB.value;
  }

  for (uint8_t target = 0; target < 2; ++target)
  {
    if (_cache.targets[target].valid)
    {
      changed = changed ||
                _runtime->blockValue(_cache.targets[target].index) !=
                    _cache.targets[target].value;
    }
  }

  if (changed)
  {
    // La topología permanece igual: se repintan conexiones y nodos sobre sus
    // mismas regiones, sin limpiar toda la pantalla ni los controles.
    drawGraph(false);
  }
}

void RuntimeUIDiagram::drawDetailStatic()
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

  drawFooter(tft, "UP/DN: otro bloque  OK: diagrama  ESC: IDLE");
}

void RuntimeUIDiagram::updateDetailFields(bool force)
{
  const LogicBlockDefinition *block =
      _runtime ? _runtime->blockDefinition(_selectedIndex) : nullptr;
  if (block == nullptr)
  {
    _mode = Mode::Graph;
    normalizeSelection();
    invalidateCache();
    drawCurrentMode();
    return;
  }

  if (force)
  {
    char blockPosition[32];
    std::snprintf(blockPosition,
                  sizeof(blockPosition),
                  "B%02u | %u de %u",
                  static_cast<unsigned int>(_selectedIndex),
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

void RuntimeUIDiagram::handleDetailInput()
{
  const uint16_t blockCount = _runtime->blockCount();

  if ((JWPLC_Buttons.pressed(BTN_UP) ||
       JWPLC_Buttons.pressed(BTN_LEFT)) &&
      blockCount > 0)
  {
    _selectedIndex = _selectedIndex == 0
                         ? static_cast<uint16_t>(blockCount - 1U)
                         : static_cast<uint16_t>(_selectedIndex - 1U);
    _detailValueValid = false;
    JWPLC_Display.notifyActivity();
    updateDetailFields(true);
  }
  else if ((JWPLC_Buttons.pressed(BTN_DOWN) ||
            JWPLC_Buttons.pressed(BTN_RIGHT)) &&
           blockCount > 0)
  {
    _selectedIndex = static_cast<uint16_t>(
        (_selectedIndex + 1U) % blockCount);
    _detailValueValid = false;
    JWPLC_Display.notifyActivity();
    updateDetailFields(true);
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::Graph;
    invalidateCache();
    drawCurrentMode();
    _fullRedraw = false;
  }
}

void RuntimeUIDiagram::drawMainBlock(const LogicBlockDefinition &block,
                                     bool value)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t background = COLOR_SELECTED;
  const uint16_t border = value ? COLOR_OK : COLOR_ACCENT;

  tft.fillRoundRect(MAIN_X, MAIN_Y, MAIN_W, MAIN_H, 5, background);
  tft.drawRoundRect(MAIN_X, MAIN_Y, MAIN_W, MAIN_H, 5, border);
  tft.drawRoundRect(MAIN_X + 2,
                    MAIN_Y + 2,
                    MAIN_W - 4,
                    MAIN_H - 4,
                    4,
                    border);

  char identifier[12];
  std::snprintf(identifier,
                sizeof(identifier),
                "B%02u",
                static_cast<unsigned int>(_selectedIndex));

  char data[20];
  formatMainData(data, sizeof(data), block);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(MAIN_X + 6, MAIN_Y + 5);
  tft.print(identifier);

  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(MAIN_X + 6, MAIN_Y + 17);
  tft.print(blockTypeName(block.type));

  tft.setTextColor(COLOR_MUTED);
  tft.setCursor(MAIN_X + 6, MAIN_Y + 30);
  tft.print(data);

  tft.setTextColor(value ? COLOR_OK : COLOR_TEXT);
  tft.setCursor(MAIN_X + 6, MAIN_Y + 43);
  tft.print(value ? "SALIDA: 1" : "SALIDA: 0");
}

void RuntimeUIDiagram::drawMiniBlock(int16_t x,
                                     int16_t y,
                                     uint16_t blockIndex,
                                     bool value)
{
  const LogicBlockDefinition *block =
      _runtime ? _runtime->blockDefinition(blockIndex) : nullptr;
  if (block == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t background = value ? COLOR_SELECTED : COLOR_BACKGROUND;
  const uint16_t border = value ? COLOR_OK : COLOR_BORDER;

  tft.fillRoundRect(x, y, MINI_W, MINI_H, 3, background);
  tft.drawRoundRect(x, y, MINI_W, MINI_H, 3, border);

  char title[20];
  std::snprintf(title,
                sizeof(title),
                "B%02u %s",
                static_cast<unsigned int>(blockIndex),
                blockTypeShort(block->type));

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(value ? COLOR_ACCENT : COLOR_TEXT);
  tft.setCursor(x + 4, y + 4);
  tft.print(title);

  tft.setTextColor(value ? COLOR_OK : COLOR_MUTED);
  tft.setCursor(x + 4, y + 15);
  tft.print(value ? "VALOR = 1" : "VALOR = 0");
}

void RuntimeUIDiagram::drawEndpoint(int16_t x,
                                    int16_t y,
                                    const char *title,
                                    const char *subtitle,
                                    bool value)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t background = value ? COLOR_SELECTED : COLOR_BACKGROUND;
  const uint16_t border = value ? COLOR_OK : COLOR_BORDER;

  tft.fillRoundRect(x, y, MINI_W, MINI_H, 3, background);
  tft.drawRoundRect(x, y, MINI_W, MINI_H, 3, border);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(value ? COLOR_ACCENT : COLOR_TEXT);
  tft.setCursor(x + 4, y + 4);
  tft.print(title ? title : "-");

  tft.setTextColor(value ? COLOR_OK : COLOR_MUTED);
  tft.setCursor(x + 4, y + 15);
  tft.print(subtitle ? subtitle : "");
}

void RuntimeUIDiagram::drawLeftConnection(int16_t sourceY,
                                          int16_t destinationY,
                                          bool active,
                                          const char *portLabel)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t color = active ? COLOR_OK : COLOR_BORDER;
  const int16_t bendX = 109;

  drawWireSegment(tft, LEFT_X + MINI_W, sourceY, bendX, sourceY, color, active);
  drawWireSegment(tft, bendX, sourceY, bendX, destinationY, color, active);
  drawWireSegment(tft, bendX, destinationY, MAIN_X, destinationY, color, active);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED);
  tft.setCursor(MAIN_X - 8, destinationY - 4);
  tft.print(portLabel ? portLabel : "");
}

void RuntimeUIDiagram::drawRightConnection(int16_t sourceY,
                                           int16_t destinationY,
                                           bool active)
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const uint16_t color = active ? COLOR_OK : COLOR_BORDER;
  const int16_t bendX = 211;

  drawWireSegment(tft, MAIN_X + MAIN_W, sourceY, bendX, sourceY, color, active);
  drawWireSegment(tft, bendX, sourceY, bendX, destinationY, color, active);
  drawWireSegment(tft, bendX, destinationY, RIGHT_X, destinationY, color, active);
}

uint8_t RuntimeUIDiagram::collectTargets(uint16_t sourceIndex,
                                         uint16_t destination[2]) const
{
  if (_runtime == nullptr || destination == nullptr)
  {
    return 0;
  }

  uint8_t count = 0;
  const uint16_t blockCount = _runtime->blockCount();
  for (uint16_t index = 0; index < blockCount; ++index)
  {
    const LogicBlockDefinition *block = _runtime->blockDefinition(index);
    if (block == nullptr || !blockUsesSource(*block, sourceIndex))
    {
      continue;
    }

    if (count < 2)
    {
      destination[count] = index;
    }
    if (count < 255)
    {
      ++count;
    }
  }

  return count;
}

bool RuntimeUIDiagram::blockUsesSource(const LogicBlockDefinition &block,
                                       uint16_t sourceIndex) const
{
  return (expectsSourceA(block.type) && block.sourceA == sourceIndex) ||
         (expectsSourceB(block.type) && block.sourceB == sourceIndex);
}

bool RuntimeUIDiagram::expectsSourceA(LogicBlockType type) const
{
  switch (type)
  {
  case LogicBlockType::DigitalOutput:
  case LogicBlockType::Not:
  case LogicBlockType::And:
  case LogicBlockType::Or:
  case LogicBlockType::SetReset:
  case LogicBlockType::Ton:
    return true;
  case LogicBlockType::DigitalInput:
  default:
    return false;
  }
}

bool RuntimeUIDiagram::expectsSourceB(LogicBlockType type) const
{
  return type == LogicBlockType::And ||
         type == LogicBlockType::Or ||
         type == LogicBlockType::SetReset;
}

void RuntimeUIDiagram::copyProgramName(char *destination,
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

const char *RuntimeUIDiagram::blockTypeName(LogicBlockType type) const
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

const char *RuntimeUIDiagram::blockTypeShort(LogicBlockType type) const
{
  switch (type)
  {
  case LogicBlockType::DigitalInput:
    return "IN";
  case LogicBlockType::DigitalOutput:
    return "OUT";
  case LogicBlockType::SetReset:
    return "S/R";
  default:
    return blockTypeName(type);
  }
}

void RuntimeUIDiagram::formatSource(char *destination,
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
                  "B%02u",
                  static_cast<unsigned int>(source));
  }
}

void RuntimeUIDiagram::formatResource(char *destination,
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

void RuntimeUIDiagram::formatParameter(char *destination,
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

void RuntimeUIDiagram::formatMainData(char *destination,
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
  case LogicBlockType::DigitalOutput:
  case LogicBlockType::Ton:
    formatResource(destination, destinationCapacity, block);
    break;
  case LogicBlockType::SetReset:
    std::snprintf(destination,
                  destinationCapacity,
                  "%s",
                  block.isRetentive() ? "MEMORIA RET" : "MEMORIA");
    break;
  default:
    std::snprintf(destination, destinationCapacity, "LOGICA");
    break;
  }
}

uint16_t RuntimeUIDiagram::runtimeStateColor() const
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

const char *RuntimeUIDiagram::runtimeStateText() const
{
  return _runtime
             ? JWPLC_LogicRuntime::stateName(_runtime->state())
             : "SIN RUNTIME";
}