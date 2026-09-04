/*
  01.RS485_Send

  Envío básico por el puerto RS-485 integrado del JWPLC Basic.

  Hardware actual:
  - transceptor MAX13487E con autodirección;
  - no es necesario manejar manualmente DE/RE.

  Funciones mostradas:
  - begin(baud, config): inicia el puerto.
  - print()/println(): JWPLC_RS485 hereda de Stream/Print.
  - flush(): espera a que termine la transmisión pendiente.
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

    Serial.println("JWPLC Basic - RS485 send");
    JWPLC_RS485.printStatus(Serial);
}

void loop()
{
    static uint32_t counter = 0;
    static uint32_t lastSendMs = 0;

    if (millis() - lastSendMs >= 1000)
    {
        lastSendMs = millis();
        counter++;

        JWPLC_RS485.print("JWPLC mensaje #");
        JWPLC_RS485.println(counter);
        JWPLC_RS485.flush();

        Serial.print("TX #");
        Serial.println(counter);
    }
}
