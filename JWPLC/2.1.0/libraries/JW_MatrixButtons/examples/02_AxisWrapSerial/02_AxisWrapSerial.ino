/*
  02_AxisWrapSerial

  Navegación por páginas y edición de un valor con applyAxis(), usando la
  botonera frontal del JWPLC Basic / Basic Core (objeto global JWPLC_Buttons,
  ya escaneado por el package; el sketch no llama a update()).

  - LEFT / RIGHT: cambian de página (con wrap).
  - OK: entra a editar / guarda.
  - UP / DOWN: suben / bajan el valor en edición (con repeat).
  - ESC: en edición, invierte el sentido de UP/DOWN.
*/

#include <Arduino.h>

static const uint8_t NUM_PAGES = 6;
uint32_t pageIdx = 0;
uint32_t pageVal[NUM_PAGES] = { 10, 50, 120, 999, 1000, 2500 };

bool editMode = false;
bool invertY = false;
uint32_t editVal = 0;

static void printState()
{
  Serial.print(editMode ? "[EDIT] " : "[PAGE] ");
  Serial.print("page=");
  Serial.print(pageIdx);
  Serial.print(" val=");
  Serial.print(editMode ? editVal : pageVal[pageIdx]);
  Serial.print(" Yinv=");
  Serial.println(invertY ? "ON" : "OFF");
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  // El package ya habilita repeat en LEFT, UP, RIGHT y DOWN.
  JWPLC_Buttons.clearPendingInput();
  printState();
}

void loop()
{
  bool changed = false;

  if (!editMode)
  {
    uint32_t before = pageIdx;
    bool ev = JWPLC_Buttons.applyAxis(pageIdx, 0, NUM_PAGES - 1, BTN_LEFT, BTN_RIGHT, true, true);
    if (ev && pageIdx != before) changed = true;

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      editMode = true;
      editVal = pageVal[pageIdx];
      changed = true;
    }
  }
  else
  {
    uint8_t decKey = invertY ? BTN_UP : BTN_DOWN;
    uint8_t incKey = invertY ? BTN_DOWN : BTN_UP;

    uint32_t before = editVal;
    bool ev = JWPLC_Buttons.applyAxis(editVal, 0, 9999, decKey, incKey, true, true);
    if (ev && editVal != before) changed = true;

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      invertY = !invertY;
      changed = true;
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      pageVal[pageIdx] = editVal;
      editMode = false;
      changed = true;
    }
  }

  if (changed) printState();
  delay(5);
}
