/*
  02.Ethernet_StaticIP_Basic

  Configuración IPv4 estática para el W5500 del JWPLC Basic.

  IMPORTANTE: cambia IP, gateway y DNS según la red del taller.
  El autoload sigue encargándose del W5500; setStaticIP() sólo define la
  configuración que utilizará el servicio automático.
*/

#include <JWPLC_Ethernet.h>

static const IPAddress LOCAL_IP(192, 168, 1, 50);
static const IPAddress DNS_IP(192, 168, 1, 1);
static const IPAddress GATEWAY_IP(192, 168, 1, 1);
static const IPAddress SUBNET_MASK(255, 255, 255, 0);

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Debe configurarse en setup(), antes de que el task de sistema inicie ETH.
    JWPLC_Ethernet.setStaticIP(
        LOCAL_IP,
        DNS_IP,
        GATEWAY_IP,
        SUBNET_MASK);

    Serial.println("JWPLC Basic - Ethernet Static IP");
    Serial.print("IP solicitada: ");
    Serial.println(LOCAL_IP);
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 1000)
        return;

    lastPrintMs = millis();

    Serial.print("diag=");
    Serial.print(JWPLC_Ethernet.diagnosticCode());
    Serial.print(" status=");
    Serial.print(JWPLC_Ethernet.statusString());
    Serial.print(" ready=");
    Serial.print(JWPLC_Ethernet.isReady());

    if (JWPLC_Ethernet.isReady())
    {
        Serial.print(" ip=");
        Serial.print(JWPLC_Ethernet.localIP());
        Serial.print(" gw=");
        Serial.print(JWPLC_Ethernet.gatewayIP());
    }

    Serial.println();
}
