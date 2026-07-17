#include "RuntimeUIFBDMapV4.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace
{
  static constexpr int16_t SOURCE_X = 10;
  static constexpr int16_t SOURCE_Y = 42;
  static constexpr int16_t SOURCE_W = 84;
  static constexpr int16_t SOURCE_H = 48;

  static constexpr int16_t TARGET_X = 218;
  static constexpr int16_t TARGET_Y = 42;
  static constexpr int16_t TARGET_W = 88;
  static constexpr int16_t TARGET_H = 48;

  static constexpr int16_t FIELD_Y = 103;
  static constexpr int16_t FIELD_H = 38;
  static constexpr int16_t SOURCE_FIELD_X = 10;
  static constexpr int16_t SOURCE_FIELD_W = 145;
  static constexpr int16_t LOGIC_FIELD_X = 165;
  static constexpr int16_t LOGIC_FIELD_W = 145;

  bool neutralValueFor(LogicV2BlockType type)
  {
    return type == LogicV2BlockType::And ||
           type == LogicV2BlockType::Nand;
  }
}

void RuntimeUIFBDMapV4::drawInputEditStatic()
{
  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearScreen(tft);
  drawHeaderStatic(tft, "EDITAR IN");
  _headerStateValid = false;
  updateHeaderStateIfNeeded(true);
  tft.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_PANEL);
  tft.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COLOR_BORDER);
}

void RuntimeUIFBDMapV4::formatEditSource(char *id,
                                         size_t idCapacity,
                                         char *type,
                                         size_t typeCapacity) const
{
  if (id == nullptr || idCapacity == 0 ||
      type == nullptr || typeCapacity == 0)
  {
    return;
  }

  const uint16_t source = sourceCandidateAt(_editSourceCandidate);
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(id, idCapacity, "X");
    std::snprintf(type, typeCapacity, "ABIERTO");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(id, idCapacity, "HI");
    std::snprintf(type, typeCapacity, "CONST 1");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(id, idCapacity, "LO");
    std::snprintf(type, typeCapacity, "CONST 0");
    return;
  }

  std::snprintf(id,
                idCapacity,
                "B%02u",
                static_cast<unsigned>(source));
  const LogicV2BlockRecord *definition =
      _model != nullptr ? _model->block(source) : nullptr;
  std::snprintf(type,
                typeCapacity,
                "%s",
                definition != nullptr
                    ? _model->typeShort(definition->type)
                    : "?");
}

void RuntimeUIFBDMapV4::drawInputEdit()
{
  if (_model == nullptr)
  {
    return;
  }

  const LogicV2BlockRecord *target = _model->block(_selectedIndex);
  if (target == nullptr || _detailInputIndex >= target->inputCount)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.fillRect(MAP_X, MAP_Y, MAP_W, MAP_H, COLOR_PANEL);

  char headerInfo[28];
  std::snprintf(headerInfo,
                sizeof(headerInfo),
                "B%02u %s IN%u/%u",
                static_cast<unsigned>(_selectedIndex),
                _model->typeShort(target->type),
                static_cast<unsigned>(_detailInputIndex + 1U),
                static_cast<unsigned>(target->inputCount));
  updateTextField(tft,
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  headerInfo,
                  COLOR_MUTED,
                  COLOR_PANEL);

  char sourceId[12];
  char sourceType[14];
  formatEditSource(sourceId,
                   sizeof(sourceId),
                   sourceType,
                   sizeof(sourceType));

  const uint16_t source = sourceCandidateAt(_editSourceCandidate);
  bool rawValue = false;
  bool sourceKnown = true;
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    rawValue = neutralValueFor(target->type);
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    rawValue = true;
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    rawValue = false;
  }
  else if (source < _model->blockCount())
  {
    rawValue = _model->blockValue(source);
  }
  else
  {
    sourceKnown = false;
  }

  const bool effectiveValue = sourceKnown
                                  ? (_editInverted ? !rawValue : rawValue)
                                  : false;
  const uint16_t sourceBorder = rawValue ? COLOR_OK : COLOR_BORDER;
  const uint16_t wireColor = effectiveValue ? COLOR_OK : COLOR_MUTED;

  // Fuente candidata.
  tft.fillRect(SOURCE_X, SOURCE_Y, SOURCE_W, SOURCE_H, COLOR_BACKGROUND);
  tft.drawRect(SOURCE_X, SOURCE_Y, SOURCE_W, SOURCE_H, sourceBorder);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(SOURCE_X + 6, SOURCE_Y + 7);
  tft.print(sourceId);
  tft.setTextColor(rawValue ? COLOR_OK : COLOR_MUTED,
                   COLOR_BACKGROUND);
  tft.setCursor(SOURCE_X + 6, SOURCE_Y + 26);
  tft.print(sourceType);

  // Bloque destino.
  tft.fillRect(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, COLOR_BACKGROUND);
  tft.drawRect(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, COLOR_BORDER);

  char targetId[8];
  formatBlockId(targetId, sizeof(targetId), _selectedIndex);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(TARGET_X + 8, TARGET_Y + 6);
  tft.print(targetId);

  tft.setTextSize(2);
  tft.setCursor(TARGET_X + 42, TARGET_Y + 19);
  tft.setTextColor(effectiveValue ? COLOR_OK : COLOR_TEXT,
                   COLOR_BACKGROUND);
  tft.print(detailSymbol(target->type));

  const char *role = _model->inputRole(target->type, _detailInputIndex);
  char roleText[6];
  if (role != nullptr && role[0] != '\0')
  {
    std::snprintf(roleText, sizeof(roleText), "%s", role);
  }
  else
  {
    std::snprintf(roleText,
                  sizeof(roleText),
                  "%u",
                  static_cast<unsigned>(_detailInputIndex + 1U));
  }
  tft.setTextSize(1);
  tft.setTextColor(wireColor, COLOR_BACKGROUND);
  tft.setCursor(TARGET_X + 7, TARGET_Y + 28);
  tft.print(roleText);

  // Vista previa gráfica de la conexión.
  const int16_t wireY = static_cast<int16_t>(SOURCE_Y + SOURCE_H / 2);
  const int16_t sourceEnd = static_cast<int16_t>(SOURCE_X + SOURCE_W);
  const int16_t targetStart = TARGET_X;
  tft.drawFastHLine(sourceEnd,
                    wireY,
                    static_cast<int16_t>(targetStart - sourceEnd),
                    wireColor);
  if (_editInverted)
  {
    tft.fillCircle(targetStart,
                   wireY,
                   5,
                   COLOR_PANEL);
    tft.drawCircle(targetStart,
                   wireY,
                   4,
                   COLOR_MUTED);
  }
  else
  {
    tft.fillCircle(targetStart, wireY, 2, wireColor);
  }

  char sourceField[24];
  std::snprintf(sourceField,
                sizeof(sourceField),
                "FUENTE <%s>",
                sourceId);
  drawMenuButton(tft,
                 SOURCE_FIELD_X,
                 FIELD_Y,
                 SOURCE_FIELD_W,
                 FIELD_H,
                 sourceField,
                 _editFocus == EditFocus::Source);

  char logicField[24];
  std::snprintf(logicField,
                sizeof(logicField),
                "LOGICA <%s>",
                _editInverted ? "NEGADA" : "NORMAL");
  drawMenuButton(tft,
                 LOGIC_FIELD_X,
                 FIELD_Y,
                 LOGIC_FIELD_W,
                 FIELD_H,
                 logicField,
                 _editFocus == EditFocus::Logic);

  const char *status = "OK GUARDAR   ESC CANCELAR";
  uint16_t statusColor = COLOR_MUTED;
  switch (_editFeedback)
  {
  case EditFeedback::InvalidDraft:
    status = "CONFIGURACION NO VALIDA";
    statusColor = COLOR_ERROR;
    break;

  case EditFeedback::Applying:
    status = "APLICANDO CAMBIOS...";
    statusColor = COLOR_WARNING;
    break;

  case EditFeedback::ApplyFailed:
    status = "ERROR AL APLICAR";
    statusColor = COLOR_ERROR;
    break;

  case EditFeedback::None:
  default:
    break;
  }

  updateTextField(tft,
                  78,
                  154,
                  28,
                  status,
                  statusColor,
                  COLOR_PANEL);
}
