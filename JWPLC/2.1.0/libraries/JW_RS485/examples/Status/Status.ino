/*
  Status

  Inicia el puerto RS-485 integrado del JWPLC Basic e imprime su estado.

  El JWPLC Basic tiene un solo transceptor RS-485 (MAX13487E con
  AutoDirection) en Serial2. Los pines salen de jwplc_hardware_config.h:
  JWPLC_RS485_RX_PIN (GPIO16) y JWPLC_RS485_TX_PIN (GPIO17).

  Nota: no usar GPIO25/26/27 para un segundo bus: son TFT DC, buzzer y
  EN_IO (habilitación del expansor de E/S).
*/

#include <JW_RS485.h>

JW_RS485 bus(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);

void setup()
{
    Serial.begin(115200);

    bus.setAutoDirection();
    bus.begin(115200, SERIAL_8N1);

    Serial.println("== RS-485 ==");
    bus.printStatus(Serial);
}

void loop()
{
}
