/*
  Display_Idle_Return_Modes

  Ejemplo para probar los tres modos de retorno USER -> IDLE:

  1) IDLE_RETURN_TIMEOUT
     Retorna automáticamente a IDLE luego de un tiempo sin actividad.

  2) IDLE_RETURN_ESC_ONLY
     Solo retorna a IDLE con ESC o acción equivalente del runtime.

  3) IDLE_RETURN_DISABLED
     No retorna automáticamente. El sketch debe llamar goIdle().

  Uso:
  - Cambia CURRENT_MODE para probar cada caso.
  - Presiona una tecla para entrar a USER.
*/

#include <JWPLC_Display.h>

// Cambia esta línea para probar los tres modos:
// IDLE_RETURN_TIMEOUT
// IDLE_RETURN_ESC_ONLY
// IDLE_RETURN_DISABLED
const JWPLC_DisplayIdleReturnMode CURRENT_MODE = IDLE_RETURN_TIMEOUT;

bool displayConfigured = false;

uint32_t userCounter = 0;
unsigned long lastManualReturnMs = 0;

const unsigned long TIMEOUT_MS = 8000;
const unsigned long MANUAL_RETURN_MS = 12000;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("Display_Idle_Return_Modes");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(CURRENT_MODE);
        JWPLC_Display.setIdleTimeoutMs(TIMEOUT_MS);
        JWPLC_Display.setUserRefreshPeriodMs(250);

        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setErrLed(false);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setEthLed(false);

        Serial.print("Display ready. Mode: ");

        if (CURRENT_MODE == IDLE_RETURN_TIMEOUT)
        {
            Serial.println("IDLE_RETURN_TIMEOUT");
        }
        else if (CURRENT_MODE == IDLE_RETURN_ESC_ONLY)
        {
            Serial.println("IDLE_RETURN_ESC_ONLY");
        }
        else if (CURRENT_MODE == IDLE_RETURN_DISABLED)
        {
            Serial.println("IDLE_RETURN_DISABLED");
        }
    }

    // Ejemplo de retorno manual solo para IDLE_RETURN_DISABLED.
    // Esto evita que el usuario piense que el equipo quedó atrapado en USER.
    if (displayConfigured &&
        CURRENT_MODE == IDLE_RETURN_DISABLED &&
        !JWPLC_Display.isIdleMode())
    {
        unsigned long now = millis();

        if (lastManualReturnMs == 0)
        {
            lastManualReturnMs = now;
        }

        if (now - lastManualReturnMs >= MANUAL_RETURN_MS)
        {
            Serial.println("Retorno manual a IDLE usando goIdle()");
            JWPLC_Display.goIdle();
            lastManualReturnMs = 0;
        }
    }
    else
    {
        lastManualReturnMs = 0;
    }
}

extern "C" void jwplcUserDisplayEnterCallback()
{
    userCounter = 0;

    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(10, 12);
    tft.print("USER MODE");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    tft.setCursor(10, 50);
    tft.print("Modo:");

    tft.setCursor(10, 80);
    tft.print("Counter:");

    tft.setCursor(10, 120);

    if (CURRENT_MODE == IDLE_RETURN_TIMEOUT)
    {
        tft.print("Vuelve por timeout.");
    }
    else if (CURRENT_MODE == IDLE_RETURN_ESC_ONLY)
    {
        tft.print("Vuelve solo con ESC.");
    }
    else if (CURRENT_MODE == IDLE_RETURN_DISABLED)
    {
        tft.print("Vuelve con goIdle().");
    }
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    userCounter++;

    auto &tft = JWPLC_Display.tft();

    tft.setTextSize(1);

    tft.fillRect(80, 50, 220, 16, ST77XX_BLACK);
    tft.setCursor(80, 50);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);

    if (CURRENT_MODE == IDLE_RETURN_TIMEOUT)
    {
        tft.print("IDLE_RETURN_TIMEOUT");
    }
    else if (CURRENT_MODE == IDLE_RETURN_ESC_ONLY)
    {
        tft.print("IDLE_RETURN_ESC_ONLY");
    }
    else if (CURRENT_MODE == IDLE_RETURN_DISABLED)
    {
        tft.print("IDLE_RETURN_DISABLED");
    }

    tft.fillRect(80, 80, 120, 16, ST77XX_BLACK);
    tft.setCursor(80, 80);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(userCounter);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
}
