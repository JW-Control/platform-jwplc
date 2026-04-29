/*
  Ethernet_DHCP_Status

  Prueba inicial de JWPLC_Ethernet usando DHCP.

  Requiere:
  - JWPLC Basic con W5500.
  - Cable Ethernet conectado a una red con DHCP.
*/

#include <JWPLC_Ethernet.h>

unsigned long lastPrintMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC_Ethernet DHCP status test");

    if (JWPLC_Ethernet.begin())
    {
        Serial.println("Ethernet begin OK");
    }
    else
    {
        Serial.println("Ethernet begin failed");
    }

    JWPLC_Ethernet.printStatus(Serial);
}

void loop()
{
    JWPLC_Ethernet.maintain();

    unsigned long now = millis();

    if (now - lastPrintMs >= 2000)
    {
        lastPrintMs = now;

        Serial.print("Status: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | Link: ");
        Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

        Serial.print(" | IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
}
