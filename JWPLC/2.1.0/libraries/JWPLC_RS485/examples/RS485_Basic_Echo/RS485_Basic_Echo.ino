/*
  RS485_Basic_Echo

  Recibe bytes por RS-485 y los devuelve por el mismo puerto.
  También imprime en USB Serial lo recibido.

  Útil para probar recepción/transmisión con un conversor USB-RS485 externo.
*/

#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC RS485 basic echo test");

    if (!JWPLC_RS485.begin())
    {
        Serial.print("RS485 begin failed: ");
        Serial.println(JWPLC_RS485.lastErrorString());
        return;
    }

    JWPLC_RS485.printStatus(Serial);
    Serial.println("Waiting RS485 data...");
}

void loop()
{
    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value < 0)
        {
            return;
        }

        uint8_t b = (uint8_t)value;

        Serial.write(b);
        JWPLC_RS485.write(b);
    }
}
