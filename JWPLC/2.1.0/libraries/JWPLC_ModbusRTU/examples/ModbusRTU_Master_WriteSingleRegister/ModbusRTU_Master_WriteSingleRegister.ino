/*
  ModbusRTU_Master_WriteSingleRegister

  Master Modbus RTU cooperativo/no bloqueante para FC06.

  Requiere un slave Modbus RTU con:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1

  Importante:
  - requestWriteSingleRegister() solo inicia la solicitud.
  - JWPLC_ModbusRTU.task() debe ejecutarse frecuentemente.
*/

#include <JWPLC_ModbusRTU.h>

static uint16_t valueToWrite = 0;
static uint32_t nextWriteMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU master cooperativo - FC06");

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
            Serial.print("Write OK | HR1 = ");
            Serial.println(valueToWrite);
        }
        else
        {
            Serial.print("Write failed: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextWriteMs = now + 2000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextWriteMs) >= 0)
    {
        valueToWrite++;

        const bool accepted = JWPLC_ModbusRTU.requestWriteSingleRegister(
            1,
            1,
            valueToWrite,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextWriteMs = now + 2000;
        }
    }
}
