/*
  NvramDemo

  Cuenta los arranques del equipo en la memoria NVRAM del RTC DS3232M
  (236 bytes con batería: no se borran al quitar la alimentación).

  En el JWPLC Basic el objeto global JWPLC_RTC tiene los mismos métodos
  (JWPLC_RTC.nvramReadObject(...)). Para guardar más datos usa la FRAM
  (JWPLC_GlobalPeripherals/04.FRAM_Basic).

  Qué mirar: el Monitor serie (115200) imprime «Boot counter: N», que sube
  en cada reinicio.
*/

#include <JW_RTC.h>

JW_RTC rtc;

const uint16_t MARCA_VALIDA = 0x55AA;

struct DemoData
{
  uint32_t bootCounter;
  uint16_t marker;
};

void setup()
{
  Serial.begin(115200);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  DemoData data{};
  if (!rtc.nvramReadObject(0, data))
  {
    Serial.print("NVRAM read failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  // Primera vez (o NVRAM sin datos nuestros): empezar de cero.
  if (data.marker != MARCA_VALIDA)
  {
    data.marker = MARCA_VALIDA;
    data.bootCounter = 0;
  }

  data.bootCounter++;

  if (!rtc.nvramWriteObject(0, data))
  {
    Serial.print("NVRAM write failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  Serial.print("Boot counter: ");
  Serial.println(data.bootCounter);
}

void loop()
{
}
