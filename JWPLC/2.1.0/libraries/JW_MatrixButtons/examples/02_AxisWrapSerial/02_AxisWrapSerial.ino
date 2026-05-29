#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROW_PINS[2] = { 25, 26 };
static const uint8_t COL_PINS[4] = { 35, 34, 39, 36 };

enum BtnId : uint8_t {
  BTN_INFO = 0,
  BTN_CONFIG,
  BTN_LEFT,
  BTN_START,
  BTN_RIGHT,
  BTN_UP,
  BTN_DOWN,
  BTN__COUNT
};

static const JW_MatrixButtons::BtnMapItem BTN_MAP[] = {
  { BTN_INFO, 0, 0 },
  { BTN_CONFIG, 0, 1 },
  { BTN_LEFT, 0, 2 },
  { BTN_START, 0, 3 },
  { BTN_RIGHT, 1, 0 },
  { BTN_UP, 1, 1 },
  { BTN_DOWN, 1, 2 },
};

JW_MatrixButtons btn;

static const uint8_t NUM_PAGES = 6;
uint32_t pageIdx = 0;
uint32_t pageVal[NUM_PAGES] = { 10, 50, 120, 999, 1000, 2500 };

bool editMode = false;
bool invertY = false;
uint32_t editVal = 0;

static void printState() {
  Serial.print(editMode ? "[EDIT] " : "[PAGE] ");
  Serial.print("page=");
  Serial.print(pageIdx);
  Serial.print(" val=");
  Serial.print(editMode ? editVal : pageVal[pageIdx]);
  Serial.print(" Yinv=");
  Serial.println(invertY ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  btn.begin(ROW_PINS, 2, COL_PINS, 4, BTN_MAP, (uint8_t)(sizeof(BTN_MAP) / sizeof(BTN_MAP[0])),
            BTN__COUNT, false, 35);

  btn.setRepeatEnabled(BTN_LEFT, true);
  btn.setRepeatEnabled(BTN_RIGHT, true);
  btn.setRepeatEnabled(BTN_UP, true);
  btn.setRepeatEnabled(BTN_DOWN, true);

  printState();
}

void loop() {
  btn.update();
  bool changed = false;

  if (!editMode) {
    uint32_t before = pageIdx;
    bool ev = btn.applyAxis(pageIdx, 0, NUM_PAGES - 1, BTN_LEFT, BTN_RIGHT, true, true);
    if (ev && pageIdx != before) changed = true;

    if (btn.pressed(BTN_START)) {
      editMode = true;
      editVal = pageVal[pageIdx];
      changed = true;
    }
  } else {
    uint8_t decKey = invertY ? BTN_UP : BTN_DOWN;
    uint8_t incKey = invertY ? BTN_DOWN : BTN_UP;

    uint32_t before = editVal;
    bool ev = btn.applyAxis(editVal, 0, 9999, decKey, incKey, true, true);
    if (ev && editVal != before) changed = true;

    if (btn.pressed(BTN_INFO)) {
      invertY = !invertY;
      changed = true;
    }

    if (btn.pressed(BTN_START)) {
      pageVal[pageIdx] = editVal;
      editMode = false;
      changed = true;
    }
  }

  if (changed) printState();
  delay(5);
}
