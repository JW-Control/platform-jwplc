/*
  05.microSD_Basic

  microSD integrada del JWPLC Basic.

  JWPLC_SD ya está configurada por el package para compartir el bus SPI con
  TFT, Ethernet y FRAM. Para operaciones normales use JWPLCFile, que protege
  cada operación con el mutex del ecosistema.

  Este ejemplo:
  1. comprueba tarjeta/estado;
  2. agrega una línea a /taller.txt;
  3. vuelve a leer el archivo por Serial.
*/

#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println("JWPLC Basic - microSD");
    Serial.print("Card present = ");
    Serial.println(JWPLC_SD.isCardPresent());
    Serial.print("Ready = ");
    Serial.println(JWPLC_SD.isReady());

    if (!JWPLC_SD.isReady())
    {
        Serial.print("SD status: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return;
    }

    // FILE_APPEND conserva el contenido previo y agrega al final.
    JWPLCFile file = JWPLC_SD.open("/taller.txt", FILE_APPEND);
    if (!file)
    {
        Serial.print("Open write failed: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return;
    }

    file.print("Arranque ms=");
    file.println(millis());
    file.close();

    Serial.println("Contenido de /taller.txt:");

    JWPLCFile readFile = JWPLC_SD.open("/taller.txt", FILE_READ);
    if (!readFile)
    {
        Serial.println("No se pudo abrir para lectura.");
        return;
    }

    while (readFile.available())
    {
        Serial.write((uint8_t)readFile.read());
    }

    readFile.close();
    Serial.println();
}

void loop()
{
    delay(1000);
}
