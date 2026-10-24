// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.4.0)

#include <Arduino.h>
#include "JWPLC_mBlock.h"

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Clock.setFromComputer();
  JWPLC_Memory.save(1, (JWPLC_Memory.load(1) + 1));
  JWPLC_Screen.print(1, JWPLC_Join("Arranques: ", JWPLC_Memory.load(1)));

}

void loop() {
  if (JWPLC_Buttons.pressed(BTN_OK)) {
    JWPLC_Timer.reset();

  }
  if (JWPLC_Buttons.pressed(BTN_UP)) {
    JWPLC_Screen.print(4, JWPLC_Join("Crono s: ", JWPLC_Timer.seconds()));

  }
  if (JWPLC_Buttons.pressed(BTN_DOWN)) {
    JWPLC_Screen.print(4, JWPLC_Join("Minuto: ", JWPLC_Time.minute()));

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      JWPLC_Screen.print(2, JWPLC_Clock.timeText());
      JWPLC_Screen.print(3, JWPLC_Clock.dateText());
      JWPLC_Screen.print(4, JWPLC_Join("Temp C: ", JWPLC_Clock.temperature()));
      if(JWPLC_Files.ready()){
        JWPLC_Files.appendLine("registro.csv", JWPLC_Join(JWPLC_Clock.timeText(), JWPLC_Join(";", JWPLC_Clock.temperature())));

      }

    }
  }
  if (JWPLC_Buttons.pressed(BTN_RIGHT)) {
    JWPLC_Screen.print(4, JWPLC_Files.exists("registro.csv"));

  }
  if (JWPLC_Buttons.pressed(BTN_ESC)) {
    JWPLC_Files.remove("registro.csv");
    JWPLC_Buzzer.beep();

  }
}

