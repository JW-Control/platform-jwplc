/*
  ModbusRTU_Slave_HoldingRegisters

  Servidor/slave Modbus RTU básico para JWPLC Basic.

  Default:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1

  Funciones soportadas por la librería base:
  - 0x03 Read Holding Registers
  - 0x06 Write Single Register
  - 0x10 Write Multiple Registers
*/

#include <JWPLC_ModbusRTU.h>

uint16_t holdingRegs[16];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU slave holding registers");

    for (uint16_t i = 0; i < 16; i++)
    {
        holdingRegs[i] = 1000 + i;
    }

    JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 16);

    if (!JWPLC_ModbusRTU.begin()) // 1, 19200, SERIAL_8E1
    {
        Serial.print("Modbus begin failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }

    JWPLC_ModbusRTU.printStatus(Serial);
    Serial.println("Slave ready.");
}

void loop()
{
    JWPLC_ModbusRTU.task();

    static unsigned long lastPrintMs = 0;
    unsigned long now = millis();

    holdingRegs[0] = (uint16_t)(millis() / 1000);

    if (now - lastPrintMs >= 1000)
    {
        lastPrintMs = now;

        const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

        Serial.print("HR0: ");
        Serial.print(holdingRegs[0]);
        Serial.print(" | HR1: ");
        Serial.print(holdingRegs[1]);
        Serial.print(" | RX: ");
        Serial.print(s.rxFrames);
        Serial.print(" | TX: ");
        Serial.print(s.txFrames);
        Serial.print(" | CRC errors: ");
        Serial.print(s.crcErrors);
        Serial.print(" | Last: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }
}
