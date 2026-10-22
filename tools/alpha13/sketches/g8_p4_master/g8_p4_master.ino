/*
 * Alpha13 G8-P4 — MASTER COM4, 12 lecturas FC03, 115200/8N1.
 * No opera Q ni desactiva peripherals autoload.
 * Inicia solo con G8_P4_START:<token> recibido por USB CH340.
 */
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>
#include <string.h>

static constexpr uint8_t kTargetId = 2;
static constexpr uint8_t kCycles = 12;
static constexpr uint32_t kBaud = 115200;
static uint16_t values[4] = {};
static char usbLine[96];
static size_t usbLen = 0;
static char token[25] = {};
static bool ready = false;
static bool running = false;
static bool finished = false;
static uint8_t successes = 0;
static uint8_t failures = 0;
static uint32_t nextRequest = 0;
static uint32_t nextReady = 0;

static void publishResult()
{
    finished = true;
    running = false;
    const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();
    Serial.print("G8_P4_MASTER_RESULT:");
    Serial.print(token);
    Serial.print(" OK=");
    Serial.print(successes);
    Serial.print(" FAIL=");
    Serial.print(failures);
    Serial.print(" RXBYTES=");
    Serial.print((unsigned long)s.rxBytes);
    Serial.print(" RXFRAMES=");
    Serial.print(s.rxFrames);
    Serial.print(" TXFRAMES=");
    Serial.print(s.txFrames);
    Serial.print(" TIMEOUTS=");
    Serial.print(s.masterTimeouts);
    Serial.print(" CRC=");
    Serial.print(s.crcErrors);
    Serial.print(" READY=");
    Serial.println(JWPLC_RS485.isReady() ? 1 : 0);
}

static void onLine(const char *line)
{
    const char *prefix = "G8_P4_START:";
    const size_t n = strlen(prefix);
    if (!ready || running || finished ||
        strncmp(line, prefix, n) != 0 ||
        strlen(line + n) != 24)
        return;
    strcpy(token, line + n);
    successes = 0;
    failures = 0;
    JWPLC_ModbusRTU.resetStats();
    running = true;
    nextRequest = millis() + 200;
    Serial.print("G8_P4_MASTER_STARTED:");
    Serial.println(token);
}

static void serviceUsb()
{
    while (Serial.available() > 0)
    {
        const int c = Serial.read();
        if (c < 0) break;
        if (c == '\n' || c == '\r')
        {
            if (usbLen)
            {
                usbLine[usbLen] = 0;
                onLine(usbLine);
                usbLen = 0;
            }
        }
        else if (usbLen + 1 < sizeof(usbLine))
            usbLine[usbLen++] = (char)c;
        else
            usbLen = 0;
    }
}

void setup()
{
    Serial.begin(115200);
    if (!JWPLC_ModbusRTU.begin(247, kBaud, SERIAL_8N1))
    {
        Serial.print("G8_P4_FAIL:MASTER_BEGIN:");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }
    JWPLC_ModbusRTU.resetStats();
    ready = true;
}

void loop()
{
    serviceUsb();
    if (!ready) return;
    JWPLC_ModbusRTU.task();
    const uint32_t now = millis();
    if (!running)
    {
        if (!finished && (int32_t)(now - nextReady) >= 0)
        {
            nextReady = now + 500;
            Serial.println("G8_P4_READY:MASTER:115200:8N1");
        }
        return;
    }
    if (JWPLC_ModbusRTU.masterDone())
    {
        const bool ok = JWPLC_ModbusRTU.masterSucceeded();
        if (ok && values[0] == 0x1234 && values[1] == 0x2345 &&
            values[2] == 0x3456 && values[3] == 0x4567)
            ++successes;
        else
            ++failures;
        JWPLC_ModbusRTU.clearMasterResult();
        if (successes + failures >= kCycles)
        {
            publishResult();
            return;
        }
        nextRequest = now + 80;
    }
    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(now - nextRequest) >= 0)
    {
        if (!JWPLC_ModbusRTU.requestReadHoldingRegisters(
                kTargetId, 0, 4, values, 1200))
        {
            ++failures;
            if (successes + failures >= kCycles)
            {
                publishResult();
                return;
            }
            nextRequest = now + 100;
        }
        else
            nextRequest = now + 1500;
    }
}
