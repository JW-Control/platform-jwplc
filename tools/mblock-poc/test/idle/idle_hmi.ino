// Como lo genera mBlock: pantalla «Demo» del Designer (2 páginas, MENU en
// Ajustes), con la configuración de los bloques antes de «usar la pantalla».
#include <Arduino.h>
#include "JWPLC_mBlock.h"
#include "JWPLC_mBlock_HMI.h"

void setup() {
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Screen.openWith(JWPLC_ANY_BUTTON);
  JWPLC_Screen.closeWith(JWPLC_NO_BUTTON);
  JWPLC_HMI.begin("Demo");
  JWPLC_HmiSet(estado, "LISTO");
}

void loop() {
  if (JWPLC_Keypad.pressed(BTN_OK)) {
    JWPLC_HmiSet(piezas, JWPLC_HmiGet(piezas) + 1);
  }
  if (JWPLC_Keypad.pressed(BTN_ESC)) {
    JWPLC_Display.goIdle();
  }
}
