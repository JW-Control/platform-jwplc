/*
  06.Runtime_Cache_Status

  Vistas ligeras del runtime JWPLC Basic.

  JWPLC_IO y JWPLC_Time NO realizan una nueva transacción SPI/I2C al consultar
  sus valores. Sólo leen snapshots que el runtime ya mantiene actualizados.

  Útil para HMI, telemetría y lógica que necesita consultar con frecuencia el
  estado actual sin aumentar tráfico de buses.
*/

#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("JWPLC Basic - Runtime cache views");
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs < 1000)
        return;

    lastPrintMs = millis();

    Serial.print("IO ready=");
    Serial.print(JWPLC_IO.ready());
    Serial.print(" IN=0x");
    if (JWPLC_IO.inputs() < 0x10) Serial.print('0');
    Serial.print(JWPLC_IO.inputs(), HEX);
    Serial.print(" OUT=0x");
    if (JWPLC_IO.outputs() < 0x10) Serial.print('0');
    Serial.print(JWPLC_IO.outputs(), HEX);
    Serial.print(" scanMs=");
    Serial.print(JWPLC_IO.lastScanMs());

    Serial.print(" | RTC present=");
    Serial.print(JWPLC_Time.present());
    Serial.print(" valid=");
    Serial.print(JWPLC_Time.valid());
    Serial.print(" updateMs=");
    Serial.println(JWPLC_Time.lastUpdateMs());
}
