// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.6.0)

#include <Arduino.h>
#include "JWPLC_mBlock_ModbusRTU.h"
#include "JWPLC_mBlock.h"

float n = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_ModbusEasy.beginSlave(2, 115200);

}

void loop() {
  JWPLC_ModbusRTU.task();
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      JWPLC_Registers.set(1, JWPLC_Timer.seconds());
      JWPLC_Screen.print(1, JWPLC_Join("R2 = ", JWPLC_Registers.get(2)));
      if(fmod(JWPLC_Registers.get(2), 2) == 1.000000){
        digitalWrite(Q0_0, HIGH);

      }else{
        digitalWrite(Q0_0, LOW);

      }

    }
  }
}

