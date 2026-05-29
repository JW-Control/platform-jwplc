/*
  Ethernet_Auto_DHCP_Status

  Prueba de Ethernet automático por DHCP para JWPLC Basic.

  Importante:
  - No incluye librerías manualmente.
  - No llama JWPLC_Ethernet.begin().
  - No llama JWPLC_Ethernet.maintain().
  - El runtime JWPLC se encarga de iniciar y mantener Ethernet.

  Validar:
  - JWPLC Basic con RJ45 conectado: Status OK + IP DHCP.
  - JWPLC Basic sin RJ45: Status Link OFF + IP 0.0.0.0.
  - Conectar RJ45 después del arranque: debe pasar a OK.
  - JWPLC Basic Core: Ethernet disabled.
*/

unsigned long lastPrintMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("Ethernet_Auto_DHCP_Status");
    Serial.println("Runtime auto Ethernet test. No begin() called.");
}

void loop()
{
    unsigned long now = millis();

    if (now - lastPrintMs >= 1000)
    {
        lastPrintMs = now;

        Serial.print("Enabled: ");
        Serial.print(JWPLC_Ethernet.isEnabled() ? "yes" : "no");

        Serial.print(" | Attempted: ");
        Serial.print(JWPLC_Ethernet.isBeginAttempted() ? "yes" : "no");

        Serial.print(" | Ready: ");
        Serial.print(JWPLC_Ethernet.isReady() ? "yes" : "no");

        Serial.print(" | HW: ");
        Serial.print(JWPLC_Ethernet.hardwarePresent() ? "present" : "not found");

        Serial.print(" | Link: ");
        Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

        Serial.print(" | Status: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
}
