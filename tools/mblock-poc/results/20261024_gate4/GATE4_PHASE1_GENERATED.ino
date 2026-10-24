// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic 0.2.0)

#include <Arduino.h>

float contador = 0;

// Pantalla JWPLC: 4 líneas de texto (campos de JWPLC_Display).
static const JWPLC_UIField _JW_SCREEN_FIELDS[] = {
  JWPLC_UITextField(1, 10, 18, nullptr, 24),
  JWPLC_UITextField(2, 10, 52, nullptr, 24),
  JWPLC_UITextField(3, 10, 86, nullptr, 24),
  JWPLC_UITextField(4, 10, 120, nullptr, 24)};

void _jw_screen_begin() {
  JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
  JWPLC_Display.setFields(_JW_SCREEN_FIELDS, 4);
  JWPLC_Display.enterUserUI();
}

void _jw_line(int line, const String &text) {
  if (line >= 1 && line <= 4) JWPLC_Display.setText((uint8_t)line, text.c_str());
}
void _jw_line(int line, const char *text) { _jw_line(line, String(text)); }
void _jw_line(int line, double value) {
  if (value == (double)(long)value) _jw_line(line, String((long)value));
  else _jw_line(line, String(value, 2));
}
void _jw_line(int line, float value) { _jw_line(line, (double)value); }
void _jw_line(int line, int value) { _jw_line(line, String(value)); }
void _jw_line(int line, long value) { _jw_line(line, String(value)); }
void _jw_line(int line, unsigned long value) { _jw_line(line, String(value)); }
void _jw_line(int line, bool value) { _jw_line(line, String(value ? "SI" : "NO")); }

void _jw_screen_clear() {
  for (int i = 1; i <= 4; i++) _jw_line(i, "");
}
void _jw_tone(double hz, double seconds) {
  unsigned long ms = (unsigned long)(seconds * 1000);
  if (ms > 65000UL) ms = 65000UL;
  JWPLC_Buzzer.tone((uint16_t)hz, (uint16_t)ms);
  delay(ms);
}

void setup() {
  _jw_screen_begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Buzzer.setVolume((uint8_t)constrain((int)((30) * 255 / 100), 0, 255));
  _jw_line(1, "Hola JWPLC");
  _jw_tone(523, 0.2);

  // DO SOMETHING

}

void loop() {
  if (JWPLC_Buttons.pressed(BTN_OK)) {
    JWPLC_Buzzer.beep();
    _jw_line(3, "OK pulsado");

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((0.5) * 1000)) {
      _jw_last = millis();
      _jw_line(3, String("t = ") + String(millis()));
      _jw_line(4, ((double)(10) / (double)(3)));

    }
  }
  if (JWPLC_Buttons.pressed(BTN_ESC)) {
    _jw_screen_clear();
    JWPLC_Buzzer.stop();

  }
  {
    static unsigned long _jw_last = 0;
    if (millis() - _jw_last >= (unsigned long)((1) * 1000)) {
      _jw_last = millis();
      contador += 1;
      _jw_line(2, contador);
      digitalWrite(Q0_7, !digitalRead(Q0_7));

    }
  }
  {
    static bool _jw_prev = false;
    bool _jw_now = (digitalRead(I0_0) == HIGH);
    if (_jw_now && !_jw_prev) {
      _jw_tone(880, 0.1);

    }
    _jw_prev = _jw_now;
  }
  if(JWPLC_Buttons.isDown(BTN_UP)){
    digitalWrite(Q0_0, HIGH);

  }else{
    digitalWrite(Q0_0, LOW);

  }
  _jw_line(4, (digitalRead(I0_1) == HIGH));

}

