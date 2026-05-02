/*
  ModbusRTU_CRC_Test

  Valida el CRC16 Modbus sin requerir cable RS-485.

  Vector probado:
  Request: 01 03 00 00 00 0A
  CRC esperado: 0xCDC5
  Bytes RTU: C5 CD
*/

#include <JWPLC_ModbusRTU.h>

void printHex16(uint16_t value)
{
    if (value < 0x1000) Serial.print('0');
    if (value < 0x0100) Serial.print('0');
    if (value < 0x0010) Serial.print('0');
    Serial.print(value, HEX);
}

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Modbus RTU CRC test");

    const uint8_t request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = JWPLC_ModbusRTU.crc16(request, sizeof(request));

    Serial.print("Calculated CRC: 0x");
    printHex16(crc);
    Serial.println();

    Serial.print("Expected CRC:   0x");
    printHex16(0xCDC5);
    Serial.println();

    Serial.print("RTU CRC bytes:  ");
    Serial.print(lowByte(crc), HEX);
    Serial.print(" ");
    Serial.println(highByte(crc), HEX);

    Serial.println(crc == 0xCDC5 ? "CRC OK" : "CRC FAILED");
}

void loop()
{
}
