/*
  A14 H3E.2A - JWPLC_TFT public API compile probe

  Compile-only. Demuestra que el sketch usa exclusivamente JWPLC_TFT.h y
  que ningun tipo del backend TFT_eSPI queda expuesto por el header publico.
*/

#include <Arduino.h>
#include <JWPLC_TFT.h>

#ifdef _TFT_eSPIH_
#error "H3E2A: TFT_eSPI se filtro al sketch por JWPLC_TFT.h"
#endif

static_assert(
    JWPLC_TFT_VERSION_MAJOR == 0 &&
        JWPLC_TFT_VERSION_MINOR == 1 &&
        JWPLC_TFT_VERSION_PATCH == 0,
    "H3E2A: version publica JWPLC_TFT inesperada");

void setup()
{
    const JWPLC_TFTPanelInfo info =
        JWPLC_TFT.panelInfo();

    (void)info;
    (void)JWPLC_TFT.panel();
    (void)JWPLC_TFT.isReady();
    (void)JWPLC_TFT.width();
    (void)JWPLC_TFT.height();
    (void)JWPLC_TFT.rotation();

    (void)JWPLC_TFT.begin();

    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE,
        JWPLC_TFT_BLACK);
    JWPLC_TFT.setTextWrap(false);
    JWPLC_TFT.setCursor(8, 8);

    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    JWPLC_TFT.getTextBounds(
        "JWPLC",
        0,
        0,
        &x1,
        &y1,
        &w,
        &h);

    (void)JWPLC_TFT.textWidth("JWPLC");
    (void)JWPLC_TFT.fontHeight();

    if (JWPLC_TFT.beginBatch())
    {
        JWPLC_TFT.fillScreen(
            JWPLC_TFT_BLACK);
        JWPLC_TFT.fillRect(
            0,
            0,
            20,
            20,
            JWPLC_TFT_RED);
        JWPLC_TFT.drawRect(
            2,
            2,
            16,
            16,
            JWPLC_TFT_WHITE);
        JWPLC_TFT.drawFastHLine(
            0,
            24,
            32,
            JWPLC_TFT_GREEN);
        JWPLC_TFT.drawFastVLine(
            36,
            0,
            24,
            JWPLC_TFT_BLUE);
        JWPLC_TFT.drawLine(
            0,
            0,
            10,
            10,
            JWPLC_TFT_YELLOW);
        JWPLC_TFT.drawPixel(
            1,
            1,
            JWPLC_TFT_CYAN);

        JWPLC_TFT.setCursor(40, 8);
        JWPLC_TFT.print("JWPLC");
        JWPLC_TFT.printf(" %d", 2);

        JWPLC_TFT.endBatch();
    }
}

void loop()
{
}
