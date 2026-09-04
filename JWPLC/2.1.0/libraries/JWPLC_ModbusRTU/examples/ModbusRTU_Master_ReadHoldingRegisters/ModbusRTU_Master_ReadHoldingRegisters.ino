/*
  ModbusRTU_Master_ReadHoldingRegisters

  Cliente/master Modbus RTU básico.

  Requiere un slave Modbus RTU con:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1
  - Holding registers disponibles desde address 0

  Puedes usar otro JWPLC Basic con el ejemplo:
  ModbusRTU_Slave_HoldingRegisters
*/

#include <JWPLC_ModbusRTU.h>

uint16_t values[4];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU master read holding registers");

    // En modo master usamos slaveId local 247 solo como ID interno no usado.
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
    static unsigned long lastReadMs = 0;
    unsigned long now = millis();

    if (now - lastReadMs >= 1000)
    {
        lastReadMs = now;

        bool ok = JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);

        if (ok)
        {
            Serial.print("Read OK | HR0: ");
            Serial.print(values[0]);
            Serial.print(" HR1: ");
            Serial.print(values[1]);
            Serial.print(" HR2: ");
            Serial.print(values[2]);
            Serial.print(" HR3: ");
            Serial.println(values[3]);
        }
        else
        {
            Serial.print("Read failed: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }
    }
}
