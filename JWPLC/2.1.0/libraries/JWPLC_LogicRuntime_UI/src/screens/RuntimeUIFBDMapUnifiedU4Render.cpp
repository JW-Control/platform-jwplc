#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;
using namespace JWPLCUnifiedU4;

extern "C" void jwplcUnifiedWizardLinkAnchor() {}

void RuntimeUIFBDMapUnified::renderWizard(bool force)
{
  ensureU4(this, _model);
  if (!force)
    return;

  clearContentArea();
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  if (_view == View::AddType)
  {
    const uint8_t count = static_cast<uint8_t>(U4Type::Count);
    for (uint8_t index = 0; index < count; ++index)
      drawTypeChoice(static_cast<U4Type>(index),
                     static_cast<uint8_t>(gU4.type) == index);

    drawPanel(tft, 148, 34, 162, 122, "CREAR AL FINAL");
    drawFieldLabel(tft, 157, 57, "UP/DOWN TIPO");
    drawFieldLabel(tft, 157, 76, "OK CONFIGURAR");
    drawFieldLabel(tft, 157, 95, "ESC VOLVER");
    drawFieldLabel(tft, 157, 122, "RAM / SIN FRAM", COLOR_ACCENT);
    return;
  }

  if (_view == View::Configure)
  {
    char typeLabel[28];
    std::snprintf(typeLabel, sizeof(typeLabel), "TIPO: %s", typeName(gU4.type));
    drawFieldLabel(tft, 10, 31, typeLabel, COLOR_ACCENT, COLOR_PANEL);
    drawMainGroup(U4MainFocus::Sources);
    drawMainGroup(U4MainFocus::Parameters);
    drawMainGroup(U4MainFocus::Create);

    if (gU4.mainFocus == U4MainFocus::Sources)
      drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, true, gU4.sourceA, nullptr);
    else if (gU4.mainFocus == U4MainFocus::Parameters)
    {
      char summary[48];
      if (gU4.type == U4Type::Ton)
      {
        char value[16];
        formatTonConfigured(value, sizeof(value));
        std::snprintf(summary, sizeof(summary), "T CONFIGURADO: %s", value);
      }
      else
      {
        char value[20];
        formatResource(value, sizeof(value));
        std::snprintf(summary, sizeof(summary), "RECURSO: %s", value);
      }
      drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, false, 0, summary, COLOR_ACCENT);
    }
    else
      drawContextPanel(_levels, _lanes, _maxLevel, CONTEXT_Y, CONTEXT_H, false, 0,
                       "LISTO PARA CREAR - RAM / SIN FRAM", COLOR_ACCENT);

    drawFooter(gU4.error ? "CONFIGURACION NO VALIDA"
                         : "LEFT/RIGHT CAMPO   OK ENTRAR   ESC ATRAS",
               gU4.error ? COLOR_ERROR : COLOR_MUTED);
    return;
  }

  if (_view == View::SourceList)
  {
    drawFieldLabel(tft, 10, 31, "SELECCIONA UNA ENTRADA", COLOR_ACCENT, COLOR_PANEL);
    const uint8_t count = sourceCountForType(gU4.type);
    for (uint8_t input = 0; input < count; ++input)
      drawListRow(input, input == gU4.sourceInput);
    drawContextPanel(_levels,
                     _lanes,
                     _maxLevel,
                     sourceCountForType(gU4.type) > 1 ? 100 : CONTEXT_Y,
                     sourceCountForType(gU4.type) > 1 ? 50 : CONTEXT_H,
                     true,
                     sourceValue(gU4.sourceInput),
                     nullptr);
    drawFooter("UP/DOWN ENTRADA   OK EDITAR   ESC ATRAS");
    return;
  }

  if (_view == View::SourceEdit)
  {
    drawFieldLabel(tft, 10, 34, "UP/DOWN CAMBIAR FUENTE", COLOR_MUTED, COLOR_PANEL);
    char source[28];
    formatSource(_model, sourceValue(gU4.sourceInput), source, sizeof(source));
    tft.fillRoundRect(10, 58, 300, 36, 4, COLOR_SELECTED);
    tft.drawRoundRect(10, 58, 300, 36, 4, COLOR_WARNING);
    tft.drawRoundRect(11, 59, 298, 34, 3, COLOR_WARNING);
    drawFieldLabel(tft, 18, 65, "FUENTE", COLOR_WARNING, COLOR_SELECTED);
    updateTextField(tft, 112, 75, 28, source, COLOR_WARNING, COLOR_SELECTED);
    drawContextPanel(_levels,
                     _lanes,
                     _maxLevel,
                     100,
                     50,
                     true,
                     sourceValue(gU4.sourceInput),
                     nullptr);
    drawFooter("OK ACEPTAR   ESC CANCELAR");
    return;
  }

  if (_view == View::ParameterList)
  {
    drawFieldLabel(tft, 10, 31, "SELECCIONA PARAMETRO", COLOR_ACCENT, COLOR_PANEL);
    char value[24];
    if (gU4.type == U4Type::Ton)
      formatTonConfigured(value, sizeof(value));
    else
      formatResource(value, sizeof(value));
    char label[48];
    std::snprintf(label, sizeof(label), "%s  <%s>", parameterName(gU4.type), value);
    drawMenuButton(tft, LIST_X, LIST_Y, LIST_W, LIST_ROW_H, label, true);
    drawContextPanel(_levels,
                     _lanes,
                     _maxLevel,
                     CONTEXT_Y,
                     CONTEXT_H,
                     false,
                     0,
                     gU4.type == U4Type::Ton
                         ? "TEMPORIZACION DEL NUEVO TON"
                         : "ASIGNACION DE RECURSO FISICO",
                     COLOR_ACCENT);
    drawFooter("OK EDITAR   ESC ATRAS");
    return;
  }

  if (_view == View::ParameterEdit)
  {
    if (gU4.type != U4Type::Ton)
    {
      drawFieldLabel(tft, 10, 36, "UP/DOWN CAMBIAR RECURSO", COLOR_MUTED, COLOR_PANEL);
      char value[20];
      formatResource(value, sizeof(value));
      drawMenuButton(tft, 10, 62, 300, 36, value, true);
      drawContextPanel(_levels, _lanes, _maxLevel, 104, 46, false, 0,
                       "RECURSO DEL NUEVO BLOQUE", COLOR_ACCENT);
      drawFooter("OK ACEPTAR   ESC CANCELAR");
      return;
    }

    drawFieldLabel(tft, 10, 34,
                   "LEFT/RIGHT CAMPO   UP/DOWN CAMBIAR",
                   COLOR_MUTED,
                   COLOR_PANEL);
    char configured[16];
    formatTonConfigured(configured, sizeof(configured));
    drawFieldLabel(tft, 10, 68, "T CONFIGURADO", COLOR_TEXT, COLOR_PANEL);
    updateTextField(tft, 112, 70, 16, configured, COLOR_WARNING, COLOR_PANEL);

    char major[12], minor[12];
    formatTwoDigit(gU4.tonMajor, major, sizeof(major));
    formatTwoDigit(gU4.tonMinor, minor, sizeof(minor));
    drawTonField(0, tonMajorLabel(), major,
                 gU4.tonFocus == U4TonField::Major, true);
    drawTonField(1, tonMinorLabel(), minor,
                 gU4.tonFocus == U4TonField::Minor, true);
    drawTonField(2, "BASE", tonBaseText(),
                 gU4.tonFocus == U4TonField::Base, true);
    drawFooter("OK ACEPTAR   ESC CANCELAR");
  }
}
