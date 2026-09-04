/*
  03.ModbusRTU_Master_Write

  Master Modbus RTU cooperativo: escritura FC06 de un Holding Register.

  Úselo junto con 01.ModbusRTU_Slave_Holding en otro JWPLC Basic.
  Cada 2 s escribe un valor nuevo en HR1 del Slave ID 2.

  Funciones mostradas:
  - requestWriteSingleRegister(): inicia FC06 sin bloquear el loop.
  - task(): hace avanzar la transacción.
  - masterDone()/masterSucceeded(): resultado.
*/

#include <JWPLC_ModbusRTU.h>

static constexpr uint8_t TARGET_SLAVE_ID = 2;
static uint16_t valueToWrite = 0;
static uint32_t nextRequestMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (!JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    Serial.println("JWPLC Basic - Modbus RTU Master FC06");
    Serial.println("Target ID=2 | HR1 | 115200 8N1");
}

void loop()
{
    JWPLC_ModbusRTU.task();

    const uint32_t now = millis();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.print("WRITE OK | HR1=");
            Serial.println(valueToWrite);
        }
        else
        {
            Serial.print("WRITE FAIL | ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRequestMs = now + 2000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextRequestMs) >= 0)
    {
        valueToWrite++;

        const bool accepted = JWPLC_ModbusRTU.requestWriteSingleRegister(
            TARGET_SLAVE_ID,
            1,
            valueToWrite,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextRequestMs = now + 2000;
        }
    }
}
