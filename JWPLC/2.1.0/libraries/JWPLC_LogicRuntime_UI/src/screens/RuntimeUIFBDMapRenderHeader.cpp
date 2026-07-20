#include "RuntimeUIFBDMap.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

void RuntimeUIFBDMap::invalidateHeaderCache()
{
  _headerCacheValid = false;
  _headerTitleCache[0] = '\0';
  _headerLine1Cache[0] = '\0';
  _headerLine2Cache[0] = '\0';
  _headerStateCache = LogicV2EngineState::Empty;
}

const char *RuntimeUIFBDMap::titleForView(View view) const
{
  switch (view)
  {
  case View::Map:
    return "MAPA FBD";
  case View::Detail:
    return "DETALLE";
  case View::EditInput:
    return "EDITAR IN";
  case View::EditTon:
    return "EDITAR T";
  case View::AddType:
    return "NUEVO BLOQUE";
  case View::AddConfig:
    return "CONFIGURAR";
  case View::AddSource:
    return "ELEGIR FUENTE";
  case View::AddResource:
    return "ELEGIR I/O";
  case View::AddTon:
    return "EDITAR T";
  default:
    return "FBD";
  }
}

void RuntimeUIFBDMap::buildHeaderContext(char *line1,
                                         size_t line1Capacity,
                                         char *line2,
                                         size_t line2Capacity) const
{
  if (line1 == nullptr || line1Capacity == 0 ||
      line2 == nullptr || line2Capacity == 0)
  {
    return;
  }
  line1[0] = '\0';
  line2[0] = '\0';

  if (_view == View::AddType)
  {
    std::snprintf(line1, line1Capacity, "TIPO");
    std::snprintf(line2, line2Capacity, "%s", addTypeText(_addType));
    return;
  }
  if (_view == View::AddConfig ||
      _view == View::AddSource ||
      _view == View::AddResource ||
      _view == View::AddTon)
  {
    std::snprintf(line1, line1Capacity, "%s", addTypeText(_addType));
    if (_view == View::AddSource)
    {
      std::snprintf(line2,
                    line2Capacity,
                    "FUENTE %u",
                    static_cast<unsigned>(_addEditingSourceIndex + 1U));
    }
    else if (_view == View::AddResource)
    {
      std::snprintf(line2, line2Capacity, "RECURSO");
    }
    else if (_view == View::AddTon)
    {
      std::snprintf(line2, line2Capacity, "PARAM T");
    }
    else
    {
      std::snprintf(line2, line2Capacity, "B%02u NUEVO",
                    static_cast<unsigned>(_model != nullptr
                                              ? _model->blockCount()
                                              : 0));
    }
    return;
  }

  if (_view == View::Map && _addNodeSelected)
  {
    std::snprintf(line1, line1Capacity, "NUEVO BLOQUE");
    std::snprintf(line2, line2Capacity, "+");
    return;
  }

  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (_model == nullptr || definition == nullptr)
  {
    std::snprintf(line1, line1Capacity, "SIN BLOQUES");
    return;
  }

  std::snprintf(line1,
                line1Capacity,
                "B%02u %s",
                static_cast<unsigned>(_selectedIndex),
                _model->typeShort(definition->type));

  if (_view == View::Map)
  {
    std::snprintf(line2,
                  line2Capacity,
                  "%s",
                  _model->blockValue(_selectedIndex) ? "ON" : "OFF");
    return;
  }

  if (_view == View::EditTon)
  {
    std::snprintf(line2, line2Capacity, "PARAM T");
    return;
  }

  if (_view == View::EditInput)
  {
    const char *role = _model->inputRole(definition->type, _detailInputIndex);
    if (role != nullptr && role[0] != '\0')
    {
      std::snprintf(line2, line2Capacity, "EDIT %s", role);
    }
    else
    {
      std::snprintf(line2,
                    line2Capacity,
                    "EDIT IN%u/%u",
                    static_cast<unsigned>(_detailInputIndex + 1U),
                    static_cast<unsigned>(definition->inputCount));
    }
    return;
  }

  if (_detailFocus == DetailFocus::Parameters &&
      definition->type == LogicV2BlockType::Ton)
  {
    std::snprintf(line2, line2Capacity, "PARAM T");
    return;
  }

  if (definition->inputCount > 0)
  {
    const char *role = _model->inputRole(definition->type, _detailInputIndex);
    if (role != nullptr && role[0] != '\0')
    {
      std::snprintf(line2, line2Capacity, "%s", role);
    }
    else
    {
      std::snprintf(line2,
                    line2Capacity,
                    "IN%u/%u",
                    static_cast<unsigned>(_detailInputIndex + 1U),
                    static_cast<unsigned>(definition->inputCount));
    }
  }
  else
  {
    std::snprintf(line2, line2Capacity, "SIN ENTRADAS");
  }
}

const char *RuntimeUIFBDMap::stateText() const
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

uint16_t RuntimeUIFBDMap::stateColor() const
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

void RuntimeUIFBDMap::drawUnifiedHeader(bool force)
{
  char title[16];
  char line1[24];
  char line2[24];
  std::snprintf(title, sizeof(title), "%s", titleForView(_view));
  buildHeaderContext(line1, sizeof(line1), line2, sizeof(line2));

  const LogicV2EngineState state =
      _model != nullptr ? _model->state() : LogicV2EngineState::Empty;
  const bool titleChanged = !_headerCacheValid ||
                            std::strcmp(_headerTitleCache, title) != 0;
  const bool line1Changed = !_headerCacheValid ||
                            std::strcmp(_headerLine1Cache, line1) != 0;
  const bool line2Changed = !_headerCacheValid ||
                            std::strcmp(_headerLine2Cache, line2) != 0;
  const bool stateChanged = !_headerCacheValid ||
                            _headerStateCache != state;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  if (force || titleChanged)
  {
    tft.fillRect(HEADER_TITLE_X, 0, HEADER_TITLE_W, 23, COLOR_PANEL);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.setCursor(6, 5);
    tft.print(title);
  }

  if (force || line1Changed)
  {
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_CONTEXT_LINE1_Y,
                    HEADER_CONTEXT_COLUMNS,
                    line1,
                    COLOR_MUTED,
                    COLOR_PANEL);
  }
  if (force || line2Changed)
  {
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_CONTEXT_LINE2_Y,
                    HEADER_CONTEXT_COLUMNS,
                    line2,
                    COLOR_MUTED,
                    COLOR_PANEL);
  }
  if (force || stateChanged)
  {
    updateHeaderState(tft, stateText(), stateColor());
  }
  if (force || titleChanged)
  {
    tft.drawFastHLine(0, 23, SCREEN_W, COLOR_BORDER);
  }

  std::snprintf(_headerTitleCache, sizeof(_headerTitleCache), "%s", title);
  std::snprintf(_headerLine1Cache, sizeof(_headerLine1Cache), "%s", line1);
  std::snprintf(_headerLine2Cache, sizeof(_headerLine2Cache), "%s", line2);
  _headerStateCache = state;
  _headerCacheValid = true;
}

void RuntimeUIFBDMap::clearMapArea()
{
  JWPLC_Display.tft().fillRect(MAP_X, MAP_Y, MAP_W, MAP_H, COLOR_PANEL);
  JWPLC_Display.tft().drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

bool RuntimeUIFBDMap::valuesChanged()
{
  if (_model == nullptr)
  {
    return false;
  }
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

void RuntimeUIFBDMap::cacheValues()
{
  if (_model == nullptr)
  {
    return;
  }
  const uint16_t count = _model->blockCount();
  for (uint16_t index = 0; index < count; ++index)
  {
    _valueCache[index] = _model->blockValue(index);
  }
  _valueCacheValid = true;
}

void RuntimeUIFBDMap::formatBlockId(char *destination,
                                    size_t capacity,
                                    uint16_t blockIndex) const
{
  std::snprintf(destination,
                capacity,
                "B%02u",
                static_cast<unsigned>(blockIndex));
}

void RuntimeUIFBDMap::formatMapLine2(char *destination,
                                     size_t capacity,
                                     uint16_t blockIndex) const
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(blockIndex) : nullptr;
  if (definition == nullptr)
  {
    std::snprintf(destination, capacity, "?");
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
  else
  {
    std::snprintf(destination,
                  capacity,
                  "%s",
                  _model->typeShort(definition->type));
  }
}

const char *RuntimeUIFBDMap::detailSymbol(LogicV2BlockType type) const
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
