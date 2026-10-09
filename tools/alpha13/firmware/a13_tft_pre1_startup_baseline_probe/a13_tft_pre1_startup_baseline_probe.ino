#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "soc/gpio_reg.h"

static uint32_t g_setupEntryMs = 0;

static bool outputEnabled(uint8_t pin)
{
    if (pin < 32)
    {
        return (REG_READ(GPIO_ENABLE_REG) & (uint32_t)(1UL << pin)) != 0;
    }

    return (REG_READ(GPIO_ENABLE1_REG) &
            (uint32_t)(1UL << (pin - 32))) != 0;
}

static bool outputLatchHigh(uint8_t pin)
{
    if (pin < 32)
    {
        return (REG_READ(GPIO_OUT_REG) & (uint32_t)(1UL << pin)) != 0;
    }

    return (REG_READ(GPIO_OUT1_REG) &
            (uint32_t)(1UL << (pin - 32))) != 0;
}

static void printResult()
{
    const bool rstOutputEnable = outputEnabled(JWPLC_TFT_RST);
    const bool rstOutputLatch = outputLatchHigh(JWPLC_TFT_RST);
    const bool csOutputEnable = outputEnabled(JWPLC_TFT_CS);
    const bool csOutputLatch = outputLatchHigh(JWPLC_TFT_CS);

    Serial.println("A13_TFT_PRE1_RESULT=BEGIN");
    Serial.print("SETUP_ENTRY_MS=");
    Serial.println((unsigned long)g_setupEntryMs);
    Serial.print("DISPLAY_READY=");
    Serial.println(JWPLC_Display.isReady() ? "YES" : "NO");
    Serial.print("IO_READY=");
    Serial.println(JWPLC_IO.ready() ? "YES" : "NO");
    Serial.print("TFT_RST_OUTPUT_ENABLE=");
    Serial.println(rstOutputEnable ? "YES" : "NO");
    Serial.print("TFT_RST_OUTPUT_LATCH=");
    Serial.println(rstOutputLatch ? "HIGH" : "LOW");
    Serial.print("TFT_CS_OUTPUT_ENABLE=");
    Serial.println(csOutputEnable ? "YES" : "NO");
    Serial.print("TFT_CS_OUTPUT_LATCH=");
    Serial.println(csOutputLatch ? "HIGH" : "LOW");
    Serial.print("UPTIME_MS=");
    Serial.println((unsigned long)millis());
    Serial.println("A13_TFT_PRE1_RESULT=END");
}

void setup()
{
    g_setupEntryMs = millis();
    Serial.begin(115200);
    delay(100);
    printResult();
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - lastPrintMs) >= 500u)
    {
        lastPrintMs = now;
        printResult();
    }

    delay(10);
}
