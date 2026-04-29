/*
  Display_UserUI_Callbacks

  Ejemplo de pantalla USER usando callbacks y JWPLC_Display.tft().

  Este ejemplo sí incluye JWPLC_Display.h porque usa dibujo directo con
  Adafruit_ST7789 y constantes ST77XX_*.

  Valida:
  - JWPLC_Display.tft()
  - jwplcUserDisplayEnterCallback()
  - jwplcUserDisplayRefreshCallback(...)
  - jwplcUserDisplayExitCallback()
  - Retorno automático USER -> IDLE
*/

#include <JWPLC_Display.h>

bool displayConfigured = false;
uint32_t refreshCounter = 0;
unsigned long lastSerialMs = 0;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(2);
    tft.setCursor(10, 20);
    tft.print("USER DOT API");

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 55);
    tft.print("Dibujando con:");

    tft.setCursor(10, 70);
    tft.print("JWPLC_Display.tft()");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    static unsigned long lastDrawMs = 0;
    unsigned long now = millis();

    if (now - lastDrawMs < 250)
    {
        return;
    }

    lastDrawMs = now;
    refreshCounter++;

    auto &tft = JWPLC_Display.tft();

    // Redibujar solo la zona que cambia.
    tft.fillRect(10, 100, 250, 45, ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);

    tft.setCursor(10, 100);
    tft.print("Refresh: ");
    tft.print(refreshCounter);

    tft.setCursor(10, 116);
    tft.print("Millis: ");
    tft.print(now);

    tft.setCursor(10, 132);
    tft.print("Retorna a IDLE en 8s");
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC_Display dot API - USER test");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(100);

        Serial.println("Display configurado usando API con punto");
    }

    if (!displayConfigured)
    {
        return;
    }

    unsigned long now = millis();

    if (now - lastSerialMs >= 1000)
    {
        lastSerialMs = now;

        Serial.print("IDLE mode: ");
        Serial.println(JWPLC_Display.isIdleMode() ? "yes" : "no");
    }
}
