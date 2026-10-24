// Simulación en host del ICON de JWPLC_Display (Alpha16): compila el
// JWPLC_UI.cpp real contra stubs (icon-host/stub) y verifica la decodificación
// 4 bpp, el transparente, los estados, la etiqueta y que un cambio de estado
// sólo repinta el recuadro del icono. Guarda icon_sim.ppm.
#include "sim_stubs.h"

// ---- iconos de prueba --------------------------------------------------------
// 5x3 (ancho impar: cada fila ocupa 3 bytes), índice 0 transparente.
static const uint8_t SMALL_PX[] = {
  0x01, 0x23, 0x40,
  0x12, 0x34, 0x00,
  0xF0, 0x0F, 0x10,
};
static const uint16_t PAL_A[16] = { 0x1111, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFFFF };
static const uint16_t PAL_B[16] = { 0x2222, 0x8410, 0x8410, 0x8410, 0x8410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0001 };
static const JWPLC_UIIconState SMALL_STATES[] = { { SMALL_PX, PAL_A }, { SMALL_PX, PAL_B } };
static const JWPLC_UIIcon SMALL(5, 3, 2, SMALL_STATES, 0);

// 64x64 sin transparente (8 envíos de 8 filas): diagonal de índices.
static uint8_t BIG_PX[64 * 32];
static uint16_t BIG_PAL0[16], BIG_PAL1[16], BIG_PAL2[16];
static const JWPLC_UIIconState BIG_STATES[] = { { BIG_PX, BIG_PAL0 }, { BIG_PX, BIG_PAL1 }, { BIG_PX, BIG_PAL2 } };
static const JWPLC_UIIcon BIG(64, 64, 3, BIG_STATES);

// 4x2 para el ICON sin fondo: (0,0) y (3,1) transparentes en los dos estados;
// (1,0) sólo en el estado 0.
static const uint8_t TR_PX0[] = { 0x00, 0x12, 0x12, 0x10 };
static const uint8_t TR_PX1[] = { 0x01, 0x12, 0x12, 0x10 };
static const JWPLC_UIIconState TR_STATES[] = { { TR_PX0, PAL_A }, { TR_PX1, PAL_A } };
static const JWPLC_UIIcon TR(4, 2, 2, TR_STATES, 0);

enum : uint8_t { ID_SMALL = 1, ID_LABEL, ID_BIG, ID_INLINE, ID_TRANSP };

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) printf("  OK  " __VA_ARGS__); else { printf("  FAIL " __VA_ARGS__); ++failures; } printf("\n"); } while (0)

static uint16_t refPixel(const JWPLC_UIIcon &icon, uint8_t state, int x, int y, uint16_t bg)
{
  if (state >= icon.stateCount) state = icon.stateCount - 1;
  const uint8_t b = icon.states[state].pixels[y * ((icon.width + 1) / 2) + x / 2];
  const uint8_t i = (x % 2) ? (b & 0x0F) : (b >> 4);
  return i == icon.transparentIndex ? bg : icon.states[state].palette[i];
}

static bool iconMatches(JWPLC_TFTClass &tft, const JWPLC_UIIcon &icon, uint8_t state, int ox, int oy, uint16_t bg)
{
  for (int y = 0; y < icon.height; ++y)
    for (int x = 0; x < icon.width; ++x)
      if (tft.fb[(oy + y) * 320 + ox + x] != refPixel(icon, state, x, y, bg))
      {
        printf("    (%d,%d) = %04X, esperado %04X\n", x, y, tft.fb[(oy + y) * 320 + ox + x], refPixel(icon, state, x, y, bg));
        return false;
      }
  return true;
}

int main(int argc, char **argv)
{
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; x += 2)
      BIG_PX[y * 32 + x / 2] = (uint8_t)((((x + y) & 15) << 4) | ((x + 1 + y) & 15));
  for (int i = 0; i < 16; ++i) { BIG_PAL0[i] = (uint16_t)(i * 0x0841); BIG_PAL1[i] = (uint16_t)(0xF800 | i); BIG_PAL2[i] = (uint16_t)(0x07E0 | i); }

  const JWPLC_UIField fields[] = {
    JWPLC_UIIconField(ID_SMALL, JWPLC_UIRect(10, 10), &SMALL, JWPLC_UIText(), JWPLC_UIIconStyle(), 0, JWPLC_UIColors(0xFFFF, 0xFFFF, 0x0841, 0xFFFF)),
    JWPLC_UIIconField(ID_LABEL, JWPLC_UIRect(40, 10), &SMALL, JWPLC_UIText("Motor"), JWPLC_UIIconStyle(1, true), 0, JWPLC_UIColors(0xFFE0, 0xFFFF, 0x0010, 0x07FF)),
    JWPLC_UIIconField(ID_BIG, JWPLC_UIRect(100, 10), &BIG, JWPLC_UIText("Tanque"), JWPLC_UIIconStyle(), 0),
    JWPLC_UIIconField(ID_INLINE, JWPLC_UIRect(10, 120), &SMALL, JWPLC_UIText("M2"), JWPLC_UIIconStyle(2, false, JWPLC_UI_LAYOUT_INLINE), 0),
    JWPLC_UIIconField(ID_TRANSP, JWPLC_UIRect(200, 120), &TR, JWPLC_UIText("T"), JWPLC_UIIconStyle(), 0, JWPLC_UIColors(0xFFFF, 0xFFFF, 0x0841, 0xFFFF), true),
  };

  static JWPLC_TFTClass tft;
  printf("ICON en host (JWPLC_UI.cpp real)\n");
  CHECK(JWPLCUI::setFields(fields, sizeof(fields) / sizeof(fields[0])), "setFields acepta 5 ICON");
  tft.fillScreen(0x0000);
  tft.fillRect(190, 110, 60, 40, 0xABCD); // gráfica estática bajo el ICON sin fondo
  JWPLCUI::drawStatic(tft);
  JWPLCUI::drawDirty(tft);

  // Sin etiqueta: el icono empieza en x + 3, y + 3 (margen fijo del campo).
  CHECK(iconMatches(tft, SMALL, 0, 13, 13, 0x0841), "estado 0 sin setter, ancho impar, transparente = fondo del campo");
  CHECK(tft.fb[10 * 320 + 10] == 0x0841 && tft.fb[(13 + 3 + 2) * 320 + 13 + 5 + 2] == 0x0841,
        "recuadro AUTO = icono + margen 3/3 con el fondo del campo");
  // Etiqueta arriba (STACKED): 7 px de texto + 4 de separación.
  CHECK(iconMatches(tft, SMALL, 0, 43, 10 + 3 + 7 + 4, 0x0010), "con etiqueta STACKED el icono va bajo la etiqueta");
  CHECK(tft.fb[10 * 320 + 40] == 0x07FF, "marco del campo");
  CHECK(iconMatches(tft, BIG, 0, 103, 24, 0x0000), "64x64 en 8 envíos");
  // INLINE con etiqueta de escala 2: "M2" mide 2*6*2 - 2 = 22 px + 4 de separación.
  CHECK(iconMatches(tft, SMALL, 0, 10 + 3 + 22 + 4, 123, 0x0000), "INLINE: el icono va a la derecha de la etiqueta");

  // Sin fondo: icono en (203, 134) bajo la etiqueta "T" (STACKED).
  const auto at = [&](int x, int y) { return tft.fb[y * 320 + x]; };
  CHECK(at(200, 120) == 0xABCD && at(208, 123) == 0xABCD, "sin fondo: ni recuadro ni fondo detrás de la etiqueta");
  CHECK(at(203, 123) != 0xABCD || at(204, 123) != 0xABCD || at(205, 124) != 0xABCD, "sin fondo: la etiqueta sí se dibuja");
  CHECK(at(203, 134) == 0xABCD && at(206, 135) == 0xABCD, "sin fondo: transparente en todos los estados no se pinta");
  CHECK(at(204, 134) == 0x0841 && at(205, 134) == 0xF800 && at(206, 134) == 0x07E0, "sin fondo: transparente sólo en este estado toma el fondo del campo");
  JWPLC_Display.setIconState(ID_TRANSP, 1);
  JWPLCUI::drawDirty(tft);
  CHECK(at(203, 134) == 0xABCD && at(204, 134) == 0xF800, "sin fondo: el cambio de estado pinta lo que cambia y no toca el resto");

  // Cambio de estado: sólo pushImage del icono (sin fillScreen ni fillRect).
  tft.fillScreens = tft.fillRects = tft.pushImages = 0;
  CHECK(JWPLC_Display.setIconState(ID_SMALL, 1), "setIconState(1) válido");
  CHECK(JWPLCUI::refreshNeeded(), "cambio de estado pide refresco");
  JWPLCUI::drawDirty(tft);
  CHECK(iconMatches(tft, SMALL, 1, 13, 13, 0x0841), "estado 1: misma imagen, otra paleta");
  CHECK(tft.fillScreens == 0 && tft.fillRects == 0 && tft.pushImages == 1, "sólo un pushImage (fillRect=%d push=%d)", tft.fillRects, tft.pushImages);

  JWPLCUI::consumeRefreshRequest();
  JWPLCUI::consumePageRedrawPending();
  JWPLC_Display.setIconState(ID_SMALL, 1);
  CHECK(!JWPLCUI::refreshNeeded(), "mismo estado: nada que repintar");

  tft.pushImages = 0;
  CHECK(JWPLC_Display.setBool(ID_BIG, true), "setBool en un ICON = estado 1");
  JWPLCUI::drawDirty(tft);
  CHECK(iconMatches(tft, BIG, 1, 103, 24, 0x0000) && tft.pushImages == 8, "64x64 estado 1 (push=%d)", tft.pushImages);
  CHECK(!JWPLC_Display.setIconState(ID_BIG, 7), "estado fuera de rango devuelve false");
  JWPLCUI::drawDirty(tft);
  CHECK(iconMatches(tft, BIG, 2, 103, 24, 0x0000), "fuera de rango se ve el último estado");
  CHECK(!JWPLC_Display.setIconState(99, 1), "id inexistente devuelve false");
  CHECK(!JWPLC_Display.setText(ID_SMALL, "x"), "setText no aplica a ICON");

  // Cambio de página y vuelta: se redibuja el último estado.
  JWPLCUI::setPage(1);
  JWPLCUI::setPage(0);
  tft.fillScreen(0x0000);
  JWPLCUI::drawStatic(tft);
  JWPLCUI::drawDirty(tft);
  CHECK(iconMatches(tft, SMALL, 1, 13, 13, 0x0841), "al volver a la página conserva el estado");

  savePpm(tft, argc > 1 ? argv[1] : "icon_sim.ppm");
  printf(failures ? "ICON host: %d FALLAS\n" : "ICON host: PASS\n", failures);
  return failures ? 1 : 0;
}
