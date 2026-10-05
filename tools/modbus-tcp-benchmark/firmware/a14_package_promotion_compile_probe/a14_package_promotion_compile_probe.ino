#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_TFT.h>
#include <JWPLC_Ethernet.h>

static EthernetUDP g_udp;
static uint8_t g_udpBuffer[1016];
static volatile bool g_compileProbe = false;

void setup()
{
    if (g_compileProbe)
    {
        const int got =
            g_udp.jwplcReadPacketFastDeferred(
                g_udpBuffer,
                sizeof(g_udpBuffer));

        const bool committed =
            g_udp.jwplcCommitRxFast();

        const bool tftReady =
            JWPLC_TFT.isReady();

        Serial.print(got);
        Serial.print(committed);
        Serial.print(tftReady);
    }
}

void loop()
{
}
