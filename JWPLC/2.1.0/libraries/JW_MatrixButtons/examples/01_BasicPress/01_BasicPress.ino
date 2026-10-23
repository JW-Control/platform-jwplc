/*
  01_BasicPress

  Pulsaciones de la botonera frontal del JWPLC Basic / Basic Core.

  La botonera es una matriz 2x3 (pines JWPLC_BTN_ROW*_PIN y JWPLC_BTN_COL*_PIN
  de jwplc_hardware_config.h) que el package ya escanea en segundo plano con
  el objeto global JWPLC_Buttons (un JW_MatrixButtons). El sketch no crea otra
  instancia sobre los mismos pines ni llama a update().

    fila 0: BTN_LEFT  BTN_UP  BTN_RIGHT
    fila 1: BTN_ESC   BTN_OK  BTN_DOWN
*/

#include <Arduino.h>

static void logPress(const char *name)
{
  Serial.print("[PRESS] ");
  Serial.println(name);
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  JWPLC_Buttons.clearPendingInput();
  Serial.println("BasicPress listo.");
}

void loop()
{
  if (JWPLC_Buttons.pressed(BTN_LEFT)) logPress("LEFT");
  if (JWPLC_Buttons.pressed(BTN_UP)) logPress("UP");
  if (JWPLC_Buttons.pressed(BTN_RIGHT)) logPress("RIGHT");
  if (JWPLC_Buttons.pressed(BTN_ESC)) logPress("ESC");
  if (JWPLC_Buttons.pressed(BTN_OK)) logPress("OK");
  if (JWPLC_Buttons.pressed(BTN_DOWN)) logPress("DOWN");

  delay(5);
}
