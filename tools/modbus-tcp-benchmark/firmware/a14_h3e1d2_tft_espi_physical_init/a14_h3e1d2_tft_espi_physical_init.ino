/*
  A14 H3E.1D.2 - TFT_eSPI physical init / geometry qualification

  Gate diagnostico. No es firmware de produccion.

  El build define JWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD para que:
  - JWPLC_Display/Adafruit NO se enlace;
  - JWPLC_GlobalPeripherals siga en autoload;
  - TFT_eSPI sea el unico owner grafico de la TFT en este probe.
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <jwplc_spi_bus.h>

#ifndef JWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD
#error "H3E1D2: bypass diagnostico de Display no definido"
#endif

#ifndef JWPLC_H3E1D2_SETUP
#error "H3E1D2: tft_setup.h local no cargado"
#endif

#if TFT_WIDTH != 170 || TFT_HEIGHT != 320
#error "H3E1D2: geometria fisica esperada 170x320"
#endif

#if SPI_FREQUENCY != 80000000
#error "H3E1D2: SPI esperado 80 MHz"
#endif

#if TFT_SPI_MODE != SPI_MODE0
#error "H3E1D2: SPI MODE0 requerido"
#endif

#if TFT_RGB_ORDER != TFT_RGB
#error "H3E1D2: RGB requerido"
#endif

#ifndef TFT_INVERSION_ON
#error "H3E1D2: inversion ON requerida"
#endif

#ifndef SUPPORT_TRANSACTIONS
#error "H3E1D2: SUPPORT_TRANSACTIONS requerido"
#endif

static TFT_eSPI tft = TFT_eSPI();

static void drawQualificationScreen()
{
    tft.fillScreen(TFT_BLACK);

    // Marco completo: detecta clipping/offset en cualquier borde.
    tft.drawRect(0, 0, 320, 170, TFT_WHITE);

    // Esquinas con identidad visual y colores conocidos.
    tft.fillRect(2, 2, 26, 18, TFT_RED);
    tft.fillRect(292, 2, 26, 18, TFT_GREEN);
    tft.fillRect(2, 150, 26, 18, TFT_BLUE);
    tft.fillRect(292, 150, 26, 18, TFT_WHITE);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.setCursor(34, 4);
    tft.print("TL RED");
    tft.setCursor(238, 4);
    tft.print("TR GREEN");

    tft.setCursor(34, 156);
    tft.print("BL BLUE");
    tft.setCursor(238, 156);
    tft.print("BR WHITE");

    tft.setTextSize(2);
    tft.setCursor(52, 42);
    tft.print("JWPLC TFT_eSPI");

    tft.setTextSize(1);
    tft.setCursor(94, 70);
    tft.print("320x170  ROT=3");
    tft.setCursor(84, 84);
    tft.print("80MHz  SPI MODE0");

    // Barras RGB centrales para validar orden de color e inversion.
    tft.fillRect(82, 104, 48, 20, TFT_RED);
    tft.fillRect(136, 104, 48, 20, TFT_GREEN);
    tft.fillRect(190, 104, 48, 20, TFT_BLUE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(99, 129);
    tft.print("R");
    tft.setCursor(153, 129);
    tft.print("G");
    tft.setCursor(207, 129);
    tft.print("B");
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println("H3E1D2_BOOT=YES");
    Serial.printf("H3E1D2_TFT_ESPI_VERSION=%s\n", TFT_ESPI_VERSION);

    const bool spiReadyBefore = jwplcSPI_isReady();
    Serial.printf(
        "H3E1D2_SPI_READY_BEFORE=%s\n",
        spiReadyBefore ? "YES" : "NO");

    if (!spiReadyBefore)
    {
        Serial.println("H3E1D2_GATE=FAIL_SPI_NOT_READY");
        return;
    }

    if (!jwplcSPI_acquire(2000))
    {
        Serial.println("H3E1D2_GATE=FAIL_SPI_MUTEX");
        return;
    }

    jwplcSPI_prepareForTFT();

    tft.init();
    tft.setRotation(3);

    Serial.printf("H3E1D2_LOGICAL_WIDTH=%d\n", tft.width());
    Serial.printf("H3E1D2_LOGICAL_HEIGHT=%d\n", tft.height());

    if (tft.width() != 320 || tft.height() != 170)
    {
        jwplcSPI_release();
        Serial.println("H3E1D2_GATE=FAIL_GEOMETRY");
        return;
    }

    drawQualificationScreen();

    jwplcSPI_release();

    Serial.println("H3E1D2_DISPLAY_DRAW=PASS");
    Serial.println("H3E1D2_GATE=WAIT_PHYSICAL_OBSERVATION");
}

void loop()
{
    static uint32_t lastHeartbeatMs = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - lastHeartbeatMs) >= 1000U)
    {
        lastHeartbeatMs = now;
        Serial.println("H3E1D2_HEARTBEAT=OK");
    }

    delay(10);
}
