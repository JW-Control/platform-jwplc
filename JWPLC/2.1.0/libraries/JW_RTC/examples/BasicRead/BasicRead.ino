/*
  BasicRead

  Lee la fecha, la hora y la temperatura del RTC DS3232M una vez por segundo.

  En el JWPLC Basic el package ya inicia el RTC y lo publica en el objeto
  global JWPLC_RTC (y en JWPLC_Time, que guarda la última lectura). Este
  ejemplo crea su propio objeto JW_RTC para mostrar la librería sola; en un
  programa del JWPLC usa JWPLC_RTC o JWPLC_Time
  (ver JWPLC_GlobalPeripherals/03.RTC_Basic).

  Qué mirar: el Monitor serie (115200) imprime «2026/11/02 10:15:30» y la
  temperatura del chip.
*/

#include <JW_RTC.h>

JW_RTC rtc;

const uint32_t PERIODO_MS = 1000;
uint32_t ultimaLecturaMs = 0;

// Dos dígitos: 7 -> "07".
String dosDigitos(uint8_t valor)
{
  return (valor < 10) ? "0" + String(valor) : String(valor);
}

void setup()
{
  Serial.begin(115200);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  Serial.println("RTC found");
}

void loop()
{
  if (millis() - ultimaLecturaMs < PERIODO_MS)
    return;
  ultimaLecturaMs = millis();

  JWRTCDateTime dt;
  if (rtc.read(dt))
  {
    Serial.print("Valid: ");
    Serial.print(rtc.isTimeValid());
    Serial.print(" | ");
    Serial.println(String(dt.year) + "/" + dosDigitos(dt.month) + "/" + dosDigitos(dt.day) + " " +
                   dosDigitos(dt.hour) + ":" + dosDigitos(dt.minute) + ":" + dosDigitos(dt.second));
  }
  else
  {
    Serial.print("Read failed: ");
    Serial.println(rtc.lastErrorString());
  }

  float tempC = 0.0f;
  if (rtc.readTemperatureC(tempC))
  {
    Serial.print("Temp C: ");
    Serial.println(tempC, 2);
  }
}
