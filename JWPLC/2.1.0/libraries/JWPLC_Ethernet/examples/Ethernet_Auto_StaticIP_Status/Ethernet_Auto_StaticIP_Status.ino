/*
  Ethernet_Auto_StaticIP_Status

  Prueba de Ethernet automático con IP estática para JWPLC Basic.

  Importante:
  - No incluye librerías manualmente.
  - No llama JWPLC_Ethernet.begin().
  - No llama JWPLC_Ethernet.maintain().
  - Solo configura la IP en setup().
  - El runtime JWPLC inicia Ethernet después de setup().

  Ajusta la IP según tu red antes de probar.
*/

unsigned long lastPrintMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("Ethernet_Auto_StaticIP_Status");
    Serial.println("Configuring static IP. No begin() called.");

    // Ajusta estos valores a tu red.
    JWPLC_Ethernet.setStaticIP(
        IPAddress(192, 168, 1, 50),
        IPAddress(8, 8, 8, 8),
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 255, 0));
}

void loop()
{
    unsigned long now = millis();

    if (now - lastPrintMs >= 1000)
    {
        lastPrintMs = now;

        Serial.print("Mode: STATIC");

        Serial.print(" | Enabled: ");
        Serial.print(JWPLC_Ethernet.isEnabled() ? "yes" : "no");

        Serial.print(" | Attempted: ");
        Serial.print(JWPLC_Ethernet.isBeginAttempted() ? "yes" : "no");

        Serial.print(" | Ready: ");
        Serial.print(JWPLC_Ethernet.isReady() ? "yes" : "no");

        Serial.print(" | Link: ");
        Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

        Serial.print(" | Status: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | IP: ");
        Serial.print(JWPLC_Ethernet.localIP());

        Serial.print(" | Gateway: ");
        Serial.print(JWPLC_Ethernet.gatewayIP());

        Serial.print(" | DNS: ");
        Serial.println(JWPLC_Ethernet.dnsServerIP());
    }
}
