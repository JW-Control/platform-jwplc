#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

#include "soc/gpio_reg.h"

extern "C"
{
#include "jwplc_peripherals.h"
}

extern "C" uint8_t a13G2FaultStep(void);
extern "C" uint8_t a13G2AttemptMask(void);
extern "C" uint8_t a13G2OpOkMask(void);
extern "C" bool a13G2EnIoHighRequested(void);
extern "C" bool a13G2PeripheralsInitialized(void);

static void printResult()
{
    const JWPLC_IOState *io = jwplcGetIOState();
    const uint32_t enIoMask = (uint32_t)(1UL << EN_IO);
    const bool enIoOutputEnabled =
        (REG_READ(GPIO_ENABLE_REG) & enIoMask) != 0;
    const bool enIoOutputLatch =
        (REG_READ(GPIO_OUT_REG) & enIoMask) != 0;

    Serial.println("A13_G2_CANDIDATE_RESULT=BEGIN");
    Serial.print("FAULT_STEP=");
    Serial.println(a13G2FaultStep());
    Serial.print("OP_ATTEMPT_MASK=");
    Serial.println(a13G2AttemptMask());
    Serial.print("OP_OK_MASK=");
    Serial.println(a13G2OpOkMask());
    Serial.print("EN_IO_HIGH_REQUESTED=");
    Serial.println(a13G2EnIoHighRequested() ? "YES" : "NO");
    Serial.print("EN_IO_OUTPUT_ENABLE=");
    Serial.println(enIoOutputEnabled ? "YES" : "NO");
    Serial.print("EN_IO_OUTPUT_LATCH=");
    Serial.println(enIoOutputLatch ? "HIGH" : "LOW");
    Serial.print("EN_IO_PAD_READBACK=");
    Serial.println(digitalRead(EN_IO) == HIGH ? "HIGH" : "LOW");
    Serial.print("PERIPHERALS_INITIALIZED=");
    Serial.println(a13G2PeripheralsInitialized() ? "YES" : "NO");
    Serial.print("IO_STATE_INITIALIZED=");
    Serial.println((io != nullptr && io->initialized) ? "YES" : "NO");
    Serial.print("IO_VIEW_READY=");
    Serial.println(JWPLC_IO.ready() ? "YES" : "NO");
    Serial.println("A13_G2_CANDIDATE_RESULT=END");
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
