#include "RuntimeUIFBDMapActiveRenderer.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t PANEL_X_ACTIVE = 2;
static constexpr int16_t PANEL_Y_ACTIVE = 27;
static constexpr int16_t PANEL_W_ACTIVE = 316;
static constexpr int16_t PANEL_H_ACTIVE = 142;

static constexpr int16_t HEADER_INFO_X_ACTIVE = 122;
static constexpr int16_t HEADER_INFO_LINE1_Y_ACTIVE = 3;
static constexpr int16_t HEADER_INFO_LINE2_Y_ACTIVE = 12;
static constexpr uint8_t HEADER_INFO_COLUMNS_ACTIVE = 16;

static constexpr int16_t ELAPSED_VALUE_X_ACTIVE = 88;
static constexpr int16_t ELAPSED_VALUE_Y_ACTIVE = 72;
static constexpr uint8_t ELAPSED_VALUE_COLUMNS_ACTIVE = 12;

const char *stateTextActive(LogicV2EngineState state)
{
  switch (state)
  {
  case LogicV2EngineState::Running:
    return "RUN";
  case LogicV2EngineState::Ready:
    return "READY";
  case LogicV2EngineState::Stopped:
    return "STOP";
  case LogicV2EngineState::Fault:
    return "FAULT";
  case LogicV2EngineState::Empty:
  default:
    return "EMPTY";
  }
}

uint16_t stateColorActive(LogicV2EngineState state)
{
  switch (state)
  {
  case LogicV2EngineState::Running:
    return COLOR_OK;
  case LogicV2EngineState::Ready:
    return COLOR_WARNING;
  case LogicV2EngineState::Fault:
    return COLOR_ERROR;
  case LogicV2EngineState::Stopped:
  case LogicV2EngineState::Empty:
  default:
    return COLOR_MUTED;
  }
}
}

RuntimeUIFBDMapActiveRenderer::RuntimeUIFBDMapActiveRenderer()
    : RuntimeUIFBDMapV14(),
      _activeFieldsCacheValid(false),
      _activeMajorCache(0),
      _activeMinorCache(0),
      _activeBaseCache(LogoTimeBase::Seconds),
      _activeFocusCache(LogoField::Major),
      _activeElapsedCacheValid(false),
      _activeElapsedColorOn(false),
      _activeElapsedCache{}
{
}

int16_t RuntimeUIFBDMapActiveRenderer::fieldX(LogoField field)
{
  switch (field)
  {
  case LogoField::Minor:
    return 108;
  case LogoField::Base:
    return 206;
  case LogoField::Major:
  default:
    return 10;
  }
}

int16_t RuntimeUIFBDMapActiveRenderer::fieldWidth(LogoField field)
{
  return field == LogoField::Base ? 104 : 94;
}

void RuntimeUIFBDMapActiveRenderer::invalidateActiveEditorCaches()
{
  _activeFieldsCacheValid = false;
  _activeMajorCache = 0;
  _activeMinorCache = 0;
  _activeBaseCache = LogoTimeBase::Seconds;
  _activeFocusCache = LogoField::Major;

  _activeElapsedCacheValid = false;
  _activeElapsedColorOn = false;
  _activeElapsedCache[0] = '\0';
}

void RuntimeUIFBDMapActiveRenderer::refresh(const JWPLC_IOState *io,
                                            const JWPLC_RTCState *rtc)
{
  if (!parameterEditorActiveForExtension() && detailModeActiveV11())
  {
    // ESC se resuelve antes del handler V7. Así DETALLE -> MAPA no ejecuta la
    // pareja histórica drawMapStatic()+drawMapFull().
    if (handleDetailBackSinglePassV11())
    {
      invalidateActiveEditorCaches();
      return;
    }

    // OK sobre PARAM T abre la sesión y dibuja directamente el editor de tres
    // campos. El editor V5 VALOR/UNIDAD no recibe nunca este evento.
    if (detailParameterSelectedForExtensionV7() &&
        selectedTonForExtension() != nullptr &&
        JWPLC_Buttons.pressed(BTN_OK))
    {
      invalidateActiveEditorCaches();
      if (openExistingLogoEditorDirectV13())
      {
        return;
      }
    }
  }

  RuntimeUIFBDMapV14::refresh(io, rtc);
}

const char *RuntimeUIFBDMapActiveRenderer::existingFieldLabel(
    LogoField field) const
{
  switch (field)
  {
  case LogoField::Minor:
    return minorLabel(_existingBase);
  case LogoField::Base:
    return "BASE";
  case LogoField::Major:
  default:
    return majorLabel(_existingBase);
  }
}

void RuntimeUIFBDMapActiveRenderer::formatExistingFieldValue(
    LogoField field,
    char *destination,
    size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  switch (field)
  {
  case LogoField::Minor:
    std::snprintf(destination,
                  capacity,
                  "<%02lu>",
                  static_cast<unsigned long>(_existingMinor));
    break;
  case LogoField::Base:
    std::snprintf(destination,
                  capacity,
                  "<%s>",
                  baseText(_existingBase));
    break;
  case LogoField::Major:
  default:
    std::snprintf(destination,
                  capacity,
                  "<%02lu>",
                  static_cast<unsigned long>(_existingMajor));
    break;
  }
}

void RuntimeUIFBDMapActiveRenderer::drawExistingFieldFull(LogoField field)
{
  char value[10];
  char full[24];
  formatExistingFieldValue(field, value, sizeof(value));
  std::snprintf(full,
                sizeof(full),
                "%s %s",
                existingFieldLabel(field),
                value);

  drawMenuButton(JWPLC_Display.tft(),
                 fieldX(field),
                 FIELD_Y,
                 fieldWidth(field),
                 FIELD_H,
                 full,
                 _existingFocus == field);
}

void RuntimeUIFBDMapActiveRenderer::drawExistingFieldValueOnly(
    LogoField field)
{
  const char *label = existingFieldLabel(field);
  char value[10];
  char full[24];
  formatExistingFieldValue(field, value, sizeof(value));
  std::snprintf(full, sizeof(full), "%s %s", label, value);

  const int16_t x = fieldX(field);
  const int16_t width = fieldWidth(field);
  const int16_t textY = static_cast<int16_t>(FIELD_Y + (FIELD_H - 8) / 2);
  const int16_t fullWidth = static_cast<int16_t>(std::strlen(full) * 6U);
  const int16_t textX = static_cast<int16_t>(x + (width - fullWidth) / 2);
  const int16_t valueX = static_cast<int16_t>(
      textX + (std::strlen(label) + 1U) * 6U);
  const int16_t valueWidth = static_cast<int16_t>(std::strlen(value) * 6U);

  const bool selected = _existingFocus == field;
  const uint16_t background = selected ? COLOR_SELECTED : COLOR_PANEL;
  const uint16_t foreground = selected ? COLOR_ACCENT : COLOR_TEXT;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(valueX, textY, valueWidth, 8, background);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(foreground, background);
  tft.setCursor(valueX, textY);
  tft.print(value);
}

void RuntimeUIFBDMapActiveRenderer::drawExistingLogoFields()
{
  if (!_activeFieldsCacheValid || _activeBaseCache != _existingBase)
  {
    drawExistingFieldFull(LogoField::Major);
    drawExistingFieldFull(LogoField::Minor);
    drawExistingFieldFull(LogoField::Base);
  }
  else
  {
    const bool focusChanged = _activeFocusCache != _existingFocus;
    if (focusChanged)
    {
      drawExistingFieldFull(_activeFocusCache);
      drawExistingFieldFull(_existingFocus);
    }

    if (_activeMajorCache != _existingMajor &&
        (!focusChanged ||
         (_activeFocusCache != LogoField::Major &&
          _existingFocus != LogoField::Major)))
    {
      drawExistingFieldValueOnly(LogoField::Major);
    }

    if (_activeMinorCache != _existingMinor &&
        (!focusChanged ||
         (_activeFocusCache != LogoField::Minor &&
          _existingFocus != LogoField::Minor)))
    {
      drawExistingFieldValueOnly(LogoField::Minor);
    }
  }

  _activeMajorCache = _existingMajor;
  _activeMinorCache = _existingMinor;
  _activeBaseCache = _existingBase;
  _activeFocusCache = _existingFocus;
  _activeFieldsCacheValid = true;
}

void RuntimeUIFBDMapActiveRenderer::drawExistingElapsed(bool force)
{
  const uint32_t nowMs = millis();
  if (!force &&
      static_cast<uint32_t>(nowMs - _lastExistingElapsedRefreshMs) <
          EXISTING_ELAPSED_REFRESH_MS)
  {
    return;
  }
  _lastExistingElapsedRefreshMs = nowMs;

  RuntimeUIV2ReadModel *model = readModelV11();
  if (model == nullptr)
  {
    return;
  }

  uint32_t major = 0;
  uint32_t minor = 0;
  decodeMilliseconds(
      model->tonElapsedMs(selectedBlockIndexForExtension(), nowMs),
      _existingBase,
      major,
      minor);

  char elapsed[20];
  formatLogoTime(major,
                 minor,
                 _existingBase,
                 elapsed,
                 sizeof(elapsed));

  const bool colorOn =
      model->tonTiming(selectedBlockIndexForExtension()) ||
      model->blockValue(selectedBlockIndexForExtension());

  if (!force &&
      _activeElapsedCacheValid &&
      _activeElapsedColorOn == colorOn &&
      std::strcmp(_activeElapsedCache, elapsed) == 0)
  {
    return;
  }

  // El rótulo Ta LECTURA permanece inmóvil. Solo se limpia y actualiza el valor.
  updateTextField(JWPLC_Display.tft(),
                  ELAPSED_VALUE_X_ACTIVE,
                  ELAPSED_VALUE_Y_ACTIVE,
                  ELAPSED_VALUE_COLUMNS_ACTIVE,
                  elapsed,
                  colorOn ? COLOR_OK : COLOR_MUTED,
                  COLOR_PANEL);

  _activeElapsedColorOn = colorOn;
  std::snprintf(_activeElapsedCache,
                sizeof(_activeElapsedCache),
                "%s",
                elapsed);
  _activeElapsedCacheValid = true;
}

void RuntimeUIFBDMapActiveRenderer::drawEditorHeaderTwoLines()
{
  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2EngineState state =
      model != nullptr ? model->state() : LogicV2EngineState::Empty;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  drawHeaderStatic(tft, "EDITAR T");

  char line1[20];
  std::snprintf(line1,
                sizeof(line1),
                "B%02u TON",
                static_cast<unsigned>(selectedBlockIndexForExtension()));

  updateTextField(tft,
                  HEADER_INFO_X_ACTIVE,
                  HEADER_INFO_LINE1_Y_ACTIVE,
                  HEADER_INFO_COLUMNS_ACTIVE,
                  line1,
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateTextField(tft,
                  HEADER_INFO_X_ACTIVE,
                  HEADER_INFO_LINE2_Y_ACTIVE,
                  HEADER_INFO_COLUMNS_ACTIVE,
                  "PARAM T",
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateHeaderState(tft,
                    stateTextActive(state),
                    stateColorActive(state));
}

void RuntimeUIFBDMapActiveRenderer::drawExistingLogoScreen()
{
  invalidateActiveEditorCaches();
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  // El cuerpo se sustituye por bandas y cada banda recibe su contenido final de
  // inmediato. Nunca se dibuja el editor histórico de dos campos.
  tft.fillRect(0, 94, 320, 55, COLOR_PANEL);
  drawExistingLogoFields();

  tft.fillRect(0, 24, 320, 70, COLOR_PANEL);
  drawExistingActual();
  drawFieldLabel(tft,
                 22,
                 ELAPSED_VALUE_Y_ACTIVE,
                 "Ta LECTURA",
                 COLOR_MUTED,
                 COLOR_PANEL);
  drawExistingElapsed(true);

  tft.fillRect(0, 149, 320, 21, COLOR_PANEL);
  drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);

  drawEditorHeaderTwoLines();
  tft.drawRect(PANEL_X_ACTIVE,
               PANEL_Y_ACTIVE,
               PANEL_W_ACTIVE,
               PANEL_H_ACTIVE,
               COLOR_BORDER);
}
