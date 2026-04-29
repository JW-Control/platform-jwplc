/*
  Display_Efficient_Redraw

  Ejemplo de buenas prácticas para reducir consumo de recursos al dibujar
  en la TFT ST7789 del JWPLC Basic.

  Idea principal:
  - No usar fillScreen() en cada refresco.
  - Redibujar solo las zonas que cambian.
  - Guardar últimos valores dibujados.
  - Usar snprintf() en lugar de concatenaciones String frecuentes.
*/

#include <JWPLC_Display.h>

bool displayConfigured = false;

static uint32_t g_processCounter = 0;
static bool g_runState = false;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    // fillScreen() está bien al entrar a una pantalla nueva.
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(10, 15);
    tft.print("Proceso");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);

    tft.setCursor(10, 55);
    tft.print("Estado:");

    tft.setCursor(10, 80);
    tft.print("Contador:");

    tft.setCursor(10, 120);
    tft.print("Redibujo parcial");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    static bool lastRunState = false;
    static uint32_t lastCounter = 0xFFFFFFFF;
    static bool firstDraw = true;

    auto &tft = JWPLC_Display.tft();

    if (firstDraw || g_runState != lastRunState)
    {
        lastRunState = g_runState;

        // Solo limpiar el campo del estado, no toda la pantalla.
        tft.fillRect(90, 55, 120, 14, ST77XX_BLACK);
        tft.setCursor(90, 55);
        tft.setTextColor(g_runState ? ST77XX_GREEN : ST77XX_RED);
        tft.print(g_runState ? "RUN" : "STOP");
    }

    if (firstDraw || g_processCounter != lastCounter)
    {
        lastCounter = g_processCounter;

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)g_processCounter);

        // Solo limpiar el campo del contador.
        tft.fillRect(90, 80, 120, 14, ST77XX_BLACK);
        tft.setCursor(90, 80);
        tft.setTextColor(ST77XX_WHITE);
        tft.print(buffer);
    }

    firstDraw = false;
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
    Serial.println("JWPLC_Display efficient redraw test");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(200);

        Serial.println("Display configurado");
    }

    if (!displayConfigured)
    {
        return;
    }

    static unsigned long lastProcessMs = 0;
    unsigned long now = millis();

    if (now - lastProcessMs >= 1000)
    {
        lastProcessMs = now;

        g_processCounter++;
        g_runState = !g_runState;

        JWPLC_Display.setRunLed(g_runState);
        JWPLC_Display.setErrLed(false);

        Serial.print("Counter: ");
        Serial.println(g_processCounter);
    }
}
