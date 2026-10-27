/*
  Master_Read_Async

  Master Modbus RTU cooperativo: lectura FC03 de Holding Registers.

  Úselo junto con 01.ModbusRTU_Slave_Holding en otro equipo.

  Funciones mostradas:
  - requestReadHoldingRegisters(): inicia la transacción y retorna enseguida.
  - task(): hace avanzar el motor cooperativo.
  - masterBusy()/masterDone()/masterSucceeded(): estado de la solicitud.
  - clearMasterResult(): libera el resultado para iniciar la siguiente.
*/

#include <JW_ModbusRTU.h>

// Transceptor con AutoDirection en Serial2 (RX16/TX17).
// Para un MAX485 con DE/RE en un GPIO usar rs485.setDirectionPin(pin).
JW_RS485 rs485(Serial2, 16, 17);
JW_ModbusRTU modbus(rs485);

static constexpr uint8_t TARGET_SLAVE_ID = 2;
static uint16_t values[4] = {};
static uint32_t nextRequestMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(300);

    rs485.setAutoDirection();

    // 247 se usa como ID local interno del Master; el destino real es ID 2.
    if (!modbus.begin(247, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(modbus.lastErrorString());
        return;
    }

    Serial.println("JW_ModbusRTU - Modbus RTU Master FC03");
    Serial.println("Target ID=2 | 115200 8N1");
}

void loop()
{
    modbus.task();

    const uint32_t now = millis();

    if (modbus.masterDone())
    {
        if (modbus.masterSucceeded())
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
            Serial.println(modbus.lastErrorString());
        }

        modbus.clearMasterResult();
        nextRequestMs = now + 1000;
    }

    if (!modbus.masterBusy() &&
        !modbus.masterDone() &&
        (int32_t)(now - nextRequestMs) >= 0)
    {
        const bool accepted = modbus.requestReadHoldingRegisters(
            TARGET_SLAVE_ID,
            0,
            4,
            values,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(modbus.lastErrorString());
            nextRequestMs = now + 1000;
        }
    }
}
