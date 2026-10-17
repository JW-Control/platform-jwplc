// Simulación en host del MENU de JWPLC_Display (A15-8).
//
// Compila el JWPLC_UI_Menu.cpp REAL del package contra stubs (botonera, páginas,
// TFT en framebuffer) y recorre un guion de teclas verificando estado, escrituras
// y repintado parcial. Escribe capturas PPM del framebuffer en argv[1].
#include <Arduino.h>
#include <JWPLC_Display_API.h>
#include <JWPLC_UI_Pages.h>
#include <JWPLC_GlobalPeripherals.h>

#include "font_data.h"

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Stubs de las dependencias del motor
// ---------------------------------------------------------------------------
JW_MatrixButtons JWPLC_Buttons;
JWPLC_DisplayClass *g_displayUnused = nullptr;

// Ítem "Ir al inicio" (JWPLC_UI_MENU_GO_IDLE): cuenta las llamadas a goIdle().
JWPLC_DisplayClass JWPLC_Display;
static int g_goIdleCalls = 0;
void JWPLC_DisplayClass::goIdle() { g_goIdleCalls++; }

namespace JWPLCButtons
{
    bool isReady() { return true; }
    void clearPendingInput() { JWPLC_Buttons.clearPendingInput(); }
}

static uint8_t g_page = 0;
static bool g_selection = false;
static int g_refreshRequests = 0;
static int g_invalidations = 0;

namespace JWPLCUI
{
    uint8_t currentPage() { return g_page; }
    void requestRefresh() { g_refreshRequests++; }
    void invalidateAll(bool) { g_invalidations++; }
}

namespace JWPLCUIPages
{
    bool selectionMode() { return g_selection; }
}

bool JWPLC_TFTClass::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t)
{
    fillRects++;
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            pixel(i, j, c);
    return true;
}

bool JWPLC_TFTClass::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint32_t)
{
    for (int i = x; i < x + w; ++i) { pixel(i, y, c); pixel(i, y + h - 1, c); }
    for (int j = y; j < y + h; ++j) { pixel(x, j, c); pixel(x + w - 1, j, c); }
    return true;
}

size_t JWPLC_TFTClass::write(uint8_t ch)
{
    const uint8_t *glyph = jwplcSimGlyph(ch);
    for (int col = 0; col < 6; ++col)
    {
        const uint8_t bits = (col < 5) ? glyph[col] : 0;
        for (int row = 0; row < 8; ++row)
        {
            const bool on = (bits >> row) & 1;
            if (!on && fg == bg)
                continue;
            const uint16_t c = on ? fg : bg;
            for (int dy = 0; dy < size; ++dy)
                for (int dx = 0; dx < size; ++dx)
                    pixel(cx + col * size + dx, cy + row * size + dy, c);
        }
    }
    cx += 6 * size;
    return 1;
}

extern "C" void jwplcUIMenuServiceInput(void);
extern "C" void jwplcUIMenuPrepareEnter(void);
extern "C" void jwplcUIMenuDrawStatic(JWPLC_TFTClass *tft);
extern "C" void jwplcUIMenuDrawDirty(JWPLC_TFTClass *tft);

// ---------------------------------------------------------------------------
// Mismo menú que gates/A15_8_Menu
// ---------------------------------------------------------------------------
float temperatura = 21.5f;
float consigna = 45.0f;
bool bomba = false;
int32_t ciclos = 0;
int32_t retardo = 30;
float alarmaAlta = 80.0f;
bool alarmaHabilitada = true;

enum : uint8_t { N_TEMP = 0, N_CONSIGNA, N_BOMBA, N_AVANZADO, N_CICLOS, N_RETARDO, N_ALARMAS, N_ALTA, N_HABILITADA };

static const JWPLC_UIMenuNode nodes[] = {
    JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Temperatura", &temperatura, 1, "C"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna", &consigna, 0.0f, 100.0f, 0.5f, 1, "C"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Bomba", &bomba, "PARADA", "MARCHA"),
    JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "Avanzado"),
    JWPLC_UIMenuView(N_AVANZADO, "Ciclos", &ciclos),
    JWPLC_UIMenuEditNum(N_AVANZADO, "Retardo", &retardo, 0, 600, 5, "s"),
    JWPLC_UIMenuSubmenu(N_AVANZADO, "Alarmas"),
    JWPLC_UIMenuEditNum(N_ALARMAS, "Alta", &alarmaAlta, 0.0f, 150.0f, 1.0f, 0, "C"),
    JWPLC_UIMenuEditBool(N_ALARMAS, "Habilitada", &alarmaHabilitada, "NO", "SI")};

static JWPLC_TFTClass tft;
static int failures = 0;
static std::string outDir = ".";

static void check(bool cond, const char *what)
{
    printf("  %s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        failures++;
}

static void savePpm(const char *name)
{
    const std::string path = outDir + "/" + name + ".ppm";
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", JWPLC_TFTClass::W, JWPLC_TFTClass::H);
    for (int i = 0; i < JWPLC_TFTClass::W * JWPLC_TFTClass::H; ++i)
    {
        const uint16_t c = tft.fb[i];
        const uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
                                (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                                (uint8_t)((c & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);

    // RGB565 crudo (little-endian) para comparar con el lienzo del Designer.
    const std::string raw = outDir + "/" + name + ".rgb565";
    FILE *r = fopen(raw.c_str(), "wb");
    if (!r)
        return;
    fwrite(tft.fb, sizeof(uint16_t), JWPLC_TFTClass::W * JWPLC_TFTClass::H, r);
    fclose(r);
}

// Un tick del Display: entrada y luego repintado parcial (bus tomado).
static int tick()
{
    tft.fillRects = 0;
    jwplcUIMenuServiceInput();
    jwplcUIMenuDrawDirty(&tft);
    return tft.fillRects;
}

static int key(uint8_t id, int times = 1)
{
    for (int i = 0; i < times; ++i)
        JWPLC_Buttons.press(id);
    return tick();
}

// Color del primer pixel interior de la fila r (fondo de la fila).
static uint16_t rowBg(int r)
{
    const int x = 10 + 1 + 1;
    const int y = 20 + 1 + 12 + r * 12 + 1;
    return tft.fb[y * JWPLC_TFTClass::W + x];
}

int main(int argc, char **argv)
{
    if (argc > 1)
        outDir = argv[1];

    const JWPLC_UIMenu menus[] = {JWPLC_UIMenuField(0, 10, 20, 300, 140, "Ajustes", nodes, sizeof(nodes) / sizeof(nodes[0]), 0)};
    const JWPLC_UIMenuColors def;

    printf("REGISTRO\n");
    check(JWPLCUI::setMenus(menus, 1), "setMenus acepta el menú del gate");
    check(JWPLCUI::menuCount() == 1, "menuCount == 1");

    {
        JWPLC_UIMenuNode deep[] = {
            JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "A"),
            JWPLC_UIMenuSubmenu(0, "B"),
            JWPLC_UIMenuSubmenu(1, "C"),
            JWPLC_UIMenuView(2, "D", &temperatura)};
        JWPLC_UIMenu bad = JWPLC_UIMenuField(1, 0, 0, 100, 100, "X", deep, 4);
        check(!JWPLCUI::setMenus(&bad, 1), "rechaza profundidad 4");
        JWPLC_UIMenuNode fwd[] = {JWPLC_UIMenuView(1, "D", &temperatura), JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "A")};
        bad = JWPLC_UIMenuField(1, 0, 0, 100, 100, "X", fwd, 2);
        check(!JWPLCUI::setMenus(&bad, 1), "rechaza padre declarado después");
        JWPLC_UIMenuNode nul[] = {JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "N", (float *)nullptr, 0, 1)};
        bad = JWPLC_UIMenuField(1, 0, 0, 100, 100, "X", nul, 1);
        check(!JWPLCUI::setMenus(&bad, 1), "rechaza variable nula");
        check(JWPLCUI::setMenus(menus, 1), "el registro válido sigue funcionando");
    }

    printf("DIBUJO INICIAL\n");
    jwplcUIMenuPrepareEnter();
    jwplcUIMenuServiceInput();
    jwplcUIMenuDrawStatic(&tft);
    savePpm("menu_root");
    check(rowBg(0) == def.selectBackground, "fila 0 seleccionada (fondo de selección)");
    check(rowBg(1) == def.background, "fila 1 sin seleccionar");
    check(tick() == 0, "sin cambios no repinta nada");

    printf("NAVEGACIÓN\n");
    int fills = key(BTN_DOWN);
    check(rowBg(0) == def.background && rowBg(1) == def.selectBackground, "DOWN mueve la selección a la fila 1");
    check(fills == 2, "DOWN repinta sólo 2 filas");
    key(BTN_UP);
    check(rowBg(0) == def.selectBackground, "UP vuelve a la fila 0");
    key(BTN_UP);
    check(rowBg(3) == def.selectBackground, "UP en la primera fila da la vuelta a la última");
    key(BTN_DOWN);

    printf("EDICIÓN NUMÉRICA\n");
    key(BTN_DOWN);
    key(BTN_OK);
    check(JWPLCUI::menuEditing(), "OK sobre EDIT_NUM entra en edición");
    check(rowBg(1) == def.editBackground, "fila en edición con color de edición");
    key(BTN_UP, 3);
    savePpm("menu_editing");
    check(consigna == 45.0f, "la variable no cambia mientras se edita");
    key(BTN_ESC);
    check(!JWPLCUI::menuEditing() && consigna == 45.0f, "ESC cancela sin escribir");
    check(JWPLC_Buttons.pend[BTN_ESC] == 0, "ESC de edición se consume");
    key(BTN_OK);
    key(BTN_UP);
    key(BTN_RIGHT);
    key(BTN_OK);
    check(consigna == 45.0f + 0.5f + 10 * 0.5f, "UP (+1 paso) y RIGHT (+10 pasos) escriben al confirmar");
    uint8_t changedNode = 0xFF;
    check(JWPLCUI::menuChanged(0, &changedNode) && changedNode == N_CONSIGNA, "menuChanged informa el nodo editado");
    check(!JWPLCUI::menuChanged(0, &changedNode), "menuChanged se consume");
    key(BTN_OK);
    key(BTN_DOWN, 200);
    key(BTN_OK);
    check(consigna == 0.0f, "el límite mínimo se respeta");
    key(BTN_OK);
    key(BTN_UP, 200);
    key(BTN_OK);
    check(consigna == 100.0f, "el límite máximo se respeta");

    printf("EDICIÓN BOOL\n");
    key(BTN_DOWN);
    key(BTN_OK);
    key(BTN_UP);
    check(!bomba, "bool no cambia antes de confirmar");
    key(BTN_OK);
    check(bomba, "OK confirma MARCHA");

    printf("SUBMENÚS\n");
    key(BTN_DOWN);
    key(BTN_OK);
    savePpm("menu_level2");
    check(rowBg(0) == def.selectBackground, "nivel 2 abre con la primera fila seleccionada");
    key(BTN_DOWN);
    key(BTN_OK);
    key(BTN_LEFT, 3);
    key(BTN_OK);
    check(retardo == 0, "int32: LEFT resta 10 pasos y satura en el mínimo");
    key(BTN_OK);
    key(BTN_RIGHT, 2);
    key(BTN_UP);
    key(BTN_OK);
    check(retardo == 105, "int32: 21 pasos de 5 s = 105");
    key(BTN_DOWN);
    key(BTN_RIGHT);
    savePpm("menu_level3");
    key(BTN_DOWN);
    key(BTN_OK);
    key(BTN_DOWN);
    key(BTN_OK);
    check(!alarmaHabilitada, "nivel 3: EDIT_BOOL escribe");
    key(BTN_ESC);
    key(BTN_LEFT);
    check(rowBg(3) == def.selectBackground, "ESC y LEFT suben a la raíz con \"Avanzado\" seleccionado");
    check(JWPLC_Buttons.pend[BTN_ESC] == 0, "ESC fuera de la raíz se consume");
    key(BTN_ESC);
    check(JWPLC_Buttons.pend[BTN_ESC] == 1, "ESC en la raíz queda para el sistema (PAGE_SELECT)");
    JWPLC_Buttons.clearPendingInput();

    printf("VALORES EN VIVO\n");
    key(BTN_UP, 10);
    temperatura = 30.25f;
    fills = tick();
    check(fills >= 1, "un VIEW que cambia se repinta solo");
    check(tick() == 0, "y deja de repintar cuando no cambia");

    printf("PAGE_SELECT\n");
    g_selection = true;
    fills = tick();
    savePpm("menu_unfocused");
    check(fills == 1, "al perder el foco repinta sólo la fila seleccionada");
    JWPLC_Buttons.press(BTN_DOWN);
    tick();
    check(JWPLC_Buttons.pend[BTN_DOWN] == 1, "en PAGE_SELECT el menú no consume teclas");
    JWPLC_Buttons.clearPendingInput();
    g_selection = false;
    tick();

    printf("OTRA PÁGINA\n");
    key(BTN_DOWN);
    key(BTN_OK);
    check(JWPLCUI::menuEditing(), "edición abierta");
    g_page = 1;
    tick();
    check(!JWPLCUI::menuEditing(), "cambiar de página cancela la edición");
    JWPLC_Buttons.press(BTN_OK);
    tick();
    check(JWPLC_Buttons.pend[BTN_OK] == 1, "en una página sin menú las teclas quedan para la aplicación");

    printf("IR AL INICIO\n");
    {
        static const JWPLC_UIMenuNode exitNodes[] = {
            JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Temperatura", &temperatura, 1, "C"),
            JWPLC_UIMenuGoIdle(JWPLC_UI_MENU_ROOT, "Ir al inicio")};
        const JWPLC_UIMenu exitMenu[] = {JWPLC_UIMenuField(0, 10, 20, 300, 140, "Salir", exitNodes, 2, 0)};
        check(JWPLCUI::setMenus(exitMenu, 1), "acepta un ítem GO_IDLE sin variable");
        g_page = 0;
        jwplcUIMenuPrepareEnter();
        jwplcUIMenuDrawStatic(&tft);
        tick();
        key(BTN_DOWN);
        savePpm("menu_go_idle");
        const int before = g_goIdleCalls;
        key(BTN_RIGHT);
        check(g_goIdleCalls == before, "RIGHT sobre GO_IDLE no sale (sólo OK)");
        key(BTN_OK);
        check(g_goIdleCalls == before + 1, "OK sobre GO_IDLE llama a JWPLC_Display.goIdle()");
        check(JWPLC_Buttons.pend[BTN_OK] == 0, "el OK de la acción se consume");
        temperatura = 12.0f;
        check(tick() >= 0, "GO_IDLE no se lee como variable al refrescar");
    }

    printf("\n%s (%d fallas)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
