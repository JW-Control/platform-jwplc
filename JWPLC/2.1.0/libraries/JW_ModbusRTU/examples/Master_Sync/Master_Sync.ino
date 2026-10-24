/*
  Master_Sync

  Master Modbus RTU con la API bloqueante explicita (*Sync).
  Cada llamada termina cuando el esclavo responde o vence el timeout.
  Util para pantallas HMI o puesta en marcha, donde el loop usa el valor
  leido en la misma vuelta.

  Úselo junto con Slave_Holding en otro equipo (ID 2, 115200 8N1).
*/

#include <JW_ModbusRTU.h>

JW_RS485 rs485(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);
JW_ModbusRTU modbus(rs485);

static constexpr uint8_t TARGET_SLAVE_ID = 2;
static constexpr uint32_t TIMEOUT_MS = 200;

void setup()
{
    Serial.begin(115200);
    delay(300);

    rs485.setAutoDirection();

    // Transporte bloqueante: no se usa la cola TX de AutoDirection.
    modbus.setQueuedTxEnabled(false);

    if (!modbus.begin(247, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(modbus.lastErrorString());
    }
}

void loop()
{
    uint16_t values[2] = {};

    if (modbus.readHoldingRegistersSync(TARGET_SLAVE_ID, 0, 2, values, TIMEOUT_MS))
    {
        Serial.print("HR0=");
        Serial.print(values[0]);
        Serial.print(" HR1=");
        Serial.println(values[1]);

        modbus.writeSingleRegisterSync(TARGET_SLAVE_ID, 1, values[1] + 1, TIMEOUT_MS);
    }
    else
    {
        Serial.print("READ FAIL | ");
        Serial.println(modbus.lastErrorString());
    }

    delay(1000);
}
