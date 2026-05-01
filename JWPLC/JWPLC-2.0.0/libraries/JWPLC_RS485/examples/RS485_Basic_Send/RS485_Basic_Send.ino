/*
  RS485_Basic_Send

  Envía texto periódico por el puerto RS-485 del JWPLC Basic.

  Requiere otro dispositivo RS-485 conectado para recibir el mensaje.
*/

#include <JWPLC_RS485.h>

uint32_t counter = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC RS485 basic send test");

    if (!JWPLC_RS485.begin())
    {
        Serial.print("RS485 begin failed: ");
        Serial.println(JWPLC_RS485.lastErrorString());
        return;
    }

    JWPLC_RS485.printStatus(Serial);
}

void loop()
{
    static unsigned long lastSendMs = 0;
    unsigned long now = millis();

    if (now - lastSendMs >= 1000)
    {
        lastSendMs = now;
        counter++;

        JWPLC_RS485.print("JWPLC RS485 message #");
        JWPLC_RS485.println(counter);

        Serial.print("Sent RS485 message #");
        Serial.println(counter);
    }
}
