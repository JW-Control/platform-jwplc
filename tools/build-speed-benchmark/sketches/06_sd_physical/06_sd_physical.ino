#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>

static const char *TEST_PATH = "/jwplc_alpha5_sd.txt";
static const char *EXPECTED = "JWPLC Alpha5 SD source PASS";

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("JWPLC Alpha5 - gate fisico microSD");

    Serial.print("SD enabled: ");
    Serial.println(JWPLCSD::isEnabled() ? "YES" : "NO");

    const bool ready = JWPLCSD::begin();

    Serial.print("SD ready: ");
    Serial.println(ready ? "YES" : "NO");

    Serial.print("Card present: ");
    Serial.println(JWPLCSD::isCardPresent() ? "YES" : "NO");

    if (!ready)
    {
        Serial.print("[FAIL] SD begin: ");
        Serial.println(JWPLCSD::lastErrorString());
        return;
    }

    if (!JWPLCSD::isCardPresent())
    {
        Serial.println("[FAIL] No se detecta tarjeta microSD");
        return;
    }

    if (JWPLC_SD.exists(TEST_PATH))
    {
        if (!JWPLC_SD.remove(TEST_PATH))
        {
            Serial.print("[FAIL] No se pudo limpiar archivo de prueba: ");
            Serial.println(JWPLC_SD.lastErrorString());
            return;
        }
    }

    JWPLCFile file = JWPLC_SD.open(TEST_PATH, FILE_WRITE);
    if (!file)
    {
        Serial.print("[FAIL] Open write: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return;
    }

    file.println(EXPECTED);
    file.close();

    file = JWPLC_SD.open(TEST_PATH, FILE_READ);
    if (!file)
    {
        Serial.print("[FAIL] Open read: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return;
    }

    String content;
    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();
    content.trim();

    Serial.print("Leido: ");
    Serial.println(content);

    if (content == EXPECTED)
    {
        Serial.println("[PASS] Escritura y lectura microSD correctas");
    }
    else
    {
        Serial.println("[FAIL] El contenido leido no coincide");
    }
}

void loop()
{
}
