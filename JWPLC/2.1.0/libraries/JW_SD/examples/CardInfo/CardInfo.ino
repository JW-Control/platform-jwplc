#include <JW_SD.h>

#ifndef SD_CS
#define SD_CS 32
#endif

JW_SD sd(SD_CS);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("JW_SD CardInfo");

    if (!sd.begin())
    {
        Serial.print("SD begin failed: ");
        Serial.println(sd.lastErrorString());
        return;
    }

    Serial.println("SD ready");

#if defined(ESP32)
    Serial.print("Card type: ");
    Serial.println(sd.cardType());

    Serial.print("Card size MB: ");
    Serial.println((uint32_t)(sd.cardSize() / (1024ULL * 1024ULL)));
#else
    Serial.println("Card type/size helpers are available on ESP32.");
#endif
}

void loop()
{
}
