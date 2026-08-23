#include <JW_SD.h>

JW_SD sd;

void setup()
{
    // Fuerza el enlace de JW_SD sin depender de hardware presente.
    // Con _detectPin=-1 por defecto, isCardPresent() retorna true y no toca GPIO.
    // El objetivo de esta prueba es validar que JW_SD se compile/enlace desde fuente
    // en el target generico ESP32 Board sin referencias jwplc_* no resueltas.
    (void)sd.isCardPresent();
}

void loop()
{
}
