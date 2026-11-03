/*
  SetFromBuildTime

  Pone el RTC en la fecha y hora en que se compiló el sketch. Sirve para
  dejar el reloj en hora la primera vez, sin pantalla ni red.

  Solo lo hace si el RTC no tiene una hora válida (batería nueva o agotada):
  si lo hiciera siempre, cada reinicio volvería el reloj a la hora de
  compilación. Para forzarlo, cambia FORZAR a true, sube el sketch y vuelve
  a ponerlo en false.

  En el JWPLC Basic el objeto global JWPLC_RTC tiene el mismo método:
  JWPLC_RTC.setFromBuildTime().

  Qué mirar: el Monitor serie (115200) dice si actualizó el RTC.
*/

#include <JW_RTC.h>

JW_RTC rtc;

const bool FORZAR = false;

void setup()
{
  Serial.begin(115200);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  if (!FORZAR && rtc.isTimeValid())
  {
    Serial.println("RTC ya tiene hora valida: no se cambia");
    return;
  }

  if (!rtc.setFromBuildTime())
  {
    Serial.print("RTC write failed: ");
    Serial.println(rtc.lastErrorString());
    return;
  }

  Serial.println("RTC updated from build time");
}

void loop()
{
}
