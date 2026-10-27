/*
  Basic_Send

  Envia un texto por RS-485 cada segundo y muestra por Serial lo recibido.
  Transceptor con AutoDirection (por ejemplo MAX13487E) en Serial2, RX16/TX17.
*/

#include <JW_RS485.h>

JW_RS485 rs485(Serial2, 16, 17);

uint32_t counter = 0;
uint32_t lastSendMs = 0;

void setup()
{
    Serial.begin(115200);

    rs485.setAutoDirection();

    if (!rs485.begin(115200, SERIAL_8N1))
    {
        Serial.println(rs485.lastErrorString());
    }
}

void loop()
{
    if (millis() - lastSendMs >= 1000)
    {
        lastSendMs = millis();
        rs485.print("JW_RS485 ");
        rs485.println(counter++);
    }

    while (rs485.available() > 0)
    {
        Serial.write(rs485.read());
    }
}
