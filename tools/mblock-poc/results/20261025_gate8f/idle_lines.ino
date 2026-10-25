// Como lo genera mBlock: 4 líneas, abrir con OK, volver tras 20 s, botones.
#include <Arduino.h>
#include "JWPLC_mBlock.h"

float pulsaciones = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Screen.openWith(BTN_OK);
  JWPLC_Screen.closeAfter(20);
  JWPLC_Screen.print(1, "Hola");
}

void loop() {
  if (JWPLC_Keypad.pressed(BTN_UP)) {
    pulsaciones = pulsaciones + 1;
    JWPLC_Screen.print(2, JWPLC_Join("Arriba: ", pulsaciones));
  }
  if (JWPLC_Keypad.pressed(BTN_ESC)) {
    JWPLC_Screen.print(3, "ESC del programa");
  }
  digitalWrite(Q0_0, JWPLC_Keypad.isDown(BTN_DOWN));
  JWPLC_Screen.print(4, JWPLC_Screen.isShown());
}
