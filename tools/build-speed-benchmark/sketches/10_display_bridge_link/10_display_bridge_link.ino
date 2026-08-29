#include <JWPLC_Display_Auto.h>
#include <JWPLC_Display.h>

volatile bool jwplcDisplayGateSink = false;

void setup()
{
    // Fuerza la extracción del objeto principal de libJWPLC_Display.a.
    // En ESP32 genérico este enlace debe resolverse mediante jwplc-gpio-compat.c;
    // en JWPLC Basic/Core debe conservar las implementaciones reales del core.
    //
    // JWPLC_Display_Auto.h se incluye primero para que library discovery
    // seleccione las copias Adafruit bundled del package JWPLC y no variantes
    // homónimas instaladas en el sketchbook.
    jwplcDisplayGateSink = JWPLCDisplay::isReady();

    JWPLCDisplay::setRunLed(false);
    JWPLCDisplay::setErrLed(false);
    JWPLCDisplay::setBusLed(false);
    JWPLCDisplay::setEthLed(false);
}

void loop()
{
}
