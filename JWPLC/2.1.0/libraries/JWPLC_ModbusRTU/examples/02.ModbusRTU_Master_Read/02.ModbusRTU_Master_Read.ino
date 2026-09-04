/*
  02.ModbusRTU_Master_Read

  Master Modbus RTU cooperativo: lectura FC03 de Holding Registers.

  Úselo junto con 01.ModbusRTU_Slave_Holding en otro JWPLC Basic.

  Funciones mostradas:
  - requestReadHoldingRegisters(): inicia la transacción y retorna enseguida.
  - task(): hace avanzar el motor cooperativo.
  - masterBusy()/masterDone()/masterSucceeded(): estado de la solicitud.
  - clearMasterResult(): libera el resultado para iniciar la siguiente.
*/

#include <JWPLC_ModbusRTU.h>

static constexpr uint8_t TARGET_SLAVE_ID = 2;
static uint16_t values[4] = {};
static uint32_t nextRequestMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(300);

    // 247 se usa como ID local interno del Master; el destino real es ID 2.
    if (!JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    Serial.println("JWPLC Basic - Modbus RTU Master FC03");
    Serial.println("Target ID=2 | 115200 8N1");
}

void loop()
{
    JWPLC_ModbusRTU.task();

    const uint32_t now = millis();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.print("READ OK | HR0=");
            Serial.print(values[0]);
            Serial.print(" HR1=");
            Serial.print(values[1]);
            Serial.print(" HR2=");
            Serial.print(values[2]);
            Serial.print(" HR3=");
            Serial.println(values[3]);
        }
        else
        {
            Serial.print("READ FAIL | ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRequestMs = now + 1000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextRequestMs) >= 0)
    {
        const bool accepted = JWPLC_ModbusRTU.requestReadHoldingRegisters(
            TARGET_SLAVE_ID,
            0,
            4,
            values,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
            nextRequestMs = now + 1000;
        }
    }
}
