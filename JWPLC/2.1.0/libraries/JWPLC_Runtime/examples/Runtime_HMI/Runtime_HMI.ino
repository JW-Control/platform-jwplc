/*
  JWPLC Basic - Runtime_HMI

  El Runtime con páginas propias hechas en el HMI Designer (proyecto
  «Runtime»). El Designer genera JWPLC_HMI_Generated.h con los objetos
  enlazados a variables del Runtime (I, Q, M y bloques Bnn); basta con
  incluirlo antes de JWPLC_Runtime.begin().

    - Pantalla RUN: ► abre la primera página HMI y ◄ la última.
    - En las páginas: ◄ ► pasan de página (desde la primera o la última
      vuelven a RUN) y ESC abre el MENU del Runtime.
    - Los valores salen del programa en marcha: aquí, el de ejemplo
      (arranque a dos manos con retardo, marcha/parada, contador de piezas).

  El header de este ejemplo es el que genera el Designer: no se edita a mano.
*/

#include <JWPLC_Runtime.h>

#include "JWPLC_HMI_Generated.h"

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Runtime_HMI");

    JWPLC_Runtime.loadDemoIfEmpty(true);
    JWPLC_Runtime.begin();
}

void loop()
{
}
