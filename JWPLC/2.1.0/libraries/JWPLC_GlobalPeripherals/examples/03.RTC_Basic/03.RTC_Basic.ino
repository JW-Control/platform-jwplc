/*
  03.RTC_Basic

  RTC integrado del JWPLC Basic.

  JWPLC_Time entrega una vista cacheada por el runtime. Es la forma más ligera
  de consultar fecha/hora repetidamente porque no inicia una transacción I2C
  nueva en cada llamada.

  JWPLC_RTC sigue disponible cuando se necesita una función específica del
  dispositivo, por ejemplo leer su sensor interno de temperatura.
*/

#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println("JWPLC Basic - RTC");
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 1000)
        return;

    lastPrintMs = millis();

    Serial.print("RTC present=");
    Serial.print(JWPLC_Time.present());
    Serial.print(" valid=");
    Serial.print(JWPLC_Time.valid());
    Serial.print(" lostPower=");
    Serial.print(JWPLC_Time.lostPower());

    if (JWPLC_Time.valid())
    {
        Serial.print(" | ");
        Serial.print(JWPLC_Time.day());
        Serial.print('/');
        Serial.print(JWPLC_Time.month());
        Serial.print('/');
        Serial.print(JWPLC_Time.year());
        Serial.print(' ');

        if (JWPLC_Time.hour() < 10) Serial.print('0');
        Serial.print(JWPLC_Time.hour());
        Serial.print(':');
        if (JWPLC_Time.minute() < 10) Serial.print('0');
        Serial.print(JWPLC_Time.minute());
        Serial.print(':');
        if (JWPLC_Time.second() < 10) Serial.print('0');
        Serial.print(JWPLC_Time.second());
    }

    // Lectura específica del RTC. No hace falta llamar begin():
    // el package ya inicializó el objeto global JWPLC_RTC.
    float temperatureC = 0.0f;
    if (JWPLC_RTC.readTemperatureC(temperatureC))
    {
        Serial.print(" | tempRTC=");
        Serial.print(temperatureC, 2);
        Serial.print(" C");
    }

    Serial.println();
}
