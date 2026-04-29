/*
  Ethernet_Display_Status

  Prueba de integración básica Ethernet + Display.

  La pantalla IDLE usa el indicador ETH para reflejar link/estado Ethernet.
*/

#include <JWPLC_Ethernet.h>
#include <JWPLC_Display.h>

bool ethernetStarted = false;
bool displayConfigured = false;

unsigned long lastCheckMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC_Ethernet + Display status test");

    ethernetStarted = JWPLC_Ethernet.begin();
    JWPLC_Ethernet.printStatus(Serial);
}

void loop()
{
    JWPLC_Ethernet.maintain();

    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;
        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);

        Serial.println("Display ready");
    }

    unsigned long now = millis();

    if (now - lastCheckMs >= 1000)
    {
        lastCheckMs = now;

        bool ethOk = ethernetStarted && JWPLC_Ethernet.linkUp();

        if (displayConfigured)
        {
            JWPLC_Display.setEthLed(ethOk);
            JWPLC_Display.setErrLed(!ethOk);
        }

        Serial.print("ETH: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
}
