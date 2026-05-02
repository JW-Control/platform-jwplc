/*
  RS485_USB_Bridge

  Puente entre USB Serial y RS-485:

      Monitor Serie USB <-> Serial2 RS-485

  Sirve para depurar tramas, pantallas DWIN, sensores RS-485,
  comandos propios o pruebas previas de Modbus RTU.

  Monitor Serie recomendado:
  - 115200 baud
  - "No line ending" o el final de línea que necesite tu equipo externo.
*/

#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC RS485 USB bridge");

    if (!JWPLC_RS485.begin())
    {
        Serial.print("RS485 begin failed: ");
        Serial.println(JWPLC_RS485.lastErrorString());
        return;
    }

    JWPLC_RS485.printStatus(Serial);
    Serial.println("Bridge ready: USB Serial <-> RS485");
}

void loop()
{
    while (Serial.available() > 0)
    {
        int value = Serial.read();

        if (value >= 0)
        {
            JWPLC_RS485.write((uint8_t)value);
        }
    }

    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value >= 0)
        {
            Serial.write((uint8_t)value);
        }
    }
}
