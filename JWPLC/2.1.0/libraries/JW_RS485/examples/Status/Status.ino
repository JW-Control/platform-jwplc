/*
  Status

  Inicia dos puertos RS-485 independientes e imprime su estado.
*/

#include <JW_RS485.h>

JW_RS485 busA(Serial1, 26, 25);
JW_RS485 busB(Serial2, 16, 17);

void setup()
{
    Serial.begin(115200);

    busA.setDirectionPin(27);
    busA.begin(19200, SERIAL_8E1);

    busB.setAutoDirection();
    busB.begin(115200, SERIAL_8N1);

    Serial.println("== Bus A ==");
    busA.printStatus(Serial);
    Serial.println("== Bus B ==");
    busB.printStatus(Serial);
}

void loop()
{
}
