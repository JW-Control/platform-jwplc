/*
  A14 H3E.2B2 - JWPLC_TFT precompiled physical qualification

  Gate diagnostico. No es firmware de produccion.

  El sketch solo conoce la API publica JWPLC_TFT. El backend queda dentro del
  archive precompilado temporal generado por H3E.2B1.
*/

#include <Arduino.h>
#include <JWPLC_TFT.h>

#ifdef _TFT_eSPIH_
#error "H3E2B2: TFT_eSPI se filtro a la API publica"
#endif

#ifndef JWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD
#error "H3E2B2: bypass diagnostico de Display no definido"
#endif

static bool drawQualificationScreen()
{
    if (!JWPLC_TFT.beginBatch(2000))
    {
        return false;
    }

    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    // Marco completo: detecta clipping/offset en cualquier borde.
    JWPLC_TFT.drawRect(
        0,
        0,
        320,
        170,
        JWPLC_TFT_WHITE);

    // Esquinas con identidad visual y colores conocidos.
    JWPLC_TFT.fillRect(
        2,
        2,
        26,
        18,
        JWPLC_TFT_RED);
    JWPLC_TFT.fillRect(
        292,
        2,
        26,
        18,
        JWPLC_TFT_GREEN);
    JWPLC_TFT.fillRect(
        2,
        150,
        26,
        18,
        JWPLC_TFT_BLUE);
    JWPLC_TFT.fillRect(
        292,
        150,
        26,
        18,
        JWPLC_TFT_WHITE);

    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE,
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(34, 4);
    JWPLC_TFT.print("TL RED");
    JWPLC_TFT.setCursor(238, 4);
    JWPLC_TFT.print("TR GREEN");

    JWPLC_TFT.setCursor(34, 156);
    JWPLC_TFT.print("BL BLUE");
    JWPLC_TFT.setCursor(238, 156);
    JWPLC_TFT.print("BR WHITE");

    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setCursor(42, 42);
    JWPLC_TFT.print("JWPLC_TFT PRECOMP");

    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setCursor(94, 70);
    JWPLC_TFT.print("320x170  ROT=1");
    JWPLC_TFT.setCursor(84, 84);
    JWPLC_TFT.print("80MHz  SPI MODE0");

    // Barras centrales R-G-B.
    JWPLC_TFT.fillRect(
        82,
        104,
        48,
        20,
        JWPLC_TFT_RED);
    JWPLC_TFT.fillRect(
        136,
        104,
        48,
        20,
        JWPLC_TFT_GREEN);
    JWPLC_TFT.fillRect(
        190,
        104,
        48,
        20,
        JWPLC_TFT_BLUE);

    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE,
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(99, 129);
    JWPLC_TFT.print("R");
    JWPLC_TFT.setCursor(153, 129);
    JWPLC_TFT.print("G");
    JWPLC_TFT.setCursor(207, 129);
    JWPLC_TFT.print("B");

    JWPLC_TFT.endBatch();
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println("H3E2B2_BOOT=YES");
    Serial.println("H3E2B2_PUBLIC_API=JWPLC_TFT");
    Serial.println("H3E2B2_BACKEND_PUBLIC_LEAK=NO");

    if (!JWPLC_TFT.begin(2000))
    {
        Serial.println("H3E2B2_GATE=FAIL_BEGIN");
        return;
    }

    const JWPLC_TFTPanelInfo info =
        JWPLC_TFT.panelInfo();

    Serial.printf(
        "H3E2B2_PANEL=%s\n",
        info.name);
    Serial.printf(
        "H3E2B2_LOGICAL_WIDTH=%d\n",
        JWPLC_TFT.width());
    Serial.printf(
        "H3E2B2_LOGICAL_HEIGHT=%d\n",
        JWPLC_TFT.height());
    Serial.printf(
        "H3E2B2_ROTATION=%u\n",
        (unsigned)JWPLC_TFT.rotation());

    if (JWPLC_TFT.width() != 320 ||
        JWPLC_TFT.height() != 170 ||
        JWPLC_TFT.rotation() != 1)
    {
        Serial.println("H3E2B2_GATE=FAIL_GEOMETRY");
        return;
    }

    if (!drawQualificationScreen())
    {
        Serial.println("H3E2B2_GATE=FAIL_DRAW");
        return;
    }

    Serial.println("H3E2B2_DISPLAY_DRAW=PASS");
    Serial.println("H3E2B2_GATE=WAIT_PHYSICAL_OBSERVATION");
}

void loop()
{
    static uint32_t lastHeartbeatMs = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - lastHeartbeatMs) >= 1000U)
    {
        lastHeartbeatMs = now;
        Serial.println("H3E2B2_HEARTBEAT=OK");
    }

    delay(10);
}
