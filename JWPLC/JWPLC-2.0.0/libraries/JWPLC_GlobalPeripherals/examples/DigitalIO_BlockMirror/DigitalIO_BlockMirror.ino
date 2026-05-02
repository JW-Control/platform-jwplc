/*
  DigitalIO_BlockMirror

  Copia las 8 entradas hacia las 8 salidas:

    I0.0 -> Q0.0
    I0.1 -> Q0.1
    ...
    I0.7 -> Q0.7
*/

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC DigitalIO block mirror");

    digitalWriteBlock(Q0_X, 0x00);
}

void loop()
{
    uint8_t inputs = digitalReadBlock(I0_X);
    digitalWriteBlock(Q0_X, inputs);

    delay(20);
}
