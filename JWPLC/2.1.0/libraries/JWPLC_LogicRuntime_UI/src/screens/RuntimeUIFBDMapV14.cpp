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

static constexpr int16_t DETAIL_HEADER_X_V14 = 112;
static constexpr int16_t DETAIL_HEADER_LINE1_Y_V14 = 4;
static constexpr int16_t DETAIL_HEADER_LINE2_Y_V14 = 13;
static constexpr uint8_t DETAIL_HEADER_COLUMNS_V14 = 20;

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
      _detailLogoCacheElapsed{},
      _existingElapsedCacheValid(false),
      _existingElapsedCacheColorOn(false),
      _existingElapsedCacheText{},
      _compactDetailHeaderValid(false),
      _compactDetailHeaderBlock(0xFFFFU),
      _compactDetailHeaderInput(0),
      _compactDetailHeaderParameter(false)
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

void RuntimeUIFBDMapV14::invalidateExistingElapsedCache()
{
  _existingElapsedCacheValid = false;
  _existingElapsedCacheColorOn = false;
  _existingElapsedCacheText[0] = '\0';
}

void RuntimeUIFBDMapV14::invalidateCompactDetailHeader()
{
  _compactDetailHeaderValid = false;
  _compactDetailHeaderBlock = 0xFFFFU;
  _compactDetailHeaderInput = 0;
  _compactDetailHeaderParameter = false;
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
  // panel completo. Se invalidan las cachés para reponer una sola vez el formato
  // LOGO! y el encabezado compacto sobre el nuevo fondo.
  if (!detailModeActiveV11() || parameterEditorActiveForExtension())
  {
    invalidateDetailLogoCache();
    invalidateCompactDetailHeader();
  }

  // V11 recompone únicamente el área FBD al salir de la columna virtual. Esto
  // evita que V8 ejecute drawMapStatic() y limpie encabezado/panel completos.
  if (handleAddBackRegionalV11())
  {
    return;
  }

  RuntimeUIFBDMapV13::refresh(io, rtc);

  if (detailModeActiveV11() && !parameterEditorActiveForExtension())
  {
    drawCompactDetailHeader(false);
  }
}

void RuntimeUIFBDMapV14::drawCompactDetailHeader(bool force)
{
  RuntimeUIV2ReadModel *model = readModelV11();
  const LogicV2BlockRecord *definition = selectedBlockV11();
  if (model == nullptr || definition == nullptr)
  {
    invalidateCompactDetailHeader();
    return;
  }

  const uint16_t blockIndex = selectedBlockIndexV11();
  const uint8_t inputIndex = detailInputIndexForExtensionV7();
  const bool parameterSelected = detailParameterSelectedForExtensionV7();

  if (!force &&
      _compactDetailHeaderValid &&
      _compactDetailHeaderBlock == blockIndex &&
      _compactDetailHeaderInput == inputIndex &&
      _compactDetailHeaderParameter == parameterSelected)
  {
    return;
  }

  char line1[24];
  char line2[24];
  std::snprintf(line1,
                sizeof(line1),
                "B%02u %s",
                static_cast<unsigned>(blockIndex),
                model->typeShort(definition->type));

  if (parameterSelected && definition->type == LogicV2BlockType::Ton)
  {
    std::snprintf(line2, sizeof(line2), "PARAM T");
  }
  else if (definition->inputCount > 0)
  {
    const char *role = model->inputRole(definition->type, inputIndex);
    if (role != nullptr && role[0] != '\0')
    {
      std::snprintf(line2, sizeof(line2), "%s", role);
    }
    else
    {
      std::snprintf(line2,
                    sizeof(line2),
                    "IN%u/%u",
                    static_cast<unsigned>(inputIndex + 1U),
                    static_cast<unsigned>(definition->inputCount));
    }
  }
  else
  {
    line2[0] = '\0';
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(DETAIL_HEADER_X_V14,
               3,
               static_cast<int16_t>(DETAIL_HEADER_COLUMNS_V14) * 6,
               18,
               COLOR_PANEL);
  updateTextField(tft,
                  DETAIL_HEADER_X_V14,
                  DETAIL_HEADER_LINE1_Y_V14,
                  DETAIL_HEADER_COLUMNS_V14,
                  line1,
                  COLOR_MUTED,
                  COLOR_PANEL);
  updateTextField(tft,
                  DETAIL_HEADER_X_V14,
                  DETAIL_HEADER_LINE2_Y_V14,
                  DETAIL_HEADER_COLUMNS_V14,
                  line2,
                  COLOR_MUTED,
                  COLOR_PANEL);

  _compactDetailHeaderBlock = blockIndex;
  _compactDetailHeaderInput = inputIndex;
  _compactDetailHeaderParameter = parameterSelected;
  _compactDetailHeaderValid = true;
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

  const bool colorOn =
      model->tonTiming(selectedBlockIndexForExtension()) ||
      model->blockValue(selectedBlockIndexForExtension());

  // En EDITAR T se aplica la misma regla ya validada en DETALLE: no limpiar ni
  // escribir la región cuando el texto visible y el color continúan iguales.
  if (!force &&
      _existingElapsedCacheValid &&
      _existingElapsedCacheColorOn == colorOn &&
      std::strcmp(_existingElapsedCacheText, line) == 0)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(22, 70, 276, 12, COLOR_PANEL);
  drawFieldLabel(tft,
                 22,
                 72,
                 line,
                 colorOn ? COLOR_OK : COLOR_MUTED,
                 COLOR_PANEL);

  _existingElapsedCacheColorOn = colorOn;
  std::snprintf(_existingElapsedCacheText,
                sizeof(_existingElapsedCacheText),
                "%s",
                line);
  _existingElapsedCacheValid = true;
}

void RuntimeUIFBDMapV14::drawExistingLogoScreen()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  invalidateExistingElapsedCache();
  invalidateCompactDetailHeader();

  // La transición se compone por bandas pequeñas y cada banda recibe su contenido
  // inmediatamente. Se evita el fillRect único de 320x146 que dejaba visible un
  // gran panel vacío durante el barrido físico del ST7789.

  // 1) Controles: es la región más reconocible de la nueva pantalla.
  tft.fillRect(0, 94, 320, 55, COLOR_PANEL);
  drawExistingLogoFields();

  // 2) Información T/Ta.
  tft.fillRect(0, 24, 320, 70, COLOR_PANEL);
  drawExistingActual();
  drawExistingElapsed(true);

  // 3) Pie de acciones.
  tft.fillRect(0, 149, 320, 21, COLOR_PANEL);
  drawExistingFooter("OK GUARDAR   ESC CANCELAR", COLOR_MUTED);

  // 4) Encabezado final. Se dibuja al terminar el contenido para que no exista un
  // periodo perceptible con título EDITAR T y cuerpo todavía perteneciente a
  // DETALLE.
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

  tft.drawRect(PANEL_X_V14,
               PANEL_Y_V14,
               PANEL_W_V14,
               PANEL_H_V14,
               COLOR_BORDER);
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
