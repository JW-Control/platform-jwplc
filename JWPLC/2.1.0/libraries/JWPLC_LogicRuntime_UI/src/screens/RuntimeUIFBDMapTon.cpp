#include "RuntimeUIFBDMap.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t TON_FIELD_X0 = 10;
static constexpr int16_t TON_FIELD_X1 = 108;
static constexpr int16_t TON_FIELD_X2 = 206;
static constexpr int16_t TON_FIELD_W0 = 94;
static constexpr int16_t TON_FIELD_W1 = 94;
static constexpr int16_t TON_FIELD_W2 = 104;
static constexpr int16_t ELAPSED_VALUE_X = 88;
static constexpr int16_t ELAPSED_VALUE_Y = 72;
static constexpr uint8_t ELAPSED_VALUE_COLUMNS = 12;
}

const char *RuntimeUIFBDMap::tonBaseText(TonBase base)
{
  switch (base)
  {
  case TonBase::Minutes:
    return "m";
  case TonBase::Hours:
    return "h";
  case TonBase::Seconds:
  default:
    return "s";
  }
}

const char *RuntimeUIFBDMap::tonMajorLabel(TonBase base)
{
  switch (base)
  {
  case TonBase::Minutes:
    return "MIN";
  case TonBase::Hours:
    return "HORA";
  case TonBase::Seconds:
  default:
    return "SEG";
  }
}

const char *RuntimeUIFBDMap::tonMinorLabel(TonBase base)
{
  return base == TonBase::Seconds ? "CENT" :
         base == TonBase::Minutes ? "SEG" : "MIN";
}

uint32_t RuntimeUIFBDMap::tonMinorMaximum(TonBase base)
{
  return base == TonBase::Seconds ? 99UL : 59UL;
}

uint16_t RuntimeUIFBDMap::tonResource(TonBase base)
{
  return static_cast<uint16_t>(base);
}

RuntimeUIFBDMap::TonBase RuntimeUIFBDMap::tonBaseFromResource(
    uint16_t resource)
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

RuntimeUIFBDMap::TonBase RuntimeUIFBDMap::effectiveTonBase(
    uint32_t milliseconds,
    uint16_t resource)
{
  const TonBase requested = tonBaseFromResource(resource);
  const auto representable = [milliseconds](TonBase base) -> bool
  {
    const uint32_t quantum = base == TonBase::Seconds ? 10UL :
                             base == TonBase::Minutes ? 1000UL : 60000UL;
    const uint32_t maximum = base == TonBase::Seconds
                                 ? 99990UL
                                 : base == TonBase::Minutes
                                       ? 5999000UL
                                       : 359940000UL;
    return milliseconds <= maximum && milliseconds % quantum == 0;
  };
  if (representable(requested))
  {
    return requested;
  }
  if (representable(TonBase::Seconds))
  {
    return TonBase::Seconds;
  }
  if (representable(TonBase::Minutes))
  {
    return TonBase::Minutes;
  }
  return TonBase::Hours;
}

uint32_t RuntimeUIFBDMap::encodeTonMilliseconds(uint32_t major,
                                                 uint32_t minor,
                                                 TonBase base)
{
  switch (base)
  {
  case TonBase::Minutes:
    return major * 60000UL + minor * 1000UL;
  case TonBase::Hours:
    return major * 3600000UL + minor * 60000UL;
  case TonBase::Seconds:
  default:
    return major * 1000UL + minor * 10UL;
  }
}

void RuntimeUIFBDMap::decodeTonMilliseconds(uint32_t milliseconds,
                                            TonBase base,
                                            uint32_t &major,
                                            uint32_t &minor)
{
  uint32_t total = 0;
  uint32_t radix = 60UL;
  if (base == TonBase::Seconds)
  {
    total = milliseconds / 10UL;
    radix = 100UL;
  }
  else if (base == TonBase::Minutes)
  {
    total = milliseconds / 1000UL;
  }
  else
  {
    total = milliseconds / 60000UL;
  }
  const uint32_t maximumUnits = 99UL * radix + (radix - 1UL);
  if (total > maximumUnits)
  {
    total = maximumUnits;
  }
  major = total / radix;
  minor = total % radix;
}

void RuntimeUIFBDMap::formatTon(uint32_t major,
                                uint32_t minor,
                                TonBase base,
                                char *destination,
                                size_t capacity)
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }
  std::snprintf(destination,
                capacity,
                "%02lu:%02lu%s",
                static_cast<unsigned long>(major),
                static_cast<unsigned long>(minor),
                tonBaseText(base));
}

void RuntimeUIFBDMap::formatTonMilliseconds(uint32_t milliseconds,
                                            uint16_t resource,
                                            char *destination,
                                            size_t capacity)
{
  const TonBase base = effectiveTonBase(milliseconds, resource);
  uint32_t major = 0;
  uint32_t minor = 0;
  decodeTonMilliseconds(milliseconds, base, major, minor);
  formatTon(major, minor, base, destination, capacity);
}

bool RuntimeUIFBDMap::beginTonEdit()
{
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(_selectedIndex) : nullptr;
  if (definition == nullptr ||
      definition->type != LogicV2BlockType::Ton ||
      !_editSession.begin())
  {
    return false;
  }

  _tonOriginalMs = definition->parameter;
  _tonOriginalResource = definition->resource;
  _tonBase = effectiveTonBase(definition->parameter, definition->resource);
  decodeTonMilliseconds(definition->parameter,
                        _tonBase,
                        _tonMajor,
                        _tonMinor);
  _tonField = TonField::Major;
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  _tonElapsedCache[0] = '\0';
  _tonFooterDefaultVisible = false;
  _feedback = Feedback::None;
  _awaitingApply = false;
  return true;
}

void RuntimeUIFBDMap::cancelTonEdit()
{
  if (_editSession.active())
  {
    _editSession.cancel();
  }
  _feedback = Feedback::None;
  _awaitingApply = false;
  _detailFocus = DetailFocus::Parameters;
  setView(View::Detail, true);
  gateInputUntilRelease(false);
}

void RuntimeUIFBDMap::handleEditTon()
{
  if (_awaitingApply)
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelTonEdit();
    return;
  }
  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const uint32_t parameter = encodeTonMilliseconds(_tonMajor,
                                                     _tonMinor,
                                                     _tonBase);
    JWPLC_Display.notifyActivity();
    if (parameter < TON_MINIMUM_MS)
    {
      drawTonFooter("T MINIMO 00:02s", COLOR_WARNING);
      _tonFooterDefaultVisible = false;
      return;
    }
    const bool prepared =
        _editSession.setBlockParameter(_selectedIndex, parameter) &&
        _editSession.setBlockResource(_selectedIndex, tonResource(_tonBase));
    const bool valid = prepared &&
        _editSession.validate() == LogicV2PrototypeError::None;
    if (!valid)
    {
      drawTonFooter("PARAMETRO NO VALIDO", COLOR_ERROR);
      _tonFooterDefaultVisible = false;
      return;
    }
    _awaitingApply = true;
    _feedback = Feedback::Applying;
    _applyContext = ApplyContext::EditTon;
    _applyRequested = true;
    gateInputUntilRelease(false);
    drawTonFooter("APLICANDO CAMBIOS...", COLOR_WARNING);
    _tonFooterDefaultVisible = false;
    return;
  }

  const TonBase previousBase = _tonBase;
  const TonField previousField = _tonField;
  bool changed = false;
  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    uint8_t field = static_cast<uint8_t>(_tonField);
    field = field == 0 ? 2U : static_cast<uint8_t>(field - 1U);
    _tonField = static_cast<TonField>(field);
    changed = true;
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    _tonField = static_cast<TonField>(
        (static_cast<uint8_t>(_tonField) + 1U) % 3U);
    changed = true;
  }
  else if (_tonField == TonField::Base)
  {
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
    if (!up && !down)
    {
      return;
    }
    uint8_t base = static_cast<uint8_t>(_tonBase);
    base = down
               ? static_cast<uint8_t>((base + 1U) % 3U)
               : (base == 0 ? 2U : static_cast<uint8_t>(base - 1U));
    _tonBase = static_cast<TonBase>(base);
    if (_tonMinor > tonMinorMaximum(_tonBase))
    {
      _tonMinor = tonMinorMaximum(_tonBase);
    }
    changed = true;
  }
  else if (_tonField == TonField::Major)
  {
    changed = JWPLC_Buttons.applyAxis(
        &_tonMajor,
        0UL,
        99UL,
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }
  else if (_tonField == TonField::Minor)
  {
    changed = JWPLC_Buttons.applyAxis(
        &_tonMinor,
        0UL,
        tonMinorMaximum(_tonBase),
        BTN_DOWN,
        BTN_UP,
        false,
        true);
  }

  if (!changed)
  {
    return;
  }

  JWPLC_Display.notifyActivity();
  drawTonFields(false);
  if (previousBase != _tonBase)
  {
    drawTonElapsed(true);
  }
  if (!_tonFooterDefaultVisible)
  {
    drawTonFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
    _tonFooterDefaultVisible = true;
  }
  if (previousField != _tonField)
  {
    drawUnifiedHeader(false);
  }
}

void RuntimeUIFBDMap::drawEditTonFull(bool clearInterior)
{
  if (clearInterior)
  {
    clearMapArea();
  }
  _tonFieldsCacheValid = false;
  _tonElapsedCacheValid = false;
  _tonFooterDefaultVisible = false;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  char actual[20];
  formatTonMilliseconds(_tonOriginalMs,
                        _tonOriginalResource,
                        actual,
                        sizeof(actual));
  drawFieldLabel(tft,
                 22,
                 50,
                 "T ACTUAL",
                 COLOR_TEXT,
                 COLOR_PANEL);
  updateTextField(tft,
                  88,
                  50,
                  12,
                  actual,
                  COLOR_TEXT,
                  COLOR_PANEL);
  drawFieldLabel(tft,
                 22,
                 ELAPSED_VALUE_Y,
                 "Ta LECTURA",
                 COLOR_MUTED,
                 COLOR_PANEL);
  drawTonElapsed(true);
  drawTonFields(true);
  drawTonFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
  _tonFooterDefaultVisible = true;
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMap::drawTonFields(bool force)
{
  if (force || !_tonFieldsCacheValid || _tonBaseCache != _tonBase)
  {
    drawTonFieldFull(TonField::Major);
    drawTonFieldFull(TonField::Minor);
    drawTonFieldFull(TonField::Base);
  }
  else
  {
    const bool focusChanged = _tonFieldCache != _tonField;
    if (focusChanged)
    {
      drawTonFieldFull(_tonFieldCache);
      drawTonFieldFull(_tonField);
    }
    if (_tonMajorCache != _tonMajor &&
        (!focusChanged ||
         (_tonFieldCache != TonField::Major && _tonField != TonField::Major)))
    {
      drawTonFieldValueOnly(TonField::Major);
    }
    if (_tonMinorCache != _tonMinor &&
        (!focusChanged ||
         (_tonFieldCache != TonField::Minor && _tonField != TonField::Minor)))
    {
      drawTonFieldValueOnly(TonField::Minor);
    }
  }
  _tonMajorCache = _tonMajor;
  _tonMinorCache = _tonMinor;
  _tonBaseCache = _tonBase;
  _tonFieldCache = _tonField;
  _tonFieldsCacheValid = true;
}

void RuntimeUIFBDMap::drawTonFieldFull(TonField field)
{
  int16_t x = TON_FIELD_X0;
  int16_t width = TON_FIELD_W0;
  const char *label = tonMajorLabel(_tonBase);
  char value[10];
  if (field == TonField::Minor)
  {
    x = TON_FIELD_X1;
    width = TON_FIELD_W1;
    label = tonMinorLabel(_tonBase);
    std::snprintf(value,
                  sizeof(value),
                  "<%02u>",
                  static_cast<unsigned>(_tonMinor));
  }
  else if (field == TonField::Base)
  {
    x = TON_FIELD_X2;
    width = TON_FIELD_W2;
    label = "BASE";
    std::snprintf(value, sizeof(value), "<%s>", tonBaseText(_tonBase));
  }
  else
  {
    std::snprintf(value,
                  sizeof(value),
                  "<%02u>",
                  static_cast<unsigned>(_tonMajor));
  }
  char full[24];
  std::snprintf(full, sizeof(full), "%s %s", label, value);
  drawMenuButton(JWPLC_Display.tft(),
                 x,
                 FIELD_Y,
                 width,
                 FIELD_H,
                 full,
                 _tonField == field);
}

void RuntimeUIFBDMap::drawTonFieldValueOnly(TonField field)
{
  int16_t x = TON_FIELD_X0;
  int16_t width = TON_FIELD_W0;
  const char *label = tonMajorLabel(_tonBase);
  char value[10];
  if (field == TonField::Minor)
  {
    x = TON_FIELD_X1;
    width = TON_FIELD_W1;
    label = tonMinorLabel(_tonBase);
    std::snprintf(value,
                  sizeof(value),
                  "<%02u>",
                  static_cast<unsigned>(_tonMinor));
  }
  else if (field == TonField::Base)
  {
    x = TON_FIELD_X2;
    width = TON_FIELD_W2;
    label = "BASE";
    std::snprintf(value, sizeof(value), "<%s>", tonBaseText(_tonBase));
  }
  else
  {
    std::snprintf(value,
                  sizeof(value),
                  "<%02u>",
                  static_cast<unsigned>(_tonMajor));
  }

  char full[24];
  std::snprintf(full, sizeof(full), "%s %s", label, value);
  const int16_t textY = static_cast<int16_t>(FIELD_Y + (FIELD_H - 8) / 2);
  const int16_t fullWidth = static_cast<int16_t>(std::strlen(full) * 6U);
  const int16_t textX = static_cast<int16_t>(x + (width - fullWidth) / 2);
  const int16_t valueX = static_cast<int16_t>(
      textX + (std::strlen(label) + 1U) * 6U);
  const int16_t valueWidth = static_cast<int16_t>(std::strlen(value) * 6U);
  const bool selected = _tonField == field;
  const uint16_t background = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t foreground = selected ? COLOR_ACCENT : COLOR_TEXT;
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(valueX, textY, valueWidth, 8, background);
  tft.setTextSize(1);
  tft.setTextColor(foreground, background);
  tft.setCursor(valueX, textY);
  tft.print(value);
}

void RuntimeUIFBDMap::drawTonElapsed(bool force)
{
  if (_model == nullptr || !_model->isTon(_selectedIndex))
  {
    return;
  }
  const uint32_t nowMs = millis();
  uint32_t major = 0;
  uint32_t minor = 0;
  decodeTonMilliseconds(_model->tonElapsedMs(_selectedIndex, nowMs),
                        _tonBase,
                        major,
                        minor);
  char elapsed[20];
  formatTon(major, minor, _tonBase, elapsed, sizeof(elapsed));
  const bool colorOn = _model->tonTiming(_selectedIndex) ||
                       _model->blockValue(_selectedIndex);
  if (!force && _tonElapsedCacheValid &&
      _tonElapsedColorOn == colorOn &&
      std::strcmp(_tonElapsedCache, elapsed) == 0)
  {
    return;
  }
  updateTextField(JWPLC_Display.tft(),
                  ELAPSED_VALUE_X,
                  ELAPSED_VALUE_Y,
                  ELAPSED_VALUE_COLUMNS,
                  elapsed,
                  colorOn ? COLOR_OK : COLOR_MUTED,
                  COLOR_PANEL);
  _tonElapsedColorOn = colorOn;
  std::snprintf(_tonElapsedCache,
                sizeof(_tonElapsedCache),
                "%s",
                elapsed);
  _tonElapsedCacheValid = true;
}

void RuntimeUIFBDMap::drawTonFooter(const char *text, uint16_t color)
{
  updateTextField(JWPLC_Display.tft(),
                  78,
                  154,
                  28,
                  text,
                  color,
                  COLOR_PANEL);
}
