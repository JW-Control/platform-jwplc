#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

extern "C"
{
#include "jwplc_i2c_bridge.h"
#include "jwplc_peripherals.h"
#include "peripheral-tca6424a.h"
}

extern "C" uint8_t a13G2FaultStep(void);
extern "C" uint8_t a13G2OpOkMask(void);
extern "C" bool a13G2EnIoHighRequested(void);
extern "C" bool a13G2PeripheralsInitialized(void);

static void printReg(const char *key, uint8_t reg)
{
    uint8_t value = 0;
    const int ret =
        jwplcI2C_readReg8(
            TCA6424A_DEFAULT_ADDRESS,
            reg,
            &value);

    Serial.print(key);
    Serial.print("_OK=");
    Serial.println(ret == 0 ? "YES" : "NO");

    Serial.print(key);
    Serial.print("=");
    if (ret == 0)
    {
        if (value < 0x10)
        {
            Serial.print('0');
        }
        Serial.println(value, HEX);
    }
    else
    {
        Serial.println("NA");
    }
}

static void printResult()
{
    const JWPLC_IOState *io = jwplcGetIOState();

    Serial.println("A13_G2_TCA_RESULT=BEGIN");
    Serial.print("FAULT_STEP=");
    Serial.println(a13G2FaultStep());
    Serial.print("OP_OK_MASK=");
    Serial.println(a13G2OpOkMask());
    Serial.print("EN_IO_HIGH_REQUESTED=");
    Serial.println(a13G2EnIoHighRequested() ? "YES" : "NO");
    Serial.print("EN_IO_ACTUAL=");
    Serial.println(digitalRead(EN_IO) == HIGH ? "HIGH" : "LOW");
    Serial.print("PERIPHERALS_INITIALIZED=");
    Serial.println(a13G2PeripheralsInitialized() ? "YES" : "NO");
    Serial.print("IO_STATE_INITIALIZED=");
    Serial.println(
        (io != nullptr && io->initialized) ? "YES" : "NO");
    Serial.print("IO_VIEW_READY=");
    Serial.println(JWPLC_IO.ready() ? "YES" : "NO");

    printReg("REG_OUTPUT0", TCA6424A_RA_OUTPUT0);
    printReg("REG_OUTPUT1", TCA6424A_RA_OUTPUT1);
    printReg("REG_OUTPUT2", TCA6424A_RA_OUTPUT2);
    printReg("REG_CONFIG0", TCA6424A_RA_CONFIG0);
    printReg("REG_CONFIG1", TCA6424A_RA_CONFIG1);
    printReg("REG_CONFIG2", TCA6424A_RA_CONFIG2);

    Serial.println("A13_G2_TCA_RESULT=END");
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
