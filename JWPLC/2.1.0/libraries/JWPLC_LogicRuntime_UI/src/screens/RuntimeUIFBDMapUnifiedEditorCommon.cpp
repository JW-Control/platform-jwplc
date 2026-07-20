#include "RuntimeUIFBDMapUnified.h"

#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

void RuntimeUIFBDMapUnified::invalidateEditorCaches()
{
  _inputEditorCacheValid = false;
  _inputFocusCache = InputEditField::Source;
  _inputSourceCache[0] = '\0';
  _inputInvertedCache = false;

  _tonEditorCacheValid = false;
  _tonMajorCache = 0;
  _tonMinorCache = 0;
  _tonBaseCache = TonBase::Seconds;
  _tonFocusCache = TonField::Major;
  _tonEditorElapsedCache[0] = '\0';
  _tonEditorElapsedColorCache = false;
}

void RuntimeUIFBDMapUnified::drawEditorFooter()
{
  const char *text = "OK GUARDAR   ESC CANCELAR";
  uint16_t color = COLOR_MUTED;

  switch (_editFeedback)
  {
  case EditFeedback::Applying:
    text = "APLICANDO CAMBIOS...";
    color = COLOR_WARNING;
    break;
  case EditFeedback::Failed:
    text = "ERROR: CAMBIO NO APLICADO";
    color = COLOR_ERROR;
    break;
  case EditFeedback::InvalidBase:
    text = "BASE NO EXACTA";
    color = COLOR_WARNING;
    break;
  case EditFeedback::None:
  default:
    break;
  }

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  tft.drawFastHLine(CONTENT_X + 2,
                    EDIT_FOOTER_Y - 4,
                    CONTENT_W - 4,
                    COLOR_BORDER);
  updateTextField(tft,
                  CONTENT_X + 6,
                  EDIT_FOOTER_Y,
                  50,
                  text,
                  color,
                  COLOR_PANEL);
}
