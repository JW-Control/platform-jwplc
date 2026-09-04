/*
  03.RS485_Status

  Estado y telemetría del transporte RS-485.

  Funciones mostradas:
  - isReady(), baudRate(), configString().
  - lastRxActivityMs()/lastTxActivityMs().
  - hasRecentActivity(windowMs).
  - printStatus().

  El ejemplo envía un PING cada 2 s para generar actividad TX.
*/

#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (!JWPLC_RS485.begin(115200, SERIAL_8N1))
    {
        Serial.print("RS485 begin failed: ");
        Serial.println(JWPLC_RS485.lastErrorString());
        return;
    }

    Serial.println("JWPLC Basic - RS485 status");
    JWPLC_RS485.printStatus(Serial);
}

void loop()
{
    // Consumir RX para que la telemetría registre actividad recibida.
    while (JWPLC_RS485.available() > 0)
    {
        const int value = JWPLC_RS485.read();
        if (value >= 0)
            Serial.write((uint8_t)value);
    }

    static uint32_t lastStatusMs = 0;
    if (millis() - lastStatusMs >= 2000)
    {
        lastStatusMs = millis();

        JWPLC_RS485.println("PING");

        Serial.println();
        Serial.print("ready=");
        Serial.print(JWPLC_RS485.isReady());
        Serial.print(" config=");
        Serial.print(JWPLC_RS485.configString());
        Serial.print(" recent2s=");
        Serial.print(JWPLC_RS485.hasRecentActivity(2000));
        Serial.print(" lastRX=");
        Serial.print(JWPLC_RS485.lastRxActivityMs());
        Serial.print(" lastTX=");
        Serial.println(JWPLC_RS485.lastTxActivityMs());
    }
}
