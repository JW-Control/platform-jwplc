// Dibuja en host un JWPLC_HMI_Generated.h del Designer con el JWPLC_UI.cpp
// real (icon-smoke.cjs). Uso: header_sim <salida.rgb565> [estado del campo 1]
// [estado del campo 2] ... (los ids de HMIFieldId van de 1 a N en orden).
// El proyecto debe tener gráfica estática (define jwplcUIPageEnter).
#include "sim_stubs.h"

// Métodos que llama jwplcHMISetup() y no intervienen en el dibujo.
void JWPLC_DisplayClass::setUserPageCount(uint8_t) {}
void JWPLC_DisplayClass::setIdleWakeMode(IdleWakeMode) {}
void JWPLC_DisplayClass::setIdleReturnMode(IdleReturnMode) {}

// jwplcUIPageEnter() dibuja la gráfica estática con JWPLC_Display.tft().
static JWPLC_TFTClass g_tft;
JWPLC_TFTClass &JWPLC_DisplayClass::tft() { return g_tft; }

#include "JWPLC_HMI_Generated.h"

int main(int argc, char **argv)
{
  JWPLC_TFTClass &tft = g_tft;
  jwplcHMISetup();
  for (int i = 2; i < argc; ++i)
  {
    const int state = atoi(argv[i]);
    if (state >= 0) JWPLC_Display.setIconState((uint8_t)(i - 1), (uint8_t)state);
  }
  // Como el runtime: fillScreen, gráfica estática de la página y luego los campos.
  tft.fillScreen(0x0000);
  jwplcUIPageEnter(0);
  JWPLCUI::drawStatic(tft);
  JWPLCUI::drawDirty(tft);
  saveRaw(tft, argv[1]);
  return 0;
}
