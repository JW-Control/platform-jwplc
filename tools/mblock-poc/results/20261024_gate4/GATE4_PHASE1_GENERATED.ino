// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.3.0)

#include <Arduino.h>
#include "JWPLC_mBlock.h"

float contador = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Sound.volume(30);
  JWPLC_Screen.print(1, "Hola JWPLC");
  JWPLC_Sound.play(523, 0.2);

  // DO SOMETHING

}

void loop() {
  if (JWPLC_Buttons.pressed(BTN_OK)) {
    JWPLC_Buzzer.beep();
    JWPLC_Screen.print(3, "OK pulsado");

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((0.5) * 1000)) {
      _jw_last = millis();
      JWPLC_Screen.print(3, String("t = ") + String(millis()));
      JWPLC_Screen.print(4, ((double)(10) / (double)(3)));

    }
  }
  if (JWPLC_Buttons.pressed(BTN_ESC)) {
    JWPLC_Screen.clear();
    JWPLC_Buzzer.stop();

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      contador += 1;
      JWPLC_Screen.print(2, contador);
      digitalWrite(Q0_7, !digitalRead(Q0_7));

    }
  }
  {
    static bool _jw_prev = false;
    bool _jw_now = (digitalRead(I0_0) == HIGH);
    if (_jw_now && !_jw_prev) {
      JWPLC_Sound.play(880, 0.1);

    }
    _jw_prev = _jw_now;
  }
  if(JWPLC_Buttons.isDown(BTN_UP)){
    digitalWrite(Q0_0, HIGH);

  }else{
    digitalWrite(Q0_0, LOW);

  }
  JWPLC_Screen.print(4, (digitalRead(I0_1) == HIGH));

}

