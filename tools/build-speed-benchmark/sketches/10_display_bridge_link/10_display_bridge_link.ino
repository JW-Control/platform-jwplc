#include <JWPLC_Display.h>

volatile bool jwplcDisplayGateSink = false;

void setup()
{
    // Fuerza la extracción del objeto principal de libJWPLC_Display.a.
    // En ESP32 genérico este enlace debe resolverse mediante jwplc-gpio-compat.c;
    // en JWPLC Basic/Core debe conservar las implementaciones reales del core.
    jwplcDisplayGateSink = JWPLCDisplay::isReady();

    JWPLCDisplay::setRunLed(false);
    JWPLCDisplay::setErrLed(false);
    JWPLCDisplay::setBusLed(false);
    JWPLCDisplay::setEthLed(false);
}

void loop()
{
}
