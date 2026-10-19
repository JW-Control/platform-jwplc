// Simulación en host del despertar desde IDLE con páginas y MENU (Alpha15).
//
// Compila los JWPLC_UI_Pages.cpp y JWPLC_UI_Menu.cpp REALES del package contra
// stubs y reproduce la secuencia de JWPLC_Display.cpp:
//   handleIdleWakeAndTimeout() -> enterUserUI() -> jwplcUIRuntimePrepareEnter()
//   y, en el mismo callback, jwplcUIRuntimeServiceInput() (MENU y luego páginas).
// La tecla que despierta la IDLE no debe actuar además sobre el selector de
// páginas ni sobre el MENU.
#include <Arduino.h>
#include <JWPLC_Display_API.h>
#include <JWPLC_UI_Pages.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display_H3E1_Profile.h>

#include <cstdio>

JW_MatrixButtons JWPLC_Buttons;
JWPLC_DisplayClass JWPLC_Display;

static JWPLC_DisplayIdleReturnMode g_idleReturn = IDLE_RETURN_ESC_ONLY;
void JWPLC_DisplayClass::goIdle() {}
JWPLC_DisplayIdleReturnMode JWPLC_DisplayClass::idleReturnMode() const { return g_idleReturn; }
void JWPLC_DisplayClass::setIdleReturnMode(JWPLC_DisplayIdleReturnMode mode) { g_idleReturn = mode; }

bool jwplcH3E1ProfilerEnabled(void) { return false; }
void jwplcH3E1RecordStage(JWPLCH3E1StageId, uint32_t) {}

namespace JWPLCButtons
{
    bool isReady() { return true; }
    void clearPendingInput() { JWPLC_Buttons.clearPendingInput(); }
}

static uint8_t g_page = 0;
namespace JWPLCUI
{
    uint8_t currentPage() { return g_page; }
    void setPage(uint8_t page) { g_page = page; }
    void requestRefresh() {}
    void invalidateAll(bool) {}
}

// Hook de la barra de navegación (strong, como el que emite el Designer).
static int g_modeCalls = 0;
static uint8_t g_modePage = 0xFF;
static bool g_modeSelecting = false;
extern "C" void jwplcUIPageModeChanged(uint8_t page, bool selecting)
{
    g_modeCalls++;
    g_modePage = page;
    g_modeSelecting = selecting;
}

bool JWPLC_TFTClass::fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t, uint32_t) { return true; }
bool JWPLC_TFTClass::drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t, uint32_t) { return true; }
size_t JWPLC_TFTClass::write(uint8_t) { return 1; }

extern "C" void jwplcUIMenuServiceInput(void);
extern "C" void jwplcUIMenuPrepareEnter(void);

// Lo mismo que jwplcUIRuntimePrepareEnter / jwplcUIRuntimeServiceInput
// (JWPLC_UI_API.cpp), sin el resto del motor.
static void prepareEnter()
{
    jwplcUIMenuPrepareEnter();
    JWPLCUIPages::prepareEnter();
}
static void serviceInput()
{
    jwplcUIMenuServiceInput();
    JWPLCUIPages::serviceNavigation();
}

// Despertar con una tecla: el latch de la pulsación ya existe cuando el
// Display entra a USER y, en el mismo callback, atiende la entrada.
static void wakeWith(uint8_t key)
{
    JWPLC_Buttons.clearPendingInput();
    JWPLC_Buttons.press(key);
    prepareEnter();
    serviceInput();
}
static void keyPress(uint8_t key)
{
    JWPLC_Buttons.press(key);
    serviceInput();
}

static int failures = 0;
static void check(bool cond, const char *what)
{
    printf("  %s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        failures++;
}

float consigna = 45.0f;
bool bomba = false;
static const JWPLC_UIMenuNode nodes[] = {
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna", &consigna, 0.0f, 100.0f, 0.5f, 1, "C"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Bomba", &bomba, "PARADA", "MARCHA")};

int main()
{
    // 3 páginas; el MENU vive en la página 0 (el caso de la placa del jefe).
    const JWPLC_UIMenu menus[] = {JWPLC_UIMenuField(0, 10, 20, 300, 140, "Ajustes", nodes, 2, 0)};
    JWPLCUI::setMenus(menus, 1);
    JWPLCUIPages::setPageCount(3);

    printf("DESPERTAR CON VARIAS PÁGINAS\n");
    g_page = 0;
    wakeWith(BTN_OK);
    check(JWPLCUIPages::selectionMode(), "OK que despierta deja el selector de páginas (no entra al contenido)");
    check(!JWPLCUI::menuEditing(), "OK que despierta no llega al MENU");
    keyPress(BTN_RIGHT);
    check(g_page == 1, "después, RIGHT cambia de página en el selector");
    keyPress(BTN_LEFT);
    check(g_page == 0, "LEFT vuelve a la página 1");
    keyPress(BTN_OK);
    check(!JWPLCUIPages::selectionMode(), "OK (ya despierto) entra al contenido");
    keyPress(BTN_ESC);
    check(JWPLCUIPages::selectionMode(), "ESC en la raíz del MENU vuelve al selector");
    keyPress(BTN_RIGHT);
    check(g_page == 1, "y RIGHT vuelve a cambiar de página");

    g_page = 0;
    wakeWith(BTN_RIGHT);
    check(g_page == 0 && JWPLCUIPages::selectionMode(), "RIGHT que despierta no cambia además de página");

    printf("BARRA: CAMBIO DE MODO SIN CAMBIAR DE PÁGINA\n");
    {
        JWPLC_TFTClass tft;
        g_page = 0;
        wakeWith(BTN_OK);
        g_modeCalls = 0;
        JWPLCUIPages::drawModeChange(tft);
        check(g_modeCalls == 0, "al despertar no se llama (la entrada dibuja la página completa)");
        keyPress(BTN_RIGHT);
        JWPLCUIPages::drawModeChange(tft);
        check(g_modeCalls == 0, "cambiar de página en el selector no lo llama (redibujo completo)");
        keyPress(BTN_OK);
        JWPLCUIPages::drawModeChange(tft);
        check(g_modeCalls == 1 && g_modePage == 1 && !g_modeSelecting, "OK: una llamada con la página 2 y selecting=false (seleccionada)");
        JWPLCUIPages::drawModeChange(tft);
        check(g_modeCalls == 1, "sin otro cambio no se vuelve a llamar");
        keyPress(BTN_ESC);
        JWPLCUIPages::drawModeChange(tft);
        check(g_modeCalls == 2 && g_modePage == 1 && g_modeSelecting, "ESC: otra llamada con selecting=true (posicionada)");
    }

    printf("DESPERTAR CON UNA PÁGINA Y MENU\n");
    JWPLCUIPages::setPageCount(1);
    g_page = 0;
    wakeWith(BTN_OK);
    check(!JWPLCUI::menuEditing(), "OK que despierta no abre la edición del MENU");
    keyPress(BTN_OK);
    check(JWPLCUI::menuEditing(), "el OK siguiente sí edita (el MENU tiene el foco)");

    printf(failures ? "\n%d verificación(es) fallaron.\n" : "\nTodas las verificaciones pasaron.\n", failures);
    return failures ? 1 : 0;
}
