/*
  DigitalIO_BlockRead

  Lee las 8 entradas I0.0 a I0.7 en una sola llamada.
*/

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC DigitalIO block read");
}

void loop()
{
    uint8_t inputs = digitalReadBlock(I0_X);

    Serial.print("IN: 0b");

    for (int8_t bit = 7; bit >= 0; bit--)
    {
        Serial.print((inputs & (1u << bit)) ? '1' : '0');
    }

    Serial.println();
    delay(500);
}
