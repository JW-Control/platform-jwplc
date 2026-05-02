/*
  DigitalIO_BlockWrite

  Escribe las 8 salidas Q0.0 a Q0.7 en una sola llamada.

  Importante:
  - Este ejemplo activa relés físicos.
  - No conectar cargas peligrosas durante la prueba.
*/

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC DigitalIO block write");

    digitalWriteBlock(Q0_X, 0x00);
}

void loop()
{
    digitalWriteBlock(Q0_X, 0b10001000);
    delay(1000);

    digitalWriteBlock(Q0_X, 0b00010001);
    delay(1000);

    digitalWriteBlock(Q0_X, 0x00);
    delay(1000);
}
