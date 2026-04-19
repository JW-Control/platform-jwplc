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

static void logPress(const char* name) {
  Serial.print("[PRESS] ");
  Serial.println(name);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  btn.begin(ROW_PINS, 2, COL_PINS, 4, BTN_MAP, (uint8_t)(sizeof(BTN_MAP) / sizeof(BTN_MAP[0])),
            BTN__COUNT, false, 35);

  Serial.println("BasicPress ready.");
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_INFO)) logPress("INFO");
  if (btn.pressed(BTN_CONFIG)) logPress("CONFIG");
  if (btn.pressed(BTN_LEFT)) logPress("LEFT");
  if (btn.pressed(BTN_RIGHT)) logPress("RIGHT");
  if (btn.pressed(BTN_UP)) logPress("UP");
  if (btn.pressed(BTN_DOWN)) logPress("DOWN");
  if (btn.pressed(BTN_START)) logPress("START");

  delay(5);
}
