// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.6.0)

#include <Arduino.h>
#include "JWPLC_mBlock_Ethernet.h"
#include "JWPLC_mBlock_ModbusTCP.h"
#include "JWPLC_mBlock_RS485.h"
#include "JWPLC_mBlock.h"

float n = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_EthernetEasy.useDHCP();
  JWPLC_ModbusTcpEasy.beginServer();
  JWPLC_RS485.begin(115200, SERIAL_8N1);

}

void loop() {
  if (JWPLC_Line485.available()) {
    JWPLC_Screen.print(2, JWPLC_Line485.line());

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      JWPLC_Registers.set(1, JWPLC_Timer.seconds());
      JWPLC_Screen.print(1, JWPLC_Join("IP ", JWPLC_EthernetEasy.ip()));
      JWPLC_Screen.print(3, JWPLC_Join("R2 = ", JWPLC_Registers.get(2)));
      JWPLC_Line485.send(JWPLC_Join("t=", JWPLC_Registers.get(1)));

    }
  }
}

