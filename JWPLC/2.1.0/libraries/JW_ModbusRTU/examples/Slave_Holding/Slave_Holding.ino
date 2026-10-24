/*
  Slave_Holding

  Slave Modbus RTU compacto de practica.

  Configuración:
  - Slave ID: 2
  - 115200 baud
  - SERIAL_8N1

  Mapa:
  - HR0: segundos desde arranque; el sketch lo actualiza.
  - HR1: registro libre para escribir desde el Master.
  - HR2..HR7: registros de práctica.

  Funciones mostradas:
  - setHoldingRegisters(): publica el mapa del Slave.
  - begin(): inicia Modbus/RS-485.
  - task(): atiende solicitudes; debe ejecutarse frecuentemente.
  - stats(): contadores de diagnóstico.
*/

#include <JW_ModbusRTU.h>

// Transceptor con AutoDirection en Serial2 (RX16/TX17).
// Para un MAX485 con DE/RE en un GPIO usar rs485.setDirectionPin(pin).
JW_RS485 rs485(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);
JW_ModbusRTU modbus(rs485);

static constexpr uint8_t SLAVE_ID = 2;
static uint16_t holding[8] = {0, 100, 200, 300, 400, 500, 600, 700};

void setup()
{
    Serial.begin(115200);
    delay(300);

    rs485.setAutoDirection();

    modbus.setHoldingRegisters(holding, 8);

    if (!modbus.begin(SLAVE_ID, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(modbus.lastErrorString());
        return;
    }

    Serial.println("JW_ModbusRTU - Modbus RTU Slave");
    Serial.println("ID=2 | 115200 8N1 | HR0..HR7");
}

void loop()
{
    // Mantener task() rápido evita bloquear o perder tramas.
    modbus.task();

    holding[0] = (uint16_t)(millis() / 1000UL);

    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs >= 1000)
    {
        lastPrintMs = millis();

        const JWModbusRTUStats &s = modbus.stats();

        Serial.print("HR0=");
        Serial.print(holding[0]);
        Serial.print(" HR1=");
        Serial.print(holding[1]);
        Serial.print(" RX=");
        Serial.print(s.rxFrames);
        Serial.print(" TX=");
        Serial.print(s.txFrames);
        Serial.print(" CRC=");
        Serial.println(s.crcErrors);
    }
}
