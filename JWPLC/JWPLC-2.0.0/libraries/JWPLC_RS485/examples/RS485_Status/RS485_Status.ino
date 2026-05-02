/*
  RS485_Status

  Valida el estado del puerto RS-485 del JWPLC Basic.

  Default:
  - Serial2
  - RX2 = IO16
  - TX2 = IO17
  - 115200, SERIAL_8N1
*/

#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC RS485 status test");

    bool ok = JWPLC_RS485.begin();

    Serial.println(ok ? "RS485 begin OK" : "RS485 begin failed");
    JWPLC_RS485.printStatus(Serial);
}

void loop()
{
    static unsigned long lastPrintMs = 0;
    unsigned long now = millis();

    if (now - lastPrintMs >= 1000)
    {
        lastPrintMs = now;

        Serial.print("RS485: ");
        Serial.print(JWPLC_RS485.statusString());
        Serial.print(" | Baud: ");
        Serial.print(JWPLC_RS485.baudRate());
        Serial.print(" | Config: ");
        Serial.println(JWPLC_RS485.configString());
    }
}
