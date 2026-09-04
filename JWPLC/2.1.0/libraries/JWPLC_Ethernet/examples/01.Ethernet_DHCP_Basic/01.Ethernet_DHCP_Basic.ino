/*
  01.Ethernet_DHCP_Basic

  Ethernet W5500 del JWPLC Basic usando DHCP.

  El autoload del package llama JWPLC_Ethernet.service() de forma cooperativa.
  Por eso este sketch NO necesita llamar begin() ni service() en loop().

  Funciones mostradas:
  - useDHCP(): selecciona DHCP antes de que arranque el servicio automático.
  - isReady(): indica si existe una configuración de red utilizable.
  - statusString(): estado legible.
  - diagnosticCode(): código corto usado también por el indicador ETH.
  - localIP(): IP obtenida.
*/

#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    // setup() termina antes de que el task de sistema empiece el autoload ETH,
    // por lo que esta selección queda aplicada al primer intento de red.
    JWPLC_Ethernet.useDHCP();

    Serial.println("JWPLC Basic - Ethernet DHCP");
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 1000)
        return;

    lastPrintMs = millis();

    Serial.print("ready=");
    Serial.print(JWPLC_Ethernet.isReady());
    Serial.print(" busy=");
    Serial.print(JWPLC_Ethernet.isBusy());
    Serial.print(" diag=");
    Serial.print(JWPLC_Ethernet.diagnosticCode());
    Serial.print(" status=");
    Serial.print(JWPLC_Ethernet.statusString());

    if (JWPLC_Ethernet.isReady())
    {
        Serial.print(" ip=");
        Serial.print(JWPLC_Ethernet.localIP());
    }

    Serial.println();
}
