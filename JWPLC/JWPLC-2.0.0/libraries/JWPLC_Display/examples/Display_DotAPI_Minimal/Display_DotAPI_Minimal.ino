/*
  Display_DotAPI_Minimal

  Prueba mínima de la API con punto de JWPLC_Display.

  Este ejemplo no incluye JWPLC_Display.h porque solo usa la API liviana
  expuesta automáticamente por el runtime del package JWPLC.

  Valida:
  - JWPLC_Display.isReady()
  - JWPLC_Display.setIdleReturnMode(...)
  - JWPLC_Display.setIdleTimeoutMs(...)
  - JWPLC_Display.setUserRefreshPeriodMs(...)
  - Indicadores RUN, ERR, BUS y ETH
  - Cambio IDLE/USER mediante botonera
*/

bool displayConfigured = false;

unsigned long lastBlinkMs = 0;
bool ledState = false;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC_Display dot API - minimal test");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(100);

        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setErrLed(false);
        JWPLC_Display.setEthLed(false);

        Serial.println("Display configurado usando API con punto");
    }

    if (!displayConfigured)
    {
        return;
    }

    unsigned long now = millis();

    if (now - lastBlinkMs >= 1000)
    {
        lastBlinkMs = now;
        ledState = !ledState;

        JWPLC_Display.setBusLed(ledState);
        JWPLC_Display.setEthLed(!ledState);

        Serial.print("IDLE mode: ");
        Serial.println(JWPLC_Display.isIdleMode() ? "yes" : "no");
    }
}
