/*
  Ethernet_Display_Status

  Prueba de integración Ethernet automático + Display.

  Importante:
  - No incluye librerías manualmente.
  - No llama JWPLC_Ethernet.begin().
  - No llama JWPLC_Ethernet.maintain().
  - El runtime JWPLC se encarga de Ethernet.
  - El sketch solo consulta estado y actualiza indicadores IDLE.
*/

bool displayConfigured = false;

unsigned long lastPrintMs = 0;
unsigned long lastDisplayMs = 0;

const unsigned long PRINT_PERIOD_MS = 1000;
const unsigned long DISPLAY_PERIOD_MS = 500;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("Ethernet_Display_Status");
    Serial.println("Runtime auto Ethernet + Display test. No begin() called.");
}

void loop()
{
    unsigned long now = millis();

    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);

        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setErrLed(false);

        Serial.println("Display ready");
    }

    if (now - lastDisplayMs >= DISPLAY_PERIOD_MS)
    {
        lastDisplayMs = now;

        bool ethOk = JWPLC_Ethernet.isReady() && JWPLC_Ethernet.linkUp();

        if (displayConfigured)
        {
            JWPLC_Display.setRunLed(true);

            bool ethernetDisabled = !JWPLC_Ethernet.isEnabled();
            JWPLC_Display.setErrLed(!ethernetDisabled && !ethOk);
        }
    }

    if (now - lastPrintMs >= PRINT_PERIOD_MS)
    {
        lastPrintMs = now;

        Serial.print("ETH: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | Enabled: ");
        Serial.print(JWPLC_Ethernet.isEnabled() ? "yes" : "no");

        Serial.print(" | Attempted: ");
        Serial.print(JWPLC_Ethernet.isBeginAttempted() ? "yes" : "no");

        Serial.print(" | Ready: ");
        Serial.print(JWPLC_Ethernet.isReady() ? "yes" : "no");

        Serial.print(" | Link: ");
        Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

        Serial.print(" | IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
}
