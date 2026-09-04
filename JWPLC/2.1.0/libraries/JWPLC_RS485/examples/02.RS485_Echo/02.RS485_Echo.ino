/*
  02.RS485_Echo

  Eco básico por RS-485.

  Todo byte recibido se devuelve inmediatamente al mismo bus y también se
  muestra por USB Serial para verificar la comunicación.

  Para probarlo, conecte otro equipo RS-485 que envíe texto a 115200 8N1.
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

    Serial.println("JWPLC Basic - RS485 echo");
}

void loop()
{
    while (JWPLC_RS485.available() > 0)
    {
        const int value = JWPLC_RS485.read();
        if (value < 0)
            break;

        // Mostrar por USB lo recibido.
        Serial.write((uint8_t)value);

        // Devolver el mismo byte al bus RS-485.
        JWPLC_RS485.write((uint8_t)value);
    }
}
