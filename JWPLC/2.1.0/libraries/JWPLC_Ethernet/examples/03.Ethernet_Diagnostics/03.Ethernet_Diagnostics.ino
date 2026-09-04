/*
  03.Ethernet_Diagnostics

  Diagnóstico compacto del runtime Ethernet del JWPLC Basic.

  Útil para distinguir:
  - W5500 no detectado;
  - cable/link desconectado;
  - DHCP en proceso o fallido;
  - IP válida;
  - contención temporal del bus SPI.

  printStatus() agrupa la información principal del driver.
*/

#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    JWPLC_Ethernet.useDHCP();

    Serial.println("JWPLC Basic - Ethernet diagnostics");
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 3000)
        return;

    lastPrintMs = millis();

    Serial.println();
    Serial.println("--- ETH STATUS ---");

    // Resumen humano: enabled, begin, ready, hardware, link, IP, etc.
    JWPLC_Ethernet.printStatus(Serial);

    // Estado de la máquina cooperativa y código corto de diagnóstico.
    Serial.print("Runtime state: ");
    Serial.println((int)JWPLC_Ethernet.runtimeState());
    Serial.print("Diagnostic code: ");
    Serial.println(JWPLC_Ethernet.diagnosticCode());
    Serial.print("Last error: ");
    Serial.println(JWPLC_Ethernet.lastErrorString());
}
