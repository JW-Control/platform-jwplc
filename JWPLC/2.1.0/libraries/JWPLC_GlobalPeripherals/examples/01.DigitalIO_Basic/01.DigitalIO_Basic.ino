/*
  01.DigitalIO_Basic

  Ejemplo compacto para el taller JWPLC Basic.

  Demuestra dos niveles de acceso a las E/S:
  - JWPLC_readInputs() / JWPLC_writeOutputs(): trabajan con los 8 bits del bloque.
  - JWPLC_IO: consulta el estado cacheado por el runtime sin hacer una lectura extra.

  En este ejemplo las 8 entradas se reflejan en las 8 salidas:
  I0_0 -> Q0_0, I0_1 -> Q0_1, ... I0_7 -> Q0_7.
*/

#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Arrancar con todas las salidas apagadas.
    JWPLC_writeOutputs(0x00);

    Serial.println("JWPLC Basic - Digital I/O");
}

void loop()
{
    // Lee I0_0..I0_7 en un solo byte. Bit 0 = I0_0.
    const uint8_t inputs = JWPLC_readInputs();

    // Escribe Q0_0..Q0_7 en bloque. Bit 0 = Q0_0.
    JWPLC_writeOutputs(inputs);

    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs >= 500)
    {
        lastPrintMs = millis();

        Serial.print("IN=0x");
        if (inputs < 0x10) Serial.print('0');
        Serial.print(inputs, HEX);

        Serial.print(" OUT=0x");
        const uint8_t outputs = JWPLC_readOutputs();
        if (outputs < 0x10) Serial.print('0');
        Serial.print(outputs, HEX);

        // JWPLC_IO lee el snapshot ya actualizado por el runtime.
        Serial.print(" I0_0=");
        Serial.print(JWPLC_IO.input(0));
        Serial.print(" Q0_0=");
        Serial.println(JWPLC_IO.output(0));
    }

    delay(20);
}
