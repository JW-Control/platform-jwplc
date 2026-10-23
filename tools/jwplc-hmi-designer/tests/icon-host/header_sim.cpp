// Dibuja en host un JWPLC_HMI_Generated.h del Designer con el JWPLC_UI.cpp
// real (icon-smoke.cjs). Uso: header_sim <salida.rgb565> [estado del campo 1]
// [estado del campo 2] ... (los ids de HMIFieldId van de 1 a N en orden).
#include "sim_stubs.h"

// Métodos que llama jwplcHMISetup() y no intervienen en el dibujo.
void JWPLC_DisplayClass::setUserPageCount(uint8_t) {}
void JWPLC_DisplayClass::setIdleWakeMode(IdleWakeMode) {}
void JWPLC_DisplayClass::setIdleReturnMode(IdleReturnMode) {}

#include "JWPLC_HMI_Generated.h"

int main(int argc, char **argv)
{
  static JWPLC_TFTClass tft;
  jwplcHMISetup();
  for (int i = 2; i < argc; ++i)
  {
    const int state = atoi(argv[i]);
    if (state >= 0) JWPLC_Display.setIconState((uint8_t)(i - 1), (uint8_t)state);
  }
  tft.fillScreen(0x0000);
  JWPLCUI::drawStatic(tft);
  JWPLCUI::drawDirty(tft);
  saveRaw(tft, argv[1]);
  return 0;
}
