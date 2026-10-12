/*
 * Alpha13 G8-P4 — SLAVE ID 2 / 115200 / 8N1.
 * Solo ensayo aislado. NO acciona Q ni configura periféricos ajenos.
 * La consola Serial es USB CH340, Modbus opera por UART RS-485.
 * Autoload del package JWPLC intacto.
 */
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>

static constexpr uint8_t kId = 2;
static constexpr uint32_t kBaud = 115200;
static uint16_t holding[4] = {0x1234, 0x2345, 0x3456, 0x4567};
static char usbLine[96];
static size_t usbLen = 0;
static bool started = false;
static bool reported = false;
static uint32_t nextReady = 0;

static void onLine(const char *line)
{
    const char *prefix = "G8_P4_REPORT:";
    const size_t n = strlen(prefix);
    if (strncmp(line, prefix, n) != 0 || !started || reported)
        return;
    const char *token = line + n;
    if (strlen(token) != 24)
        return;
    reported = true;
    const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();
    Serial.print("G8_P4_SLAVE_RESULT:");
    Serial.print(token);
    Serial.print(" RXBYTES=");
    Serial.print((unsigned long)s.rxBytes);
    Serial.print(" RXFRAMES=");
    Serial.print(s.rxFrames);
    Serial.print(" TXFRAMES=");
    Serial.print(s.txFrames);
    Serial.print(" REQUESTS=");
    Serial.print(s.requestsOk);
    Serial.print(" CRC=");
    Serial.print(s.crcErrors);
    Serial.print(" READY=");
    Serial.println(JWPLC_RS485.isReady() ? 1 : 0);
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
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 4);
    if (!JWPLC_ModbusRTU.begin(kId, kBaud, SERIAL_8N1))
    {
        Serial.print("G8_P4_FAIL:SLAVE_BEGIN:");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
        return;
    }
    JWPLC_ModbusRTU.resetStats();
    started = true;
}

void loop()
{
    serviceUsb();
    if (!started) return;
    JWPLC_ModbusRTU.task();
    const uint32_t now = millis();
    if (!reported && (int32_t)(now - nextReady) >= 0)
    {
        nextReady = now + 500;
        Serial.println("G8_P4_READY:SLAVE:ID2:115200:8N1");
    }
}
