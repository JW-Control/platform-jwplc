// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.7.0)

#include <Arduino.h>
#include "JWPLC_mBlock_HMI.h"
#include "JWPLC_mBlock.h"

void setup() {
  JWPLC_HMI.begin("Demo");
  JWPLC_HmiSet(estado, "LISTO");

}

void loop() {
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      JWPLC_HmiSet(temperatura, JWPLC_Clock.temperature());
      JWPLC_HmiSet(piezas, JWPLC_Timer.seconds());
      JWPLC_HmiSet(nivel, fmod(JWPLC_Timer.seconds(), 100));
      JWPLC_HmiSet(bomba, (digitalRead(I0_0) == HIGH));
      if(((JWPLC_HmiGet(modoAuto) != 0))  &&  (JWPLC_Clock.temperature() > JWPLC_HmiGet(consigna))){
        digitalWrite(Q0_0, HIGH);
        JWPLC_HmiSet(estado, "CALIENTE");

      }else{
        digitalWrite(Q0_0, LOW);
        JWPLC_HmiSet(estado, "LISTO");

      }

    }
  }
}

