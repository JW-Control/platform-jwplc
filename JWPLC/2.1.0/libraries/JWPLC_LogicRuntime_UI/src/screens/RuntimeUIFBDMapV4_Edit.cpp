#include "RuntimeUIFBDMapV4.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

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

  formatBlockId(id, idCapacity, source);
  formatMapLine2(type, typeCapacity, source);
}

void RuntimeUIFBDMapV4::drawInputEdit()
{
  if (_model == nullptr)
  {
    return;
  }

  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr || _detailInputIndex >= definition->inputCount)
  {
    return;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  clearMapArea();

  char headerInfo[28];
  std::snprintf(headerInfo,
                sizeof(headerInfo),
                "B%02u %s IN%u/%u",
                static_cast<unsigned>(_selectedIndex),
                _model->typeShort(definition->type),
                static_cast<unsigned>(_detailInputIndex + 1U),
                static_cast<unsigned>(definition->inputCount));
  updateTextField(tft,
                  HEADER_INFO_X,
                  HEADER_INFO_Y,
                  HEADER_INFO_COLUMNS,
                  headerInfo,
                  COLOR_WARNING,
                  COLOR_PANEL);

  char sourceId[12];
  char sourceType[14];
  formatEditSource(sourceId,
                   sizeof(sourceId),
                   sourceType,
                   sizeof(sourceType));

  const int16_t sourceX = 16;
  const int16_t sourceY = 52;
  const int16_t sourceW = 102;
  const int16_t sourceH = 58;
  const int16_t blockX = 190;
  const int16_t blockY = 45;
  const int16_t blockW = 108;
  const int16_t blockH = 76;
  const int16_t wireY = 81;

  tft.fillRect(sourceX, sourceY, sourceW, sourceH, COLOR_BACKGROUND);
  tft.drawRect(sourceX, sourceY, sourceW, sourceH, COLOR_WARNING);
  tft.drawRect(sourceX + 1, sourceY + 1, sourceW - 2, sourceH - 2, COLOR_WARNING);

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(sourceX + 7, sourceY + 8);
  tft.print("FUENTE");
  tft.setTextSize(2);
  tft.setCursor(sourceX + 8, sourceY + 23);
  tft.print(sourceId);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  tft.setCursor(sourceX + 45, sourceY + 28);
  tft.print(sourceType);

  tft.fillRect(blockX, blockY, blockW, blockH, COLOR_BACKGROUND);
  tft.drawRect(blockX, blockY, blockW, blockH, COLOR_BORDER);
  tft.drawFastVLine(blockX + 24, blockY + 2, blockH - 4, COLOR_BORDER);

  char blockId[8];
  formatBlockId(blockId, sizeof(blockId), _selectedIndex);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  tft.setCursor(blockX + 34, blockY + 8);
  tft.print(blockId);
  tft.setTextSize(2);
  tft.setCursor(blockX + 48, blockY + 29);
  tft.print(detailSymbol(definition->type));

  const char *role = _model->inputRole(definition->type, _detailInputIndex);
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
  tft.setTextColor(COLOR_WARNING, COLOR_BACKGROUND);
  tft.setCursor(blockX + 8, wireY - 3);
  tft.print(roleText);

  tft.drawFastHLine(sourceX + sourceW,
                    wireY,
                    blockX - (sourceX + sourceW),
                    COLOR_WARNING);

  if (_editInverted)
  {
    tft.fillCircle(blockX + 1, wireY, 5, COLOR_BACKGROUND);
    tft.drawCircle(blockX + 1, wireY, 4, COLOR_MUTED);
  }
  else
  {
    tft.fillCircle(blockX, wireY, 2, COLOR_WARNING);
  }

  tft.setTextSize(1);
  tft.setTextColor(_editInverted ? COLOR_WARNING : COLOR_MUTED,
                   COLOR_PANEL);
  tft.setCursor(18, 124);
  tft.print(_editInverted ? "LOGICA: NEGADA" : "LOGICA: NORMAL");

  if (!_lastApplySuccess)
  {
    tft.setTextColor(COLOR_ERROR, COLOR_PANEL);
    tft.setCursor(198, 126);
    tft.print("CAMBIO INVALIDO");
  }
  else
  {
    tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
    tft.setCursor(174, 126);
    tft.print("OK APLICAR");
  }

  tft.setTextColor(COLOR_MUTED, COLOR_PANEL);
  tft.setCursor(13, 151);
  tft.print("UP/DN FUENTE  LEFT INV  RIGHT ATRAS");
}
