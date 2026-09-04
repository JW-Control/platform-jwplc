/*
  ModbusRTU_Master_ReadHoldingRegisters

  Master Modbus RTU cooperativo/no bloqueante (API recomendada).

  Requiere un slave Modbus RTU con:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1
  - Holding registers disponibles desde address 0

  Importante:
  - requestReadHoldingRegisters() solo inicia la solicitud.
  - JWPLC_ModbusRTU.task() debe ejecutarse frecuentemente.
  - values[] debe existir hasta que masterDone() sea true.
*/

#include <JWPLC_ModbusRTU.h>

static uint16_t values[4];
static uint32_t nextReadMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU master cooperativo - FC03");

    // En modo master usamos slaveId local 247 como ID interno no usado.
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
    JWPLC_ModbusRTU.task();

    const uint32_t now = millis();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
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

        JWPLC_ModbusRTU.clearMasterResult();
        nextReadMs = now + 1000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextReadMs) >= 0)
    {
        const bool accepted = JWPLC_ModbusRTU.requestReadHoldingRegisters(
            1,
            0,
            4,
            values,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextReadMs = now + 1000;
        }
    }
}
