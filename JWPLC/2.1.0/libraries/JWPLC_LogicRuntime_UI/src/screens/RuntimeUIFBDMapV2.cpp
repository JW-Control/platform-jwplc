#include "RuntimeUIFBDMapV2.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  uint16_t absoluteDistance(int16_t a, int16_t b)
  {
    return static_cast<uint16_t>(a > b ? a - b : b - a);
  }

  void drawOrthogonalWire(Adafruit_ST7789 &tft,
                          int16_t x0,
                          int16_t y0,
                          int16_t x1,
                          int16_t y1,
                          uint16_t color)
  {
    const int16_t middleX = static_cast<int16_t>((x0 + x1) / 2);
    tft.drawFastHLine(x0,
                      y0,
                      static_cast<int16_t>(middleX - x0 + 1),
                      color);

    const int16_t topY = y0 < y1 ? y0 : y1;
    tft.drawFastVLine(middleX,
                      topY,
                      static_cast<int16_t>(absoluteDistance(y0, y1) + 1U),
                      color);

    tft.drawFastHLine(middleX,
                      y1,
                      static_cast<int16_t>(x1 - middleX + 1),
                      color);
  }
}

RuntimeUIFBDMapV2::RuntimeUIFBDMapV2()
    : _model(nullptr),
      _mode(Mode::Map),
      _fullRedraw(true),
      _layoutValid(false),
      _valueCacheValid(false),
      _selectedIndex(0),
      _viewportX(0),
      _viewportY(0),
      _lastValueRefreshMs(0),
      _levels{},
      _nodeX{},
      _nodeY{},
      _valueCache{}
{
}

void RuntimeUIFBDMapV2::attach(RuntimeUIV2ReadModel &model)
{
  _model = &model;
  _selectedIndex = 0;
  _viewportX = 0;
  _viewportY = 0;
  invalidateLayout();
  forceRedraw();
}

void RuntimeUIFBDMapV2::detach()
{
  _model = nullptr;
  _mode = Mode::Map;
  _selectedIndex = 0;
  _viewportX = 0;
  _viewportY = 0;
  invalidateLayout();
}

void RuntimeUIFBDMapV2::enter()
{
  _mode = Mode::Map;
  _lastValueRefreshMs = millis();
  normalizeSelection();
  buildLayout();
  ensureSelectionVisible();
  drawMapStatic();
  drawMap();
  _fullRedraw = false;
}

void RuntimeUIFBDMapV2::refresh(const JWPLC_IOState *io,
                                const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  if (_model == nullptr || !_model->isAttached())
  {
    return;
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
    if (_mode == Mode::Map)
    {
      drawMapStatic();
      drawMap();
    }
    else
    {
      drawDetailStatic();
      drawDetail(true);
    }
    _fullRedraw = false;
  }

  updateHeaderState(JWPLC_Display.tft(), stateText(), stateColor());

  if (_mode == Mode::Map)
  {
    handleMapInput();
  }
  else
  {
    handleDetailInput();
  }

  const uint32_t nowMs = millis();
  if (static_cast<uint32_t>(nowMs - _lastValueRefreshMs) < VALUE_REFRESH_MS)
  {
    return;
  }
  _lastValueRefreshMs = nowMs;

  if (_mode == Mode::Map)
  {
    if (valuesChanged())
    {
      drawMap();
    }
  }
  else
  {
    drawDetail(false);
  }
}

void RuntimeUIFBDMapV2::exit()
{
  _valueCacheValid = false;
}

void RuntimeUIFBDMapV2::forceRedraw()
{
  _fullRedraw = true;
  _valueCacheValid = false;
}

void RuntimeUIFBDMapV2::invalidateLayout()
{
  _layoutValid = false;
  _valueCacheValid = false;
  std::memset(_levels, 0, sizeof(_levels));
  std::memset(_nodeX, 0, sizeof(_nodeX));
  std::memset(_nodeY, 0, sizeof(_nodeY));
}

void RuntimeUIFBDMapV2::buildLayout()
{
  if (_model == nullptr)
  {
    return;
  }

  uint8_t lanes[MAX_LEVELS] = {};
  const uint16_t count = _model->blockCount();

  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    const LogicV2BlockRecord *definition = _model->block(blockIndex);
    uint8_t level = 0;

    if (definition != nullptr)
    {
      for (uint8_t inputIndex = 0;
           inputIndex < definition->inputCount;
           ++inputIndex)
      {
        const LogicV2InputLink *input =
            _model->inputLink(blockIndex, inputIndex);
        if (input == nullptr)
        {
          continue;
        }

        const uint16_t source = input->source();
        if (source < blockIndex)
        {
          const uint8_t candidate =
              _levels[source] >= MAX_LEVELS - 1U
                  ? static_cast<uint8_t>(MAX_LEVELS - 1U)
                  : static_cast<uint8_t>(_levels[source] + 1U);
          if (candidate > level)
          {
            level = candidate;
          }
        }
      }
    }

    _levels[blockIndex] = level;
    const uint8_t lane = lanes[level]++;
    _nodeX[blockIndex] = static_cast<int16_t>(
        WORLD_MARGIN_X + static_cast<int16_t>(level) * COLUMN_STEP);
    _nodeY[blockIndex] = static_cast<int16_t>(
        WORLD_MARGIN_Y + static_cast<int16_t>(lane) * ROW_STEP);
  }

  _layoutValid = true;
  _valueCacheValid = false;
}

void RuntimeUIFBDMapV2::normalizeSelection()
{
  const uint16_t count = _model ? _model->blockCount() : 0;
  if (count == 0)
  {
    _selectedIndex = 0;
  }
  else if (_selectedIndex >= count)
  {
    _selectedIndex = static_cast<uint16_t>(count - 1U);
  }
}

void RuntimeUIFBDMapV2::ensureSelectionVisible()
{
  if (_model == nullptr || _model->blockCount() == 0)
  {
    _viewportX = 0;
    _viewportY = 0;
    return;
  }

  const int16_t nodeWorldX = _nodeX[_selectedIndex];
  const int16_t nodeWorldY = _nodeY[_selectedIndex];
  int16_t relativeX = static_cast<int16_t>(nodeWorldX - _viewportX);
  int16_t relativeY = static_cast<int16_t>(nodeWorldY - _viewportY);

  if (relativeX < KEEP_MARGIN_X)
  {
    _viewportX = nodeWorldX > KEEP_MARGIN_X
                     ? static_cast<int16_t>(nodeWorldX - KEEP_MARGIN_X)
                     : 0;
  }
  else if (relativeX + NODE_W > VIEW_W - KEEP_MARGIN_X)
  {
    _viewportX = static_cast<int16_t>(
        nodeWorldX + NODE_W - (VIEW_W - KEEP_MARGIN_X));
  }

  relativeY = static_cast<int16_t>(nodeWorldY - _viewportY);
  if (relativeY < KEEP_MARGIN_Y)
  {
    _viewportY = nodeWorldY > KEEP_MARGIN_Y
                     ? static_cast<int16_t>(nodeWorldY - KEEP_MARGIN_Y)
                     : 0;
  }
  else if (relativeY + NODE_H > VIEW_H - KEEP_MARGIN_Y)
  {
    _viewportY = static_cast<int16_t>(
        nodeWorldY + NODE_H - (VIEW_H - KEEP_MARGIN_Y));
  }

  if (_viewportX < 0)
  {
    _viewportX = 0;
  }
  if (_viewportY < 0)
  {
    _viewportY = 0;
  }
}

uint16_t RuntimeUIFBDMapV2::nearestByY(const uint16_t *indices,
                                       uint8_t count) const
{
  if (indices == nullptr || count == 0)
  {
    return JWPLC_LOGIC_NO_SOURCE;
  }

  uint16_t selected = indices[0];
  uint16_t bestDistance = absoluteDistance(
      _nodeY[selected],
      _nodeY[_selectedIndex]);

  for (uint8_t index = 1; index < count; ++index)
  {
    const uint16_t candidate = indices[index];
    const uint16_t distance = absoluteDistance(
        _nodeY[candidate],
        _nodeY[_selectedIndex]);
    if (distance < bestDistance)
    {
      selected = candidate;
      bestDistance = distance;
    }
  }

  return selected;
}

bool RuntimeUIFBDMapV2::selectSource()
{
  uint16_t sources[JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK] = {};
  const uint8_t count = _model->collectSources(
      _selectedIndex,
      sources,
      JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK);
  const uint16_t next = nearestByY(sources, count);
  if (next == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = next;
  return true;
}

bool RuntimeUIFBDMapV2::selectConsumer()
{
  uint16_t consumers[16] = {};
  const uint8_t count = _model->collectConsumers(
      _selectedIndex,
      consumers,
      16);
  const uint16_t next = nearestByY(consumers, count);
  if (next == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = next;
  return true;
}

bool RuntimeUIFBDMapV2::selectVertical(bool down)
{
  const uint16_t count = _model->blockCount();
  const int16_t selectedY = _nodeY[_selectedIndex];
  const int16_t selectedX = _nodeX[_selectedIndex];
  uint16_t best = JWPLC_LOGIC_NO_SOURCE;
  uint32_t bestScore = 0xFFFFFFFFUL;

  for (uint16_t candidate = 0; candidate < count; ++candidate)
  {
    if (candidate == _selectedIndex)
    {
      continue;
    }

    const int16_t deltaY = static_cast<int16_t>(
        _nodeY[candidate] - selectedY);
    if ((down && deltaY <= 0) || (!down && deltaY >= 0))
    {
      continue;
    }

    const uint32_t score =
        static_cast<uint32_t>(absoluteDistance(_nodeY[candidate], selectedY)) * 4UL +
        absoluteDistance(_nodeX[candidate], selectedX);
    if (score < bestScore)
    {
      bestScore = score;
      best = candidate;
    }
  }

  if (best == JWPLC_LOGIC_NO_SOURCE)
  {
    return false;
  }
  _selectedIndex = best;
  return true;
}

void RuntimeUIFBDMapV2::handleMapInput()
{
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
    ensureSelectionVisible();
    drawMap();
  }

  if (JWPLC_Buttons.pressed(BTN_OK) &&
      _model->blockCount() > 0)
  {
    JWPLC_Display.notifyActivity();
    _mode = Mode::Detail;
    drawDetailStatic();
    drawDetail(true);
  }
}

void RuntimeUIFBDMapV2::handleDetailInput()
{
  if (!JWPLC_Buttons.pressed(BTN_OK))
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  _mode = Mode::Map;
  drawMapStatic();
  drawMap();
}

void RuntimeUIFBDMapV2::drawMapStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "MAPA FBD");
  updateHeaderState(tft, stateText(), stateColor());
  tft.fillRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, COLOR_PANEL);
  tft.drawRect(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, COLOR_BORDER);
  drawFooter(tft, "L/R: conexion  UP/DN: mapa  OK: detalle");
}

void RuntimeUIFBDMapV2::drawMap()
{
  if (_model == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(VIEW_X + 1,
               VIEW_Y + 1,
               VIEW_W - 2,
               VIEW_H - 2,
               COLOR_PANEL);

  if (_model->blockCount() == 0)
  {
    updateTextField(tft,
                    79,
                    78,
                    27,
                    "SIN PROGRAMA V2",
                    COLOR_WARNING,
                    COLOR_PANEL);
    return;
  }

  drawWires();
  drawNodes();

  char selectedText[24];
  std::snprintf(selectedText,
                sizeof(selectedText),
                "B%02u  X:%d Y:%d",
                static_cast<unsigned>(_selectedIndex),
                static_cast<int>(_viewportX),
                static_cast<int>(_viewportY));
  updateTextField(tft,
                  VIEW_X + 5,
                  VIEW_Y + 4,
                  22,
                  selectedText,
                  COLOR_MUTED,
                  COLOR_PANEL);

  _valueCacheValid = true;
  const uint16_t count = _model->blockCount();
  for (uint16_t index = 0; index < count; ++index)
  {
    _valueCache[index] = _model->blockValue(index);
  }
}

void RuntimeUIFBDMapV2::drawWires()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    const LogicV2BlockRecord *definition = _model->block(consumer);
    if (definition == nullptr)
    {
      continue;
    }

    for (uint8_t input = 0; input < definition->inputCount; ++input)
    {
      drawWire(consumer, input);
    }
  }
}

void RuntimeUIFBDMapV2::drawWire(uint16_t consumerIndex,
                                 uint8_t inputIndex)
{
  const LogicV2BlockRecord *consumer = _model->block(consumerIndex);
  const LogicV2InputLink *input =
      _model->inputLink(consumerIndex, inputIndex);
  if (consumer == nullptr || input == nullptr)
  {
    return;
  }

  const uint16_t sourceIndex = input->source();
  if (sourceIndex >= _model->blockCount())
  {
    return;
  }

  const int16_t sourceX = static_cast<int16_t>(
      screenX(sourceIndex) + NODE_W);
  const int16_t sourceY = static_cast<int16_t>(
      screenY(sourceIndex) + NODE_H / 2);
  const int16_t destinationX = screenX(consumerIndex);
  const int16_t destinationY = static_cast<int16_t>(
      screenY(consumerIndex) + inputPortY(*consumer, inputIndex));

  const int16_t left = sourceX < destinationX ? sourceX : destinationX;
  const int16_t right = sourceX > destinationX ? sourceX : destinationX;
  const int16_t top = sourceY < destinationY ? sourceY : destinationY;
  const int16_t bottom = sourceY > destinationY ? sourceY : destinationY;

  if (right < VIEW_X || left > VIEW_X + VIEW_W ||
      bottom < VIEW_Y || top > VIEW_Y + VIEW_H)
  {
    return;
  }

  const uint16_t color = _model->blockValue(sourceIndex)
                             ? COLOR_OK
                             : COLOR_MUTED;
  drawOrthogonalWire(JWPLC_Display.tft(),
                     sourceX,
                     sourceY,
                     destinationX,
                     destinationY,
                     color);
}

void RuntimeUIFBDMapV2::drawNodes()
{
  const uint16_t count = _model->blockCount();
  for (uint16_t blockIndex = 0; blockIndex < count; ++blockIndex)
  {
    drawNode(blockIndex);
  }
}

void RuntimeUIFBDMapV2::drawNode(uint16_t blockIndex)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    return;
  }

  const int16_t x = screenX(blockIndex);
  const int16_t y = screenY(blockIndex);
  if (!nodeVisible(x, y))
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  const bool active = _model->blockValue(blockIndex);
  const bool selected = blockIndex == _selectedIndex;
  const uint16_t border = selected
                              ? COLOR_WARNING
                              : (active ? COLOR_OK : COLOR_BORDER);

  tft.fillRect(x, y, NODE_W, NODE_H, COLOR_BACKGROUND);
  tft.drawRect(x, y, NODE_W, NODE_H, border);
  if (selected)
  {
    tft.drawRect(x + 1, y + 1, NODE_W - 2, NODE_H - 2, border);
  }

  char title[16];
  char data[16];
  formatBlockTitle(title, sizeof(title), blockIndex);
  formatNodeData(data, sizeof(data), blockIndex);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(x + 4, y + 4);
  tft.print(title);
  tft.setTextColor(active ? COLOR_OK : COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(x + 4, y + 15);
  tft.print(data);

  drawNodePorts(blockIndex, x, y);
}

void RuntimeUIFBDMapV2::drawNodePorts(uint16_t blockIndex,
                                      int16_t x,
                                      int16_t y)
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount;
       ++inputIndex)
  {
    const LogicV2InputLink *input =
        _model->inputLink(blockIndex, inputIndex);
    if (input == nullptr)
    {
      continue;
    }

    const int16_t portY = static_cast<int16_t>(
        y + inputPortY(*definition, inputIndex));
    const bool inputActive = _model->inputValue(blockIndex, inputIndex);
    const uint16_t pinColor = inputActive ? COLOR_OK : COLOR_MUTED;

    tft.fillCircle(x, portY, 1, pinColor);
    if (input->inverted())
    {
      tft.fillCircle(x + 3, portY, 3, COLOR_BACKGROUND);
      tft.drawCircle(x + 3, portY, 2, COLOR_TEXT);
    }

    const uint16_t source = input->source();
    const char *special = nullptr;
    if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
    {
      special = "X";
    }
    else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
    {
      special = "1";
    }
    else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
    {
      special = "0";
    }

    const char *role = _model->inputRole(definition->type, inputIndex);
    if (special != nullptr || (role != nullptr && role[0] != '\0'))
    {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
      tft.setCursor(x + 6, portY - 3);
      if (role != nullptr && role[0] != '\0')
      {
        tft.print(role);
      }
      else
      {
        tft.print(special);
      }
    }
  }

  const int16_t outputY = static_cast<int16_t>(y + NODE_H / 2);
  tft.fillCircle(x + NODE_W,
                 outputY,
                 1,
                 _model->blockValue(blockIndex) ? COLOR_OK : COLOR_MUTED);
}

void RuntimeUIFBDMapV2::drawDetailStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "DETALLE FBD");
  updateHeaderState(tft, stateText(), stateColor());
  drawPanel(tft, 4, 28, 312, 118, "BLOQUE SELECCIONADO");

  drawFieldLabel(tft, 12, 44, "Bloque:");
  drawFieldLabel(tft, 12, 59, "Tipo:");
  drawFieldLabel(tft, 12, 74, "Valor:");
  drawFieldLabel(tft, 12, 89, "Entradas:");
  drawFieldLabel(tft, 168, 44, "Recurso:");
  drawFieldLabel(tft, 168, 59, "Parametro:");
  drawFieldLabel(tft, 168, 74, "TON:");
  drawFieldLabel(tft, 168, 89, "Transc.:");
  drawFieldLabel(tft, 168, 104, "Restante:");
  drawFieldLabel(tft, 12, 108, "Conexiones:");
  drawFooter(tft, "OK: volver al mapa  ESC: IDLE");

  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount && inputIndex < 4;
       ++inputIndex)
  {
    char source[24];
    formatInputSource(source,
                      sizeof(source),
                      _selectedIndex,
                      inputIndex);
    const int16_t x = inputIndex % 2 == 0 ? 12 : 164;
    const int16_t y = inputIndex < 2 ? 121 : 134;
    updateTextField(tft,
                    x,
                    y,
                    23,
                    source,
                    COLOR_TEXT,
                    COLOR_PANEL);
  }
}

void RuntimeUIFBDMapV2::drawDetail(bool force)
{
  (void)force;
  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  char value[32];

  std::snprintf(value,
                sizeof(value),
                "B%02u",
                static_cast<unsigned>(_selectedIndex));
  updateTextField(tft, 60, 44, 12, value);
  updateTextField(tft,
                  60,
                  59,
                  16,
                  _model->typeName(definition->type));
  updateTextField(tft,
                  60,
                  74,
                  12,
                  _model->blockValue(_selectedIndex) ? "TRUE" : "FALSE",
                  _model->blockValue(_selectedIndex) ? COLOR_OK : COLOR_MUTED);

  std::snprintf(value,
                sizeof(value),
                "%u",
                static_cast<unsigned>(definition->inputCount));
  updateTextField(tft, 60, 89, 8, value);

  formatResource(value, sizeof(value), *definition);
  updateTextField(tft, 224, 44, 13, value);

  if (definition->type == LogicV2BlockType::Ton)
  {
    std::snprintf(value,
                  sizeof(value),
                  "%lu ms",
                  static_cast<unsigned long>(definition->parameter));
  }
  else
  {
    std::snprintf(value,
                  sizeof(value),
                  "%lu",
                  static_cast<unsigned long>(definition->parameter));
  }
  updateTextField(tft, 224, 59, 13, value);

  if (_model->isTon(_selectedIndex))
  {
    updateTextField(tft,
                    224,
                    74,
                    13,
                    _model->tonTiming(_selectedIndex)
                        ? "CONTANDO"
                        : (_model->blockValue(_selectedIndex) ? "LISTO" : "IDLE"),
                    _model->tonTiming(_selectedIndex) ? COLOR_WARNING : COLOR_TEXT);

    const uint32_t nowMs = millis();
    std::snprintf(value,
                  sizeof(value),
                  "%lu ms",
                  static_cast<unsigned long>(
                      _model->tonElapsedMs(_selectedIndex, nowMs)));
    updateTextField(tft, 224, 89, 13, value);
    std::snprintf(value,
                  sizeof(value),
                  "%lu ms",
                  static_cast<unsigned long>(
                      _model->tonRemainingMs(_selectedIndex, nowMs)));
    updateTextField(tft, 224, 104, 13, value);
  }
  else
  {
    updateTextField(tft, 224, 74, 13, "-");
    updateTextField(tft, 224, 89, 13, "-");
    updateTextField(tft, 224, 104, 13, "-");
  }
}

bool RuntimeUIFBDMapV2::valuesChanged()
{
  const uint16_t count = _model->blockCount();
  bool changed = !_valueCacheValid;

  for (uint16_t index = 0; index < count; ++index)
  {
    const bool value = _model->blockValue(index);
    if (!_valueCacheValid || _valueCache[index] != value)
    {
      changed = true;
      _valueCache[index] = value;
    }
  }

  _valueCacheValid = true;
  return changed;
}

bool RuntimeUIFBDMapV2::nodeVisible(int16_t x, int16_t y) const
{
  return x + NODE_W >= VIEW_X &&
         x <= VIEW_X + VIEW_W &&
         y + NODE_H >= VIEW_Y &&
         y <= VIEW_Y + VIEW_H;
}

int16_t RuntimeUIFBDMapV2::screenX(uint16_t blockIndex) const
{
  return static_cast<int16_t>(
      VIEW_X + _nodeX[blockIndex] - _viewportX);
}

int16_t RuntimeUIFBDMapV2::screenY(uint16_t blockIndex) const
{
  return static_cast<int16_t>(
      VIEW_Y + _nodeY[blockIndex] - _viewportY);
}

int16_t RuntimeUIFBDMapV2::inputPortY(
    const LogicV2BlockRecord &block,
    uint8_t inputIndex) const
{
  if (block.inputCount <= 1)
  {
    return NODE_H / 2;
  }

  return static_cast<int16_t>(
      3 + (static_cast<uint16_t>(inputIndex) * (NODE_H - 7U)) /
              (block.inputCount - 1U));
}

void RuntimeUIFBDMapV2::formatBlockTitle(char *destination,
                                         size_t capacity,
                                         uint16_t blockIndex) const
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  std::snprintf(destination,
                capacity,
                "B%02u %s",
                static_cast<unsigned>(blockIndex),
                definition ? _model->typeShort(definition->type) : "?");
}

void RuntimeUIFBDMapV2::formatNodeData(char *destination,
                                       size_t capacity,
                                       uint16_t blockIndex) const
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  if (definition == nullptr)
  {
    std::snprintf(destination, capacity, "-");
    return;
  }

  if (definition->type == LogicV2BlockType::DigitalInput)
  {
    std::snprintf(destination,
                  capacity,
                  "I0.%u",
                  static_cast<unsigned>(definition->resource));
  }
  else if (definition->type == LogicV2BlockType::DigitalOutput)
  {
    std::snprintf(destination,
                  capacity,
                  "Q0.%u",
                  static_cast<unsigned>(definition->resource));
  }
  else if (definition->type == LogicV2BlockType::Ton)
  {
    std::snprintf(destination,
                  capacity,
                  "%lums",
                  static_cast<unsigned long>(definition->parameter));
  }
  else if (definition->inputCount > 0)
  {
    std::snprintf(destination,
                  capacity,
                  "%u IN",
                  static_cast<unsigned>(definition->inputCount));
  }
  else
  {
    std::snprintf(destination,
                  capacity,
                  "%s",
                  _model->blockValue(blockIndex) ? "TRUE" : "FALSE");
  }
}

void RuntimeUIFBDMapV2::formatResource(
    char *destination,
    size_t capacity,
    const LogicV2BlockRecord &block) const
{
  if (block.type == LogicV2BlockType::DigitalInput)
  {
    std::snprintf(destination,
                  capacity,
                  "I0.%u",
                  static_cast<unsigned>(block.resource));
  }
  else if (block.type == LogicV2BlockType::DigitalOutput)
  {
    std::snprintf(destination,
                  capacity,
                  "Q0.%u",
                  static_cast<unsigned>(block.resource));
  }
  else
  {
    std::snprintf(destination, capacity, "-");
  }
}

void RuntimeUIFBDMapV2::formatInputSource(
    char *destination,
    size_t capacity,
    uint16_t blockIndex,
    uint8_t inputIndex) const
{
  const LogicV2BlockRecord *definition = _model->block(blockIndex);
  const LogicV2InputLink *input = _model->inputLink(blockIndex, inputIndex);
  if (definition == nullptr || input == nullptr)
  {
    std::snprintf(destination, capacity, "IN%u: -", inputIndex + 1U);
    return;
  }

  const uint16_t source = input->source();
  const char *role = _model->inputRole(definition->type, inputIndex);
  char sourceText[12];

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(sourceText, sizeof(sourceText), "X");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(sourceText, sizeof(sourceText), "HI");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(sourceText, sizeof(sourceText), "LO");
  }
  else
  {
    std::snprintf(sourceText,
                  sizeof(sourceText),
                  "B%02u",
                  static_cast<unsigned>(source));
  }

  std::snprintf(destination,
                capacity,
                "%s%u%s: %s%s",
                (role != nullptr && role[0] != '\0') ? role : "IN",
                (role != nullptr && role[0] != '\0') ? 0U : inputIndex + 1U,
                input->inverted() ? "!" : "",
                sourceText,
                _model->inputValue(blockIndex, inputIndex) ? "=1" : "=0");
}

const char *RuntimeUIFBDMapV2::stateText() const
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

uint16_t RuntimeUIFBDMapV2::stateColor() const
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
