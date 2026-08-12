#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

static const char *kGatePath = "/jwplc_p7_gate.txt";
static const char *kGatePayload = "JWPLC_P7_FS_SD_OK";

static bool runSdGate()
{
    Serial.println("[P7-SD] Iniciando microSD...");

    if (!JWPLCSD::begin())
    {
        Serial.print("[P7-SD] FAIL begin: ");
        Serial.println(JWPLCSD::lastErrorString());
        return false;
    }

    if (!JWPLCSD::isReady())
    {
        Serial.print("[P7-SD] FAIL not ready: ");
        Serial.println(JWPLCSD::lastErrorString());
        return false;
    }

    Serial.print("[P7-SD] Card type: ");
    Serial.println(JWPLC_SD.cardType());
    Serial.print("[P7-SD] Card size bytes: ");
    Serial.println((unsigned long long)JWPLC_SD.cardSize());

    if (JWPLC_SD.exists(kGatePath))
    {
        if (!JWPLC_SD.remove(kGatePath))
        {
            Serial.print("[P7-SD] FAIL remove previo: ");
            Serial.println(JWPLC_SD.lastErrorString());
            return false;
        }
    }

    JWPLCFile out = JWPLC_SD.open(kGatePath, FILE_WRITE);
    if (!out)
    {
        Serial.print("[P7-SD] FAIL open write: ");
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
        Serial.print("[P7-SD] FAIL write bytes: ");
        Serial.print(written);
        Serial.print("/");
        Serial.println(expected);
        return false;
    }

    if (!JWPLC_SD.exists(kGatePath))
    {
        Serial.println("[P7-SD] FAIL archivo no existe tras escritura");
        return false;
    }

    JWPLCFile in = JWPLC_SD.open(kGatePath, FILE_READ);
    if (!in)
    {
        Serial.print("[P7-SD] FAIL open read: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return false;
    }

    String readback;
    while (in.available())
    {
        readback += static_cast<char>(in.read());
    }
    in.close();

    Serial.print("[P7-SD] Readback: ");
    Serial.println(readback);

    if (readback != kGatePayload)
    {
        Serial.println("[P7-SD] FAIL contenido distinto");
        return false;
    }

    if (!JWPLC_SD.remove(kGatePath))
    {
        Serial.print("[P7-SD] FAIL cleanup: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return false;
    }

    if (JWPLC_SD.exists(kGatePath))
    {
        Serial.println("[P7-SD] FAIL archivo sigue existiendo tras cleanup");
        return false;
    }

    Serial.println("[P7-SD] PASS write/read/verify/remove");
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("=== JWPLC P7 FS+SD PHYSICAL GATE ===");

    const bool ok = runSdGate();
    Serial.println(ok ? "P7_SD_GATE=PASS" : "P7_SD_GATE=FAIL");
}

void loop()
{
    delay(1000);
}
