/*
  A14 H3E.4A1 - JWPLC_TFT shape primitives compile probe

  Compile-only. Fuerza API y link de las cuatro primitivas necesarias por
  JWPLC_LogicRuntime_UI sin exponer tipos del backend.
*/

#include <Arduino.h>
#include <JWPLC_TFT.h>

#ifdef _TFT_eSPIH_
#error "H3E4A1: TFT_eSPI se filtro a la API publica"
#endif

static bool drawShapeProbe()
{
    if (!JWPLC_TFT.beginBatch())
    {
        return false;
    }

    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    JWPLC_TFT.fillRoundRect(
        10,
        10,
        90,
        52,
        10,
        JWPLC_TFT_BLUE);

    JWPLC_TFT.drawRoundRect(
        10,
        10,
        90,
        52,
        10,
        JWPLC_TFT_WHITE);

    JWPLC_TFT.fillCircle(
        160,
        36,
        24,
        JWPLC_TFT_GREEN);

    JWPLC_TFT.drawCircle(
        250,
        36,
        24,
        JWPLC_TFT_YELLOW);

    JWPLC_TFT.endBatch();
    return true;
}

void setup()
{
    (void)JWPLC_TFT.begin();
    (void)drawShapeProbe();
}

void loop()
{
}
