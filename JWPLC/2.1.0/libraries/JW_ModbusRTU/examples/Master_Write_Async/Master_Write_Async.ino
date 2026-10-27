/*
  Master_Write_Async

  Master Modbus RTU cooperativo: escritura FC06 de un Holding Register.

  Úselo junto con 01.ModbusRTU_Slave_Holding en otro equipo.
  Cada 2 s escribe un valor nuevo en HR1 del Slave ID 2.

  Funciones mostradas:
  - requestWriteSingleRegister(): inicia FC06 sin bloquear el loop.
  - task(): hace avanzar la transacción.
  - masterDone()/masterSucceeded(): resultado.
*/

#include <JW_ModbusRTU.h>

// Transceptor con AutoDirection en Serial2 (RX16/TX17).
// Para un MAX485 con DE/RE en un GPIO usar rs485.setDirectionPin(pin).
JW_RS485 rs485(Serial2, 16, 17);
JW_ModbusRTU modbus(rs485);

static constexpr uint8_t TARGET_SLAVE_ID = 2;
static uint16_t valueToWrite = 0;
static uint32_t nextRequestMs = 1000;

void setup()
{
    Serial.begin(115200);
    delay(300);

    rs485.setAutoDirection();

    if (!modbus.begin(247, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(modbus.lastErrorString());
        return;
    }

    Serial.println("JW_ModbusRTU - Modbus RTU Master FC06");
    Serial.println("Target ID=2 | HR1 | 115200 8N1");
}

void loop()
{
    modbus.task();

    const uint32_t now = millis();

    if (modbus.masterDone())
    {
        if (modbus.masterSucceeded())
        {
            Serial.print("WRITE OK | HR1=");
            Serial.println(valueToWrite);
        }
        else
        {
            Serial.print("WRITE FAIL | ");
            Serial.println(modbus.lastErrorString());
        }

        modbus.clearMasterResult();
        nextRequestMs = now + 2000;
    }

    if (!modbus.masterBusy() &&
        !modbus.masterDone() &&
        (int32_t)(now - nextRequestMs) >= 0)
    {
        valueToWrite++;

        const bool accepted = modbus.requestWriteSingleRegister(
            TARGET_SLAVE_ID,
            1,
            valueToWrite,
            1000);

        if (!accepted)
        {
            Serial.print("Request rejected: ");
            Serial.println(modbus.lastErrorString());
            nextRequestMs = now + 2000;
        }
    }
}
