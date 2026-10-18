// Gate A15 I2: VALUE de fecha/hora leído del RTC de la placa.
// JWPLC_HMI_Generated.h sale del HMI Designer (tests/golden/i2Clock.h).
//
// Hora (HH:MM:SS) y Fecha (DD/MM/AAAA) no tienen variable: el firmware las
// toma de jwplcGetRTCState(). Temp sí es una variable normal, para comparar.
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup();
}

void loop()
{
    static uint32_t last = 0;
    if (millis() - last >= 2000)
    {
        last = millis();
        temperatura = 20.0f + (float)((millis() / 2000) % 50) / 10.0f;
    }
}
