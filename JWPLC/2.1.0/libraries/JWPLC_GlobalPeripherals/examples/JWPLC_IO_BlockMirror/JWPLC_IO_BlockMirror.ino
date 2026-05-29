/*
  JWPLC_IO_BlockMirror

  Versión estilo JWPLC:
  - JWPLC_readInputs()
  - JWPLC_writeOutputs(bitmap)
*/

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC IO block mirror");

    JWPLC_writeOutputs(0x00);
}

void loop()
{
    uint8_t inputs = JWPLC_readInputs();
    JWPLC_writeOutputs(inputs);

    delay(20);
}
