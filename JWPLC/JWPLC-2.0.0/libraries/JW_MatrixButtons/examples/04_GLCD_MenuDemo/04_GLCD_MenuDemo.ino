#include <Arduino.h>
#include <U8g2lib.h>
#include <JW_MatrixButtons.h>

// =========================
// GLCD ST7920 128x64 (2ND HW SPI)
// =========================
static const uint8_t CS_GLCD  = 15;
static const uint8_t RST_GLCD = 4;
U8G2_ST7920_128X64_F_2ND_HW_SPI u8g2(U8G2_R0, CS_GLCD, RST_GLCD);

// =========================
// Matriz botones (tu hardware)
// Filas: IO25, IO26
// Columnas: IO35, IO34, IO39, IO36
// =========================
static const uint8_t ROW_PINS[2] = {25, 26};
static const uint8_t COL_PINS[4] = {35, 34, 39, 36};

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

// Mapeo confirmado
static const JW_MatrixButtons::BtnMapItem BTN_MAP[] = {
  {BTN_INFO,   0, 0}, // SW1
  {BTN_CONFIG, 0, 1}, // SW2
  {BTN_LEFT,   0, 2}, // SW3
  {BTN_START,  0, 3}, // SW4
  {BTN_RIGHT,  1, 0}, // SW5
  {BTN_UP,     1, 1}, // SW6
  {BTN_DOWN,   1, 2}, // SW7
};

JW_MatrixButtons btn;

// Demo páginas/valores
static const uint8_t NUM_PAGES = 6;
uint32_t pageIdx = 0;
uint32_t pageVal[NUM_PAGES] = {10, 50, 120, 999, 1000, 2500};

bool editMode = false;
bool invertY  = false;
uint32_t editVal = 0;

// UI state
static char statusLine[24] = "Listo";

static void setStatus(const char* msg) {
  strncpy(statusLine, msg, sizeof(statusLine) - 1);
  statusLine[sizeof(statusLine) - 1] = '\0';
}

static void drawTabs(uint8_t active, uint8_t n) {
  // 6 tabs entran bien; generalizamos
  const uint8_t tabH = 9;
  const uint8_t y = 54;
  const uint8_t gap = 2;
  uint8_t tabW;

  // ancho para que entre: totalW = n*tabW + (n-1)*gap <= 124 (dejando márgenes)
  tabW = (uint8_t)((124 - (n - 1) * gap) / n);
  uint8_t x = (uint8_t)((128 - (n * tabW + (n - 1) * gap)) / 2);

  for (uint8_t i = 0; i < n; i++) {
    if (i == active) {
      u8g2.drawBox(x, y, tabW, tabH);
      u8g2.setDrawColor(0);
      char buf[4];
      snprintf(buf, sizeof(buf), "%u", i);
      u8g2.drawStr(x + 4, y + 8, buf);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(x, y, tabW, tabH);
      char buf[4];
      snprintf(buf, sizeof(buf), "%u", i);
      u8g2.drawStr(x + 4, y + 8, buf);
    }
    x = (uint8_t)(x + tabW + gap);
  }
}

static void drawUI() {
  u8g2.clearBuffer();

  // Header (línea superior)
  u8g2.setFont(u8g2_font_6x10_tf);

  // izquierda: modo
  u8g2.drawStr(0, 10, editMode ? "EDIT" : "PAGE");

  // centro: page
  char buf[24];
  snprintf(buf, sizeof(buf), "P:%lu/%u", (unsigned long)pageIdx, (unsigned)NUM_PAGES - 1);
  u8g2.drawStr(46, 10, buf);

  // derecha: Yinv
  u8g2.drawStr(98, 10, invertY ? "Y:ON" : "Y:OFF");

  // Valor grande
  uint32_t v = editMode ? editVal : pageVal[pageIdx];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);

  u8g2.setFont(u8g2_font_logisoso24_tn);
  // centrado aproximado por largo
  uint8_t len = strlen(buf);
  int x = 64 - (len * 12); // 32px font: aprox 16px por dígito
  if (x < 0) x = 0;
  u8g2.drawStr(x, 38, buf);

  // Tabs de páginas
  u8g2.setFont(u8g2_font_6x10_tf);
  drawTabs((uint8_t)pageIdx, NUM_PAGES);

  // Status line
  u8g2.drawStr(0, 50, statusLine);

  u8g2.sendBuffer();
}

void setup() {
  // Botonera
  btn.begin(ROW_PINS, 2, COL_PINS, 4,
            BTN_MAP, (uint8_t)(sizeof(BTN_MAP)/sizeof(BTN_MAP[0])),
            BTN__COUNT,
            false, 35);

  btn.setRepeatEnabled(BTN_LEFT,  true);
  btn.setRepeatEnabled(BTN_RIGHT, true);
  btn.setRepeatEnabled(BTN_UP,    true);
  btn.setRepeatEnabled(BTN_DOWN,  true);

  // GLCD
  u8g2.setBusClock(530000);
  u8g2.begin();
  u8g2.enableUTF8Print();

  setStatus("JW_MatrixButtons OK");
  drawUI();
}

void loop() {
  btn.update();
  bool changed = false;

  if (!editMode) {
    // Navegación páginas
    changed |= btn.applyAxis(pageIdx, 0, NUM_PAGES - 1,
                             BTN_LEFT, BTN_RIGHT,
                             true,   // wrap solo al re-press en extremos
                             true);  // snap con repeat

    if (btn.pressed(BTN_START)) {
      editMode = true;
      editVal = pageVal[pageIdx];
      setStatus("Edit: UP/DN  START=OK");
      changed = true;
    }

  } else {
    // INFO invierte el eje Y en modo edición
    if (btn.pressed(BTN_INFO)) {
      invertY = !invertY;
      setStatus(invertY ? "Y invertido ON" : "Y invertido OFF");
      changed = true;
    }

    uint8_t decKey = invertY ? BTN_UP   : BTN_DOWN;
    uint8_t incKey = invertY ? BTN_DOWN : BTN_UP;

    changed |= btn.applyAxis(editVal, 0, 9999,
                             decKey, incKey,
                             true,   // wrap solo al re-press
                             true);  // snap con repeat

    if (btn.pressed(BTN_START)) {
      pageVal[pageIdx] = editVal;
      editMode = false;
      setStatus("Guardado");
      changed = true;
    }
  }

  if (changed) drawUI();
  delay(5);
}
