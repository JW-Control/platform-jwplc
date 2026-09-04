/*
  ModbusRTU_Master_WriteSingleRegister

  Cliente/master Modbus RTU para escribir un holding register.

  Requiere un slave Modbus RTU con:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1

  Puedes usar otro JWPLC Basic con:
  ModbusRTU_Slave_HoldingRegisters
*/

#include <JWPLC_ModbusRTU.h>

uint16_t valueToWrite = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU master write single register");

    if (!JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    static unsigned long lastWriteMs = 0;
    unsigned long now = millis();

    if (now - lastWriteMs >= 2000)
    {
        lastWriteMs = now;
        valueToWrite++;

        bool ok = JWPLC_ModbusRTU.writeSingleRegister(1, 1, valueToWrite, 1000);

        if (ok)
        {
            Serial.print("Write OK | HR1 = ");
            Serial.println(valueToWrite);
        }
        else
        {
            Serial.print("Write failed: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }
    }
}
