// Como lo genera mBlock: pantalla «Demo» y dos bloques «cuando el operador
// cambie [variable] en el MENU» del mismo MENU (Ajustes: consigna y modoAuto).
#include <Arduino.h>
#include "JWPLC_mBlock_HMI.h"
#include "JWPLC_mBlock.h"

float cambios = 0;

void setup() {
  JWPLC_HMI.begin("Demo");
  JWPLC_HmiSet(estado, "LISTO");
}

void loop() {
  if (JWPLC_HMI.changed(&consigna)) {
    cambios = cambios + 1;
    JWPLC_HmiSet(piezas, cambios);
    JWPLC_HmiSet(estado, JWPLC_Join("Consigna ", JWPLC_HmiGet(consigna)));
    JWPLC_Sound.play(880, 0.1);
  }
  if (JWPLC_HMI.changed(&modoAuto)) {
    JWPLC_HmiSet(estado, (JWPLC_HmiGet(modoAuto) != 0) ? "AUTO" : "MANUAL");
    JWPLC_Sound.play(440, 0.1);
  }
}
