/*
  BasicReadWrite

  Añade la línea «Hola desde JW_SD» al archivo /jw_sd_test.txt de la
  microSD y luego lee el archivo completo.

  Conexión: una microSD formateada en FAT32 en la ranura del JWPLC Basic.
  En el JWPLC Basic el package ya publica el objeto global JWPLC_SD
  (ver JWPLC_GlobalPeripherals/05.microSD_Basic).

  Qué mirar: el Monitor serie (115200) imprime «SD ready» y el contenido
  del archivo, con una línea más en cada reinicio.
*/

#include <JW_SD.h>

#ifndef SD_CS
#define SD_CS JWPLC_SD_CS  // CS de la microSD del JWPLC Basic (GPIO32)
#endif

JW_SD sd(SD_CS);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("JW_SD BasicReadWrite");

    if (!sd.begin())
    {
        Serial.print("SD begin failed: ");
        Serial.println(sd.lastErrorString());
        return;
    }

    Serial.println("SD ready");

    auto file = sd.open("/jw_sd_test.txt", FILE_APPEND);

    if (!file)
    {
        Serial.print("Open failed: ");
        Serial.println(sd.lastErrorString());
        return;
    }

    file.println("Hola desde JW_SD");
    file.close();

    file = sd.open("/jw_sd_test.txt", FILE_READ);

    if (!file)
    {
        Serial.print("Read open failed: ");
        Serial.println(sd.lastErrorString());
        return;
    }

    Serial.println("Contenido:");
    while (file.available())
    {
        Serial.write(file.read());
    }

    file.close();
}

void loop()
{
}
