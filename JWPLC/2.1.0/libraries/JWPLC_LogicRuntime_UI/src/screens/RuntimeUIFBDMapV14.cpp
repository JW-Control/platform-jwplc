#include "RuntimeUIFBDMapV14.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
static constexpr int16_t PANEL_X_V14 = 2;
static constexpr int16_t PANEL_Y_V14 = 27;
static constexpr int16_t PANEL_W_V14 = 316;
static constexpr int16_t PANEL_H_V14 = 142;

static constexpr int16_t HEADER_BLOCK_X_V14 = 122;
static constexpr int16_t HEADER_BLOCK_Y_V14 = 8;
static constexpr uint8_t HEADER_BLOCK_COLUMNS_V14 = 18;

static constexpr int16_t TON_PANEL_X_V14 = 214;
static constexpr int16_t TON_PANEL_Y_V14 = 101;
static constexpr int16_t TON_PANEL_W_V14 = 80;
static constexpr int16_t TON_PANEL_H_V14 = 31;
}

RuntimeUIFBDMapV14::RuntimeUIFBDMapV14()
    : RuntimeUIFBDMapV13(),
      _detailLogoCacheValid(false),
      _detailLogoCacheBlock(0xFFFFU),
      _detailLogoCacheParameterSelected(false),
      _detailLogoCacheColorOn(false),
      _detailLogoCacheConfigured{},
      _detailLogoCacheElapsed{}
{
}

void RuntimeUIFBDMapV14::invalidateDetailLogoCache()
{
  _detailLogoCacheValid = false;
  _detailLogoCacheBlock = 0xFFFFU;
  _detailLogoCacheParameterSelected = false;
  _detailLogoCacheColorOn = false;
  _detailLogoCacheConfigured[0] = '\0';
  _detailLogoCacheElapsed[0] = '\0';
}

void RuntimeUIFBDMapV14::formatMillisecondsInBase(
    uint32_t milliseconds,
    LogoTimeBase base,
    char *destination,
    size_t capacity)
{
  uint32_t major = 0;
  uint32_t minor = 0;
  decodeMilliseconds(milliseconds, base, major, minor);
  formatLogoTime(major, minor, base, destination, capacity);
}

const char *RuntimeUIFBDMapV14::engineStateText(LogicV2EngineState state)
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

uint16_t RuntimeUIFBDMapV14::engineStateColor(LogicV2EngineState state)
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

void RuntimeUIFBDMapV14::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  // Al entrar desde MAPA o volver desde un editor, la clase base reconstruye el
  // panel completo. Se invalida la caché para que el formato LOGO! se reponga una
  // sola vez sobre ese nuevo fondo.
  if (!detailModeActiveV11() || parameterEditorActiveForExtension())
  {
    invalidateDetailLogoCache();
  }

  // V11 recompone únicamente el área FBD al salir de la columna virtual. Esto
  // evita que V8 ejecute drawMapStatic() y limpie encabezado/panel completos.
  if (handleAddBackRegionalV11())
  {
    return;
  }

  RuntimeUIFBDMapV13::refresh(io, rtc);
}

void RuntimeUIFBDMapV14::drawExistingElapsed(bool force)
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

  char elapsed[20];
  char line[36];

  // Ta siempre usa la base seleccionada. No se exige divisibilidad exacta:
  // mientras el scan avanza, los milisegundos sobrantes se truncan dentro de la
  // resolución de s/m/h en vez de provocar un salto accidental a horas.
  formatMillisecondsInBase(
      model->tonElapsedMs(selectedBlockIndexForExtension(), nowMs),
      _existingBase,
      elapsed,
      sizeof(elapsed));
  std::snprintf(line, sizeof(line), "Ta LECTURA %s", elapsed);

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(22, 70, 276, 12, COLOR_PANEL);
  drawFieldLabel(tft,
                 22,
                 72,
                 line,
                 (model->tonTiming(selectedBlockIndexForExtension()) ||
                  model->blockValue(selectedBlockIndexForExtension()))
                     ? COLOR_OK
                     : COLOR_MUTED,
                 COLOR_PANEL);
}

void RuntimeUIFBDMapV14::drawExistingLogoScreen()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);

  // El título de tamaño 2 termina antes de x=110. El identificador comienza en
  // x=122 y la insignia de estado mantiene su zona fija desde x=239.
  drawHeaderStatic(tft, "EDITAR T");

  char headerInfo[24];
  std::snprintf(headerInfo,
                sizeof(headerInfo),
                "B%02u TON",
                static_cast<unsigned>(selectedBlockIndexForExtension()));
  updateTextField(tft,
                  HEADER_BLOCK_X_V14,
                  HEADER_BLOCK_Y_V14,
                  HEADER_BLOCK_COLUMNS_V14,
                  headerInfo,
                  COLOR_MUTED,
                  COLOR_PANEL);

  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2EngineState state =
      model != nullptr ? model->state() : LogicV2EngineState::Empty;
  updateHeaderState(tft,
                    engineStateText(state),
                    engineStateColor(state));

  tft.fillRect(PANEL_X_V14,
               PANEL_Y_V14,
               PANEL_W_V14,
               PANEL_H_V14,
               COLOR_PANEL);
  tft.drawRect(PANEL_X_V14,
               PANEL_Y_V14,
               PANEL_W_V14,
               PANEL_H_V14,
               COLOR_BORDER);

  drawExistingActual();
  drawExistingElapsed(true);
  drawExistingLogoFields();
  drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);
}

void RuntimeUIFBDMapV14::drawTonDetailElapsedLive(bool force)
{
  // V7 ya no debe escribir el formato histórico sobre la misma región. Toda la
  // actualización del panel TON queda centralizada en el renderer cacheado V14.
  drawDetailLogoOverlay(force);
}

void RuntimeUIFBDMapV14::drawDetailLogoOverlay(bool force)
{
  if (!detailModeActiveV11())
  {
    invalidateDetailLogoCache();
    return;
  }

  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2BlockRecord *definition = selectedBlockV11();
  if (model == nullptr ||
      definition == nullptr ||
      definition->type != LogicV2BlockType::Ton)
  {
    invalidateDetailLogoCache();
    return;
  }

  const uint32_t nowMs = millis();
  if (_detailLogoCacheValid &&
      !force &&
      static_cast<uint32_t>(nowMs - _lastDetailOverlayRefreshMs) <
          DETAIL_OVERLAY_REFRESH_MS)
  {
    return;
  }

  // T define la base efectiva del panel. Ta se formatea directamente en esa
  // misma base aunque el tiempo instantáneo aún no complete una centésima.
  const LogoTimeBase displayBase =
      effectiveBase(definition->parameter, definition->resource);

  char configured[16];
  char elapsed[16];
  formatMillisecondsInBase(definition->parameter,
                           displayBase,
                           configured,
                           sizeof(configured));
  formatMillisecondsInBase(
      model->tonElapsedMs(selectedBlockIndexV11(), nowMs),
      displayBase,
      elapsed,
      sizeof(elapsed));

  const uint16_t blockIndex = selectedBlockIndexV11();
  const bool parameterSelected = detailParameterSelectedForExtensionV7();
  const bool colorOn = model->tonTiming(blockIndex) ||
                       model->blockValue(blockIndex);
  const bool blockChanged = !_detailLogoCacheValid ||
                            _detailLogoCacheBlock != blockIndex;

  const bool configuredChanged =
      force ||
      blockChanged ||
      _detailLogoCacheParameterSelected != parameterSelected ||
      std::strcmp(_detailLogoCacheConfigured, configured) != 0;

  const bool elapsedChanged =
      force ||
      blockChanged ||
      _detailLogoCacheColorOn != colorOn ||
      std::strcmp(_detailLogoCacheElapsed, elapsed) != 0;

  _lastDetailOverlayRefreshMs = nowMs;
  if (!configuredChanged && !elapsedChanged)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.setTextWrap(false);
  tft.setTextSize(1);

  // T es estático durante la ejecución. Solo se repinta si cambia el parámetro,
  // la base o el foco amarillo. Un cambio exclusivo de Ta no toca esta línea.
  if (configuredChanged)
  {
    tft.fillRect(TON_PANEL_X_V14 + 4,
                 TON_PANEL_Y_V14 + 3,
                 TON_PANEL_W_V14 - 8,
                 9,
                 COLOR_BACKGROUND);
    tft.setTextColor(parameterSelected ? COLOR_WARNING : COLOR_TEXT,
                     COLOR_BACKGROUND);
    tft.setCursor(TON_PANEL_X_V14 + 4, TON_PANEL_Y_V14 + 4);
    tft.print("T ");
    tft.print(configured);
  }

  // Ta solo se repinta cuando cambia el texto visible o su color de estado.
  // En reposo permanece completamente inmóvil y no genera tráfico SPI.
  if (elapsedChanged)
  {
    tft.fillRect(TON_PANEL_X_V14 + 4,
                 TON_PANEL_Y_V14 + 17,
                 TON_PANEL_W_V14 - 8,
                 TON_PANEL_H_V14 - 19,
                 COLOR_BACKGROUND);
    tft.setTextColor(colorOn ? COLOR_OK : COLOR_MUTED,
                     COLOR_BACKGROUND);
    tft.setCursor(TON_PANEL_X_V14 + 4, TON_PANEL_Y_V14 + 19);
    tft.print("Ta ");
    tft.print(elapsed);
  }

  _detailLogoCacheBlock = blockIndex;
  _detailLogoCacheParameterSelected = parameterSelected;
  _detailLogoCacheColorOn = colorOn;
  std::snprintf(_detailLogoCacheConfigured,
                sizeof(_detailLogoCacheConfigured),
                "%s",
                configured);
  std::snprintf(_detailLogoCacheElapsed,
                sizeof(_detailLogoCacheElapsed),
                "%s",
                elapsed);
  _detailLogoCacheValid = true;
}
