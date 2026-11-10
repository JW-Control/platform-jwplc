// Simulación en host del BAR de JWPLC_Display: compila el JWPLC_UI.cpp real
// contra stubs (icon-host/stub) y verifica grosor, largo, borde, esquinas,
// bloques y degradado píxel a píxel. Guarda bar_sim.ppm.
#include "sim_stubs.h"

enum : uint8_t { ID_DEFAULT = 1, ID_LEGACY, ID_THICK, ID_BORDER, ID_ROUND, ID_BLOCKS, ID_VERT, ID_GRAD, ID_VERT_STACKED };

static const uint16_t FILL = 0xF800, BG = 0x0841, BORDER = 0x07E0;

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) printf("  OK  " __VA_ARGS__); else { printf("  FAIL " __VA_ARGS__); ++failures; } printf("\n"); } while (0)

static JWPLC_TFTClass tft;

static uint16_t at(int x, int y) { return tft.fb[y * JWPLC_TFTClass::W + x]; }

static void refresh()
{
  for (int i = 0; i < 16; ++i) JWPLCUI::drawDirty(tft);
}

static JWPLC_UIField bar(uint8_t id, JWPLC_UIRect rect, JWPLC_UIBarConfig config = JWPLC_UIBarConfig())
{
  return JWPLC_UIBarField(id, rect, JWPLC_UIText(), JWPLC_UIRange(0.0f, 100.0f),
                          JWPLC_UIBarStyle(1, false, JWPLC_UI_LAYOUT_INLINE), 0,
                          JWPLC_UIColors(0xFFFF, FILL, BG, 0xFFFF), config);
}

int main(int argc, char **argv)
{
  // Sin etiqueta la barra empieza en x + 3, y + 3 y el campo de 110 px deja 104 px.
  const JWPLC_UIField fields[] = {
    bar(ID_DEFAULT, JWPLC_UIRect(10, 5, 110, JWPLC_UI_AUTO)),
    bar(ID_LEGACY, JWPLC_UIRect(10, 25, 110, 28)),
    bar(ID_THICK, JWPLC_UIRect(10, 60, 110, JWPLC_UI_AUTO), JWPLC_UIBarConfig(0, 0, 0, 0, false, 0xFFFF, 1, 30, 0)),
    bar(ID_BORDER, JWPLC_UIRect(140, 5, 110, JWPLC_UI_AUTO), JWPLC_UIBarConfig(0, 0, 0, 0, true, BORDER, 5, 20, 0)),
    bar(ID_ROUND, JWPLC_UIRect(140, 40, 110, JWPLC_UI_AUTO), JWPLC_UIBarConfig(0, 0, 0, 6)),
    bar(ID_BLOCKS, JWPLC_UIRect(140, 60, 110, JWPLC_UI_AUTO), JWPLC_UIBarConfig(0, JWPLC_UI_BAR_STYLE_BLOCKS)),
    bar(ID_VERT, JWPLC_UIRect(270, 5), JWPLC_UIBarConfig(JWPLC_UI_BAR_VERTICAL, 0, 0, 0, false, 0xFFFF, 1, 14, 60)),
    JWPLC_UIBarField(ID_VERT_STACKED, JWPLC_UIRect(220, 5), JWPLC_UIText("Nivel", "%"),
                     JWPLC_UIRange(0.0f, 100.0f), JWPLC_UIBarStyle(1, false, JWPLC_UI_LAYOUT_STACKED),
                     0, JWPLC_UIColors(0xFFFF, FILL, BG, 0xFFFF),
                     JWPLC_UIBarConfig(JWPLC_UI_BAR_VERTICAL, 0, 0, 0, false, 0xFFFF, 1, 14, 60)),
    bar(ID_GRAD, JWPLC_UIRect(10, 110, 110, JWPLC_UI_AUTO), JWPLC_UIBarConfig(0, 0, 0x0001)),
  };

  printf("BAR en host (JWPLC_UI.cpp real)\n");
  CHECK(JWPLCUI::setFields(fields, sizeof(fields) / sizeof(fields[0])), "setFields acepta los BAR");
  tft.fillScreen(0x0000);
  JWPLCUI::drawStatic(tft);
  refresh();

  JWPLCUI::setBar(ID_DEFAULT, 100.0f);
  JWPLCUI::setBar(ID_LEGACY, 100.0f);
  JWPLCUI::setBar(ID_THICK, 100.0f);
  JWPLCUI::setBar(ID_BORDER, 50.0f);
  JWPLCUI::setBar(ID_ROUND, 100.0f);
  JWPLCUI::setBar(ID_BLOCKS, 50.0f);
  JWPLCUI::setBar(ID_VERT, 50.0f);
  JWPLCUI::setBar(ID_VERT_STACKED, 50.0f);
  JWPLCUI::setBar(ID_GRAD, 100.0f);
  refresh();

  // Grosor
  CHECK(at(20, 8 + 11) == FILL && at(20, 8 + 12) != FILL, "sin config la barra mide 12 px");
  CHECK(at(20, 28 + 11) == FILL && at(20, 28 + 12) != FILL, "un rect con alto (API simple) sigue midiendo 12 px");
  CHECK(at(20, 63 + 29) == FILL && at(20, 63 + 30) != FILL, "thickness = 30 mide 30 px");

  // Borde: el nivel se reparte en el interior (94 px): 50 % = 47 px.
  const int bx = 143 + 5, by = 8 + 10;
  CHECK(at(143, by) == BORDER && at(143 + 4, by) == BORDER, "borde de 5 px intacto");
  CHECK(at(bx, by) == FILL && at(bx + 46, by) == FILL, "relleno hasta 47 px dentro del borde");
  CHECK(at(bx + 47, by) == BG, "el 50 %% no pasa de la mitad del interior");
  JWPLCUI::setBar(ID_BORDER, 90.0f);
  refresh();
  CHECK(at(bx + 84, by) == FILL && at(bx + 85, by) == BG, "al 90 %% la barra aún no está llena (85 de 94 px)");

  // Esquinas: el relleno no pisa la esquina y sí el centro.
  const int rx = 143, ry = 43;
  CHECK(at(rx, ry) == BG && at(rx + 103, ry) == BG && at(rx, ry + 11) == BG, "esquinas sin relleno");
  CHECK(at(rx + 50, ry) == FILL && at(rx, ry + 6) == FILL && at(rx + 103, ry + 6) == FILL, "relleno hasta el borde fuera de las esquinas");

  // Bloques: 21 de 4 px con paso 5; al 50 % se encienden 11.
  const int kx = 143, ky = 63 + 5;
  CHECK(at(kx, ky) == FILL && at(kx + 3, ky) == FILL && at(kx + 4, ky) != FILL, "bloque de 4 px y separación de 1 px");
  CHECK(at(kx + 50, ky) == FILL && at(kx + 55, ky) != FILL, "11 bloques al 50 %%");

  // Vertical: 14 x 60, se llena desde abajo.
  const int vx = 273, vy = 8;
  CHECK(at(vx, vy + 59) == FILL && at(vx + 13, vy + 59) == FILL && at(vx + 14, vy + 59) != FILL, "ancho de 14 px exacto");
  CHECK(at(vx, vy + 30) == FILL && at(vx, vy + 29) != FILL, "al 50 %% llena 30 de 60 px desde abajo");

  // Vertical STACKED con etiqueta ("Nivel", 30 px) y unidad ("%", 6 px):
  // contentW = 30; autoW = 3 + 3 + 30 = 36;
  // valueX = 220 + 3 + (30 - 14)/2 = 231;
  // valueY = 5 + 3 + 8 + 2 = 18;
  const int vsx = 231, vsy = 18;
  CHECK(at(vsx, vsy + 59) == FILL && at(vsx + 13, vsy + 59) == FILL && at(vsx + 14, vsy + 59) != FILL, "vertical STACKED: barra centrada en x=231");
  CHECK(at(vsx, vsy + 30) == FILL && at(vsx, vsy + 29) != FILL, "vertical STACKED: al 50 %% llena 30 de 60 px");

  // Degradado hasta casi negro: 0x0001 no significa sólido.
  CHECK(at(13, 118) == FILL && at(13 + 103, 118) != FILL && (at(13 + 103, 118) >> 11) <= 1, "degradado termina en negro");
  JWPLCUI::setBar(ID_GRAD, 0.0f);
  refresh();
  CHECK(at(13, 118) == BG && at(13 + 103, 118) == BG, "al bajar a 0 limpia con el fondo");

  savePpm(tft, argc > 1 ? argv[1] : "bar_sim.ppm");
  printf(failures ? "BAR host: %d FALLAS\n" : "BAR host: PASS\n", failures);
  return failures ? 1 : 0;
}
