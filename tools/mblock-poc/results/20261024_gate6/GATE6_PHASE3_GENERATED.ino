// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.5.0)

#include <Arduino.h>
#include "JWPLC_mBlock_WiFi.h"
#include "JWPLC_mBlock.h"

float cuenta = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Net.channel(1);
  JWPLC_Screen.print(1, "Conectando...");
  JWPLC_Net.connect("MiRed", "clave");
  JWPLC_Screen.print(1, JWPLC_Join("IP ", JWPLC_Net.ip()));

}

void loop() {
  if (JWPLC_Net.received("contador")) {
    JWPLC_Screen.print(3, JWPLC_Join("Recibido ", JWPLC_Net.value("contador")));

  }
  if (JWPLC_Buttons.pressed(BTN_OK)) {
    JWPLC_Net.send("boton");

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((2) * 1000)) {
      _jw_last = millis();
      if(JWPLC_Net.connected()){
        cuenta += 1;
        JWPLC_Net.send("contador", cuenta);
        JWPLC_Screen.print(2, JWPLC_Join("Enviado ", cuenta));

      }

    }
  }
  if (JWPLC_Net.received("boton")) {
    JWPLC_Buzzer.beep();
    JWPLC_Screen.print(4, "Llego boton");

  }
}

