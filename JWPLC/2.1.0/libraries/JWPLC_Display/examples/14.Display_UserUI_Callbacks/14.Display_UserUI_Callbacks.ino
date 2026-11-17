/*
  14.Display_UserUI_Callbacks

  Pantalla USER dibujada con los callbacks del Display y el objeto
  JWPLC_TFT (dibujo directo, colores JWPLC_TFT_*).

  Muestra:
  - JWPLC_TFT.fillRect() / .setCursor() / .print()
  - jwplcUIEnter()
  - jwplcUIUpdate()
  - jwplcUIExit()
  - Entrada USER con OK y retorno automático USER -> IDLE
*/

bool displayConfigured = false;
uint32_t refreshCounter = 0;
unsigned long lastSerialMs = 0;

static void drawDynamicValues(unsigned long now)
{
    // Redibujar solo la zona que cambia.
    JWPLC_TFT.fillRect(10, 100, 250, 45, JWPLC_TFT_BLACK);

    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);
    JWPLC_TFT.setTextSize(1);

    JWPLC_TFT.setCursor(10, 100);
    JWPLC_TFT.print("Refresh: ");
    JWPLC_TFT.print(refreshCounter);

    JWPLC_TFT.setCursor(10, 116);
    JWPLC_TFT.print("Millis: ");
    JWPLC_TFT.print(now);

    JWPLC_TFT.setCursor(10, 132);
    JWPLC_TFT.print("Retorna a IDLE en 8s");
}

extern "C" void jwplcUIEnter()
{
    refreshCounter = 0;

    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    JWPLC_TFT.setTextColor(JWPLC_TFT_CYAN);
    JWPLC_TFT.setTextSize(2);
    JWPLC_TFT.setCursor(10, 20);
    JWPLC_TFT.print("USER DOT API");

    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);
    JWPLC_TFT.setTextSize(1);
    JWPLC_TFT.setCursor(10, 55);
    JWPLC_TFT.print("Dibujando con:");

    JWPLC_TFT.setCursor(10, 70);
    JWPLC_TFT.print("JWPLC_TFT");

    // Mostrar inmediatamente la región dinámica; no esperar al primer tick.
    drawDynamicValues(millis());
}

extern "C" void jwplcUIUpdate()
{
    refreshCounter++;
    drawDynamicValues(millis());
}

extern "C" void jwplcUIExit()
{
    Serial.println("Saliendo de USER hacia IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(1200);

    // Por defecto se usa IDLE_WAKE_DISABLED. Este ejemplo solicita
    // explícitamente que OK abra la pantalla USER.
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(8000);

    // La cadencia se expresa una sola vez en el scheduler del package.
    JWPLC_Display.setUserRefreshPeriodMs(250);
    JWPLC_Display.clearPendingInput();

    Serial.println();
    Serial.println("JWPLC_Display dot API - USER test");
    Serial.println("OK=USER | retorno automatico en 8s");
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;
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
