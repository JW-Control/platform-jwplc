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
      _activeElapsedCache{},
      _activeFooterDefaultVisible(false),
      _unifiedHeaderCacheValid(false),
      _unifiedHeaderViewCache(HeaderView::None),
      _unifiedHeaderStateCache(LogicV2EngineState::Empty),
      _unifiedHeaderLine1Cache{},
      _unifiedHeaderLine2Cache{}
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
  _activeFooterDefaultVisible = false;
}

void RuntimeUIFBDMapActiveRenderer::invalidateUnifiedHeaderCache()
{
  _unifiedHeaderCacheValid = false;
  _unifiedHeaderViewCache = HeaderView::None;
  _unifiedHeaderStateCache = LogicV2EngineState::Empty;
  _unifiedHeaderLine1Cache[0] = '\0';
  _unifiedHeaderLine2Cache[0] = '\0';
}

void RuntimeUIFBDMapActiveRenderer::enter()
{
  invalidateActiveEditorCaches();
  invalidateUnifiedHeaderCache();
  RuntimeUIFBDMapV14::enter();
  drawUnifiedHeader(true);
}

void RuntimeUIFBDMapActiveRenderer::forceRedraw()
{
  invalidateActiveEditorCaches();
  invalidateUnifiedHeaderCache();
  RuntimeUIFBDMapV14::forceRedraw();
}

void RuntimeUIFBDMapActiveRenderer::refresh(const JWPLC_IOState *io,
                                            const JWPLC_RTCState *rtc)
{
  if (parameterEditorActiveForExtension())
  {
    refreshActiveTonEditor(io, rtc);
    return;
  }

  if (detailModeActiveV11())
  {
    // ESC se resuelve antes del handler V7. Así DETALLE -> MAPA no ejecuta la
    // pareja histórica drawMapStatic()+drawMapFull().
    if (handleDetailBackSinglePassV11())
    {
      invalidateActiveEditorCaches();
      invalidateUnifiedHeaderCache();
      drawUnifiedHeader(true);
      return;
    }

    // OK sobre PARAM T abre la sesión y dibuja directamente el editor de tres
    // campos. El editor V5 VALOR/UNIDAD no recibe nunca este evento.
    if (detailParameterSelectedForExtensionV7() &&
        selectedTonForExtension() != nullptr &&
        JWPLC_Buttons.pressed(BTN_OK))
    {
      invalidateActiveEditorCaches();
      invalidateUnifiedHeaderCache();
      if (openExistingLogoEditorDirectV13())
      {
        drawUnifiedHeader(true);
        return;
      }
    }
  }

  RuntimeUIFBDMapV14::refresh(io, rtc);
  drawUnifiedHeader(false);
}

void RuntimeUIFBDMapActiveRenderer::returnEditorToDetailSinglePass(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  _existingLogoInitialized = false;
  _lastDetailOverlayRefreshMs = 0;
  invalidateActiveEditorCaches();
  invalidateUnifiedHeaderCache();

  // cancelTonParameterEditorForExtension()/finishTonParameterApplyForExtension()
  // dejan _fullRedraw activo. El primer y único refresh siguiente compone DETALLE
  // completo y esta clase repone la cabecera unificada al final del mismo callback.
  RuntimeUIFBDMapV14::refresh(io, rtc);
  drawUnifiedHeader(true);
}

void RuntimeUIFBDMapActiveRenderer::refreshActiveTonEditor(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);

  if (!_existingLogoInitialized)
  {
    beginExistingLogoEditor();
    drawUnifiedHeader(true);
    return;
  }

  if (tonParameterApplyCompletedForExtension())
  {
    const bool success = tonParameterApplySucceededForExtension();
    finishTonParameterApplyForExtension(success);
    if (success)
    {
      returnEditorToDetailSinglePass(io, rtc);
      return;
    }

    drawExistingFooter("ERROR AL APLICAR", COLOR_ERROR);
    _activeFooterDefaultVisible = false;
    return;
  }

  // Ta solo se toca si cambió su valor visible o color. No se fuerza por mover
  // SEG/CENT/BASE ni por cambiar el foco.
  drawExistingElapsed(false);

  if (tonParameterApplyPendingForExtension())
  {
    drawUnifiedHeader(false);
    return;
  }

  if (consumeInputReleaseGateForExtension())
  {
    drawUnifiedHeader(false);
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    JWPLC_Display.notifyActivity();
    cancelTonParameterEditorForExtension();
    returnEditorToDetailSinglePass(io, rtc);
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    const uint32_t parameter = encodeMilliseconds(_existingMajor,
                                                  _existingMinor,
                                                  _existingBase);
    JWPLC_Display.notifyActivity();

    if (parameter < LOGO_MINIMUM_TIME_MS_ACTIVE)
    {
      drawExistingFooter("T MINIMO 00:02s", COLOR_WARNING);
      _activeFooterDefaultVisible = false;
      return;
    }

    if (!requestTonParameterApplyForExtension(
            parameter,
            resourceFromBase(_existingBase)))
    {
      drawExistingFooter("PARAMETRO NO VALIDO", COLOR_ERROR);
      _activeFooterDefaultVisible = false;
      return;
    }

    drawExistingFooter("APLICANDO CAMBIOS...", COLOR_WARNING);
    _activeFooterDefaultVisible = false;
    return;
  }

  const LogoTimeBase previousBase = _existingBase;
  if (!handleLogoFields(_existingMajor,
                        _existingMinor,
                        _existingBase,
                        _existingFocus,
                        false))
  {
    drawUnifiedHeader(false);
    return;
  }

  drawExistingLogoFields();

  // Al cambiar BASE sí cambia el significado de Ta y se actualiza una vez. Al
  // modificar solo los componentes no se toca Ta porque corresponde al runtime.
  if (previousBase != _existingBase)
  {
    drawExistingElapsed(true);
  }

  // Un mensaje de error se restaura una sola vez al volver a editar. En estado
  // normal el pie permanece inmóvil durante todos los UP/DOWN.
  if (!_activeFooterDefaultVisible)
  {
    drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
    _activeFooterDefaultVisible = true;
  }

  drawUnifiedHeader(false);
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

RuntimeUIFBDMapActiveRenderer::HeaderView
RuntimeUIFBDMapActiveRenderer::currentHeaderView() const
{
  if (parameterEditorActiveForExtension())
  {
    return HeaderView::EditTon;
  }
  if (detailModeActiveV11())
  {
    return HeaderView::Detail;
  }
  if (normalMapRootActiveV11() || addNodeSelectedV11())
  {
    return HeaderView::Map;
  }
  return HeaderView::None;
}

const char *RuntimeUIFBDMapActiveRenderer::titleForHeaderView(
    HeaderView view) const
{
  switch (view)
  {
  case HeaderView::Map:
    return "MAPA FBD";
  case HeaderView::Detail:
    return "DETALLE";
  case HeaderView::EditTon:
    return "EDITAR T";
  case HeaderView::None:
  default:
    return "";
  }
}

void RuntimeUIFBDMapActiveRenderer::buildUnifiedHeaderContext(
    HeaderView view,
    char *line1,
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

  RuntimeUIV2ReadModel *model = readModelV11();
  if (view == HeaderView::Map && addNodeSelectedV11())
  {
    std::snprintf(line1, line1Capacity, "NUEVO BLOQUE");
    std::snprintf(line2, line2Capacity, "+");
    return;
  }

  const LogicV2BlockRecord *definition = selectedBlockV11();
  if (model == nullptr || definition == nullptr)
  {
    std::snprintf(line1, line1Capacity, "SIN BLOQUES");
    return;
  }

  std::snprintf(line1,
                line1Capacity,
                "B%02u %s",
                static_cast<unsigned>(selectedBlockIndexV11()),
                model->typeShort(definition->type));

  if (view == HeaderView::EditTon)
  {
    std::snprintf(line2, line2Capacity, "PARAM T");
    return;
  }

  if (view == HeaderView::Detail)
  {
    if (detailParameterSelectedForExtensionV7() &&
        definition->type == LogicV2BlockType::Ton)
    {
      std::snprintf(line2, line2Capacity, "PARAM T");
      return;
    }

    if (definition->inputCount > 0)
    {
      const uint8_t input = detailInputIndexForExtensionV7();
      const char *role = model->inputRole(definition->type, input);
      if (role != nullptr && role[0] != '\0')
      {
        std::snprintf(line2, line2Capacity, "%s", role);
      }
      else
      {
        std::snprintf(line2,
                      line2Capacity,
                      "IN%u/%u",
                      static_cast<unsigned>(input + 1U),
                      static_cast<unsigned>(definition->inputCount));
      }
      return;
    }

    std::snprintf(line2, line2Capacity, "SIN ENTRADAS");
    return;
  }

  // En MAPA la segunda fila se mantiene compacta y útil, sin reutilizar textos
  // de DETALLE que puedan quedar superpuestos durante una transición.
  std::snprintf(line2,
                line2Capacity,
                "%s",
                model->blockValue(selectedBlockIndexV11()) ? "ON" : "OFF");
}

void RuntimeUIFBDMapActiveRenderer::drawUnifiedHeader(bool force)
{
  const HeaderView view = currentHeaderView();
  if (view == HeaderView::None)
  {
    return;
  }

  char line1[24];
  char line2[24];
  buildUnifiedHeaderContext(view,
                            line1,
                            sizeof(line1),
                            line2,
                            sizeof(line2));

  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2EngineState state =
      model != nullptr ? model->state() : LogicV2EngineState::Empty;

  const bool viewChanged =
      !_unifiedHeaderCacheValid || _unifiedHeaderViewCache != view;
  const bool line1Changed =
      !_unifiedHeaderCacheValid ||
      std::strcmp(_unifiedHeaderLine1Cache, line1) != 0;
  const bool line2Changed =
      !_unifiedHeaderCacheValid ||
      std::strcmp(_unifiedHeaderLine2Cache, line2) != 0;
  const bool stateChanged =
      !_unifiedHeaderCacheValid || _unifiedHeaderStateCache != state;

  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  if (force || viewChanged)
  {
    tft.fillRect(HEADER_TITLE_X,
                 0,
                 HEADER_TITLE_W,
                 23,
                 COLOR_PANEL);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.setCursor(6, 5);
    tft.print(titleForHeaderView(view));
  }

  if (force || viewChanged || line1Changed || line2Changed)
  {
    // Limpia toda la región histórica x=112..232 antes de escribir las dos filas.
    // Así no puede quedar Trg/PARAM T ni el header de una vista anterior.
    tft.fillRect(HEADER_CONTEXT_X,
                 0,
                 HEADER_CONTEXT_W,
                 23,
                 COLOR_PANEL);
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_CONTEXT_LINE1_Y,
                    HEADER_CONTEXT_COLUMNS,
                    line1,
                    COLOR_MUTED,
                    COLOR_PANEL);
    updateTextField(tft,
                    HEADER_CONTEXT_X + 4,
                    HEADER_CONTEXT_LINE2_Y,
                    HEADER_CONTEXT_COLUMNS,
                    line2,
                    COLOR_MUTED,
                    COLOR_PANEL);
  }

  if (force || viewChanged || stateChanged)
  {
    updateHeaderState(tft,
                      stateTextActive(state),
                      stateColorActive(state));
  }

  if (force || viewChanged)
  {
    tft.drawFastHLine(0, 23, 320, COLOR_BORDER);
  }

  _unifiedHeaderViewCache = view;
  _unifiedHeaderStateCache = state;
  std::snprintf(_unifiedHeaderLine1Cache,
                sizeof(_unifiedHeaderLine1Cache),
                "%s",
                line1);
  std::snprintf(_unifiedHeaderLine2Cache,
                sizeof(_unifiedHeaderLine2Cache),
                "%s",
                line2);
  _unifiedHeaderCacheValid = true;
}

void RuntimeUIFBDMapActiveRenderer::drawExistingLogoScreen()
{
  invalidateActiveEditorCaches();
  invalidateUnifiedHeaderCache();

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
  _activeFooterDefaultVisible = true;

  tft.drawRect(PANEL_X_ACTIVE,
               PANEL_Y_ACTIVE,
               PANEL_W_ACTIVE,
               PANEL_H_ACTIVE,
               COLOR_BORDER);

  drawUnifiedHeader(true);
}
