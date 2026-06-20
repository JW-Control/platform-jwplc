#include <Arduino.h>
#include <JW_MatrixButtons.h>

/*
  Ejemplo para botones directos con GPIO35, GPIO34 y GPIO39.

  Importante en ESP32:
  - GPIO34, GPIO35 y GPIO39 son solo entrada.
  - No tienen pull-up/pull-down interno usable.
  - Usa resistencias externas.

  Este ejemplo asume:
  - Botón presionado = LOW.
  - Cada GPIO tiene pull-up externo.
  - Cada botón conecta el GPIO a GND al presionar.

  Si tu hardware es activo HIGH con pulldown externo:
  - cambia invertLogic a false.
*/

enum BtnId : uint8_t {
  BTN_LEFT = 0,
  BTN_RIGHT,
  BTN_CENTER,
  BTN_COUNT
};

static const uint8_t BTN_PINS[] = {
  35,  // IO35 -> LEFT
  34,  // IO34 -> RIGHT
  39   // IO39 -> CENTER
};

JW_MatrixButtons btn;
uint32_t value = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JW_MatrixButtons - Direct Buttons Test");
  Serial.println("GPIO35=LEFT, GPIO34=RIGHT, GPIO39=CENTER");

  bool ok = btn.beginDirect(
    BTN_PINS,
    BTN_COUNT,
    false,  // true: presionado = LOW
    35,     // debounce ms
    INPUT   // GPIO34/35/39 requieren resistencias externas
  );

  if (!ok) {
    Serial.println("ERROR: beginDirect() fallo");
    return;
  }

  btn.setRepeatEnabled(BTN_LEFT, true);
  btn.setRepeatEnabled(BTN_RIGHT, true);

  Serial.println("Listo.");
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_LEFT)) {
    Serial.println("LEFT press");
  }

  if (btn.pressed(BTN_RIGHT)) {
    Serial.println("RIGHT press");
  }

  if (btn.released(BTN_LEFT)) {
    Serial.println("LEFT release");
  }

  if (btn.released(BTN_RIGHT)) {
    Serial.println("RIGHT release");
  }

  if (btn.pressed(BTN_CENTER)) {
    Serial.println("CENTER press");
    btn.clearPendingInput();
  }

  if (btn.released(BTN_CENTER)) {
    Serial.println("CENTER release");
  }

  if (btn.applyAxis(&value, 0, 1000, BTN_LEFT, BTN_RIGHT)) {
    Serial.print("value=");
    Serial.println(value);
  }

  JW_MatrixButtons::BtnEvent ev;
  for (uint8_t i = 0; i < btn.eventCount(); i++) {
    if (btn.getEvent(i, ev)) {
      Serial.print("event id=");
      Serial.print(ev.id);
      Serial.print(" type=");
      Serial.print((int)ev.type);
      Serial.print(" mult=");
      Serial.print(ev.mult);
      Serial.print(" held_ms=");
      Serial.println(ev.held_ms);
    }
  }

  delay(5);
}
