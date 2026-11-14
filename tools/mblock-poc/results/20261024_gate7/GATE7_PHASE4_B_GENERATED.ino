// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.6.0)

#include <Arduino.h>
#include "JWPLC_mBlock_Ethernet.h"
#include "JWPLC_mBlock_ModbusRTU.h"
#include "JWPLC_mBlock.h"

float n = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_EthernetEasy.useStaticIP("192.168.0.50", "192.168.0.1");
  JWPLC_ModbusEasy.beginMaster(115200);

}

void loop() {
  JWPLC_ModbusRTU.task();
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      n += 1;
      JWPLC_Screen.print(1, JWPLC_Join("IP ", JWPLC_EthernetEasy.ip()));
      JWPLC_Screen.print(2, JWPLC_Join("R1 esclavo: ", JWPLC_ModbusEasy.readRegister(2, 1)));
      JWPLC_ModbusEasy.writeRegister(2, 2, n);

    }
  }
}

