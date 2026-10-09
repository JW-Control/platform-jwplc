#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

#include "soc/gpio_reg.h"

static void printResult()
{
    const uint32_t enIoMask = (uint32_t)(1UL << EN_IO);
    const bool enIoOutputEnabled =
        (REG_READ(GPIO_ENABLE_REG) & enIoMask) != 0;
    const bool enIoOutputLatch =
        (REG_READ(GPIO_OUT_REG) & enIoMask) != 0;

    Serial.println("A13_G2_P4_RESULT=BEGIN");
    Serial.print("IO_READY=");
    Serial.println(JWPLC_IO.ready() ? "YES" : "NO");
    Serial.print("EN_IO_OUTPUT_ENABLE=");
    Serial.println(enIoOutputEnabled ? "YES" : "NO");
    Serial.print("EN_IO_OUTPUT_LATCH=");
    Serial.println(enIoOutputLatch ? "HIGH" : "LOW");
    Serial.print("INPUTS=");
    Serial.println((unsigned int)JWPLC_IO.inputs());
    Serial.print("OUTPUTS=");
    Serial.println((unsigned int)JWPLC_IO.outputs());
    Serial.print("LAST_SCAN_MS=");
    Serial.println((unsigned long)JWPLC_IO.lastScanMs());
    Serial.print("UPTIME_MS=");
    Serial.println((unsigned long)millis());
    Serial.println("A13_G2_P4_RESULT=END");
}

void setup()
{
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
