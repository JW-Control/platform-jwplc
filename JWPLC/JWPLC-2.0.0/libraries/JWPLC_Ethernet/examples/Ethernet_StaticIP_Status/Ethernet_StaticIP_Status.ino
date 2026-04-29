/*
  Ethernet_StaticIP_Status

  Prueba inicial de JWPLC_Ethernet usando IP estática.

  Ajusta la IP a tu red antes de subir.
*/

#include <JWPLC_Ethernet.h>

unsigned long lastPrintMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC_Ethernet static IP status test");

    bool ok = JWPLC_Ethernet.begin(
        IPAddress(192, 168, 1, 50),
        IPAddress(8, 8, 8, 8),
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 255, 0));

    Serial.println(ok ? "Ethernet static begin OK" : "Ethernet static begin failed");

    JWPLC_Ethernet.printStatus(Serial);
}

void loop()
{
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
