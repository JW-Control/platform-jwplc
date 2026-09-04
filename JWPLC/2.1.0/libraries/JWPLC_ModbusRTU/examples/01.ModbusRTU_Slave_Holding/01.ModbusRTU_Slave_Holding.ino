/*
  01.ModbusRTU_Slave_Holding

  Slave Modbus RTU compacto para el taller JWPLC Basic.

  Configuración usada por la serie numerada:
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

#include <JWPLC_ModbusRTU.h>

static constexpr uint8_t SLAVE_ID = 2;
static uint16_t holding[8] = {0, 100, 200, 300, 400, 500, 600, 700};

void setup()
{
    Serial.begin(115200);
    delay(300);

    JWPLC_ModbusRTU.setHoldingRegisters(holding, 8);

    if (!JWPLC_ModbusRTU.begin(SLAVE_ID, 115200, SERIAL_8N1))
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    Serial.println("JWPLC Basic - Modbus RTU Slave");
    Serial.println("ID=2 | 115200 8N1 | HR0..HR7");
}

void loop()
{
    // Mantener task() rápido evita bloquear o perder tramas.
    JWPLC_ModbusRTU.task();

    holding[0] = (uint16_t)(millis() / 1000UL);

    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs >= 1000)
    {
        lastPrintMs = millis();

        const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

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
