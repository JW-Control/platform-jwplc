#include <Arduino.h>
#include <Wire.h>
#include <JWPLC_GlobalPeripherals.h>

static const char *kGatePath = "/jwplc_p8_gate.txt";
static const char *kGatePayload = "JWPLC_P8_WIRE_SPI_OK";

static bool runWireGate()
{
    Serial.println("[P8-WIRE] Iniciando Wire...");

    const bool beginOk = Wire.begin();

    Serial.print("[P8-WIRE] Wire.begin() = ");
    Serial.println(beginOk ? "true" : "false");

    Serial.print("[P8-WIRE] Clock = ");
    Serial.println(Wire.getClock());

    Wire.beginTransmission(0x68);

    const size_t written = Wire.write((uint8_t)0x00);

    Serial.print("[P8-WIRE] write(0x00) = ");
    Serial.println(written);

    const uint8_t txResult = Wire.endTransmission(false);

    Serial.print("[P8-WIRE] endTransmission(false) = ");
    Serial.println(txResult);

    const size_t received =
        Wire.requestFrom((uint8_t)0x68, (size_t)1, true);

    Serial.print("[P8-WIRE] requestFrom() = ");
    Serial.println(received);

    int rawSeconds = -1;

    if (Wire.available())
    {
        rawSeconds = Wire.read();
    }

    Serial.print("[P8-WIRE] RTC reg 0x00 = 0x");

    if (rawSeconds >= 0 && rawSeconds < 16)
    {
        Serial.print("0");
    }

    if (rawSeconds >= 0)
    {
        Serial.println(rawSeconds, HEX);
    }
    else
    {
        Serial.println("INVALID");
    }

    JWRTCDateTime dt = {};
    const bool rtcOk = JWPLC_RTC.read(dt);

    Serial.print("[P8-WIRE] JWPLC_RTC.read() = ");
    Serial.println(rtcOk ? "true" : "false");

    const bool ok =
        beginOk &&
        written == 1 &&
        txResult == 0 &&
        received == 1 &&
        rawSeconds >= 0 &&
        rtcOk;

    Serial.println(
        ok
            ? "[P8-WIRE] PASS TX/RX/repeated-start"
            : "[P8-WIRE] FAIL");

    return ok;
}

static bool runSpiSdGate()
{
    Serial.println("[P8-SPI] Iniciando microSD...");

    if (!JWPLCSD::begin())
    {
        Serial.print("[P8-SPI] FAIL begin: ");
        Serial.println(JWPLCSD::lastErrorString());
        return false;
    }

    if (!JWPLCSD::isReady())
    {
        Serial.print("[P8-SPI] FAIL not ready: ");
        Serial.println(JWPLCSD::lastErrorString());
        return false;
    }

    Serial.print("[P8-SPI] Card type: ");
    Serial.println(JWPLC_SD.cardType());

    Serial.print("[P8-SPI] Card size bytes: ");
    Serial.println((unsigned long long)JWPLC_SD.cardSize());

    if (JWPLC_SD.exists(kGatePath))
    {
        if (!JWPLC_SD.remove(kGatePath))
        {
            Serial.print("[P8-SPI] FAIL remove previo: ");
            Serial.println(JWPLC_SD.lastErrorString());
            return false;
        }
    }

    JWPLCFile out = JWPLC_SD.open(kGatePath, FILE_WRITE);

    if (!out)
    {
        Serial.print("[P8-SPI] FAIL open write: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return false;
    }

    const size_t expected = strlen(kGatePayload);

    const size_t written = out.write(
        reinterpret_cast<const uint8_t *>(kGatePayload),
        expected);

    out.flush();
    out.close();

    if (written != expected)
    {
        Serial.print("[P8-SPI] FAIL write bytes: ");
        Serial.print(written);
        Serial.print("/");
        Serial.println(expected);
        return false;
    }

    JWPLCFile in = JWPLC_SD.open(kGatePath, FILE_READ);

    if (!in)
    {
        Serial.print("[P8-SPI] FAIL open read: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return false;
    }

    String readback;

    while (in.available())
    {
        readback += static_cast<char>(in.read());
    }

    in.close();

    Serial.print("[P8-SPI] Readback: ");
    Serial.println(readback);

    if (readback != kGatePayload)
    {
        Serial.println("[P8-SPI] FAIL contenido distinto");
        return false;
    }

    if (!JWPLC_SD.remove(kGatePath))
    {
        Serial.print("[P8-SPI] FAIL cleanup: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return false;
    }

    if (JWPLC_SD.exists(kGatePath))
    {
        Serial.println("[P8-SPI] FAIL archivo sigue existiendo");
        return false;
    }

    Serial.println("[P8-SPI] PASS write/read/verify/remove");
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("=== JWPLC P8 WIRE + SPI PHYSICAL GATE ===");

    const bool wireOk = runWireGate();

    Serial.println();

    const bool spiOk = runSpiSdGate();

    Serial.println();
    Serial.print("P8_WIRE_GATE=");
    Serial.println(wireOk ? "PASS" : "FAIL");

    Serial.print("P8_SPI_GATE=");
    Serial.println(spiOk ? "PASS" : "FAIL");

    Serial.println(
        (wireOk && spiOk)
            ? "P8_WIRE_SPI_GATE=PASS"
            : "P8_WIRE_SPI_GATE=FAIL");

    Serial.println("IMPORTANTE: este gate NO llama Wire.end().");
}

void loop()
{
    delay(1000);
}