// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_RECETA = 0,
    PAGE_REFLEJOS = 1,
    PAGE_MOTOR = 2,
    PAGE_AJUSTES = 3
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Receta
    FIELD_R_NUM = 1,
    FIELD_RECETA = 2,
    FIELD_R_TEMP = 3,
    FIELD_R_TIEMPO = 4,
    FIELD_R_CARGADA = 5,

    // Página 02 · Reflejos
    FIELD_LUZ = 6,
    FIELD_ESTADO_JUEGO = 7,
    FIELD_REACCION = 8,
    FIELD_RECORD = 9,

    // Página 03 · Motor
    FIELD_MARCHA = 10,
    FIELD_CONSIGNA = 11,
    FIELD_PASO = 12,
    FIELD_BAR_CONSIGNA = 13,
    FIELD_BAR_VELOCIDAD = 14,
};

// Variables HMI
// Página 01 · Receta
float recetaNum = 0.0f;
char receta[29] = {}; // 14 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float recetaTemp = 0.0f;
uint32_t recetaTiempo = 0;
bool recetaCargada = false;

// Página 02 · Reflejos
bool luz = false;
char estadoJuego[25] = {}; // 12 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float tiempoReaccion = 0.0f;
float record = 0.0f;

// Página 03 · Motor
bool marcha = false;
float consigna = 0.0f;
char paso[9] = {}; // 4 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float barConsigna = 0.0f;
float velocidad = 0.0f;

// MENU Ajustes
float rampa = 500.0f;
int32_t esperaMaxMs = 4000;
int32_t partidas = 0;
bool borrarRecord = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_R_NUM,
        JWPLC_UIRect(262, 2),
        JWPLC_UIText("N.", nullptr),
        JWPLC_UIValueFormat(
            1,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x780F,
            0xFFFF)),
    JWPLC_UITextField(
        FIELD_RECETA,
        JWPLC_UIRect(22, 26),
        JWPLC_UIText(nullptr, nullptr, 14),
        JWPLC_UITextFieldStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xFFE0,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_R_TEMP,
        JWPLC_UIRect(4, 56),
        JWPLC_UIText("Horno", "\367C"),
        JWPLC_UIValueFormat(
            3,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_R_TIEMPO,
        JWPLC_UIRect(4, 82),
        JWPLC_UIText("Tiempo", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_MM_SS,
            JWPLC_UI_DURATION_SECONDS,
            2,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_R_CARGADA,
        JWPLC_UIRect(200, 60),
        JWPLC_UIText("Horneando", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x7BEF,
            0x07E0),
#else
        JWPLC_UIBoolText(
            "OFF",
            "ON"),
#endif
        JWPLC_UIBoolStyle(
            3,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_LUZ,
        JWPLC_UIRect(250, 6),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0xF800,
            0x07E0),
#else
        JWPLC_UIBoolText(
            "OFF",
            "ON"),
#endif
        JWPLC_UIBoolStyle(
            4,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UITextField(
        FIELD_ESTADO_JUEGO,
        JWPLC_UIRect(4, 40),
        JWPLC_UIText(nullptr, nullptr, 12),
        JWPLC_UITextFieldStyle(
            3,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_REACCION,
        JWPLC_UIRect(4, 80),
        JWPLC_UIText("Tiempo", "ms"),
        JWPLC_UIValueFormat(
            4,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_RECORD,
        JWPLC_UIRect(4, 106),
        JWPLC_UIText("Record", "ms"),
        JWPLC_UIValueFormat(
            4,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFEA0,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_MARCHA,
        JWPLC_UIRect(226, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "PARADO",
            "MARCHA",
            0xFFFF,
            0x0000,
            0x7800,
            0x07E0),
#else
        JWPLC_UIBoolText(
            "PARADO",
            "MARCHA"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_CONSIGNA,
        JWPLC_UIRect(4, 24),
        JWPLC_UIText("Consigna", "rpm"),
        JWPLC_UIValueFormat(
            4,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UITextField(
        FIELD_PASO,
        JWPLC_UIRect(226, 26),
        JWPLC_UIText("Paso", nullptr, 4),
        JWPLC_UITextFieldStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0xFD20,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_BAR_CONSIGNA,
        JWPLC_UIRect(4, 50, 312, JWPLC_UI_AUTO),
        JWPLC_UIText("Consigna", "%"),
        JWPLC_UIRange(0.0f, 3000.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x10C5,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_BAR_VELOCIDAD,
        JWPLC_UIRect(4, 84, 312, JWPLC_UI_AUTO),
        JWPLC_UIText("Velocidad", "%"),
        JWPLC_UIRange(0.0f, 3000.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x10C5,
            0xFFFF))
};

// MENU (A15-8): requiere JWPLC_Display con soporte de MENU.
#ifndef JWPLC_UI_HAS_MENU
#error "El MENU requiere JWPLC_Display con soporte de MENU (package JWPLC Alpha15 o posterior)."
#endif

enum HMIMenuId : uint8_t
{
    MENU_AJUSTES = 0,
};

// Nodos de MENU_AJUSTES: el índice es el que informa JWPLC_Display.menuChanged().
enum : uint8_t
{
    MENU_AJUSTES_RAMPA_MOTOR = 0,
    MENU_AJUSTES_ESPERA_MAX_JUEGO = 1,
    MENU_AJUSTES_PARTIDAS = 2,
    MENU_AJUSTES_BORRAR_RECORD = 3,
    MENU_AJUSTES_SALIR_AL_INICIO = 4,
};

static const JWPLC_UIMenuNode HMI_MENU_AJUSTES_NODES[] =
{
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Rampa motor", &rampa, 50.0f, 3000.0f, 50.0f, 0, "rpm/s"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Espera max juego", &esperaMaxMs, 1500, 8000, 500, "ms"),
    JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Partidas", &partidas, nullptr),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Borrar record", &borrarRecord, "NO", "SI"),
    JWPLC_UIMenuGoIdle(JWPLC_UI_MENU_ROOT, "Salir al inicio")
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuField(
        MENU_AJUSTES,
        4, 2, 312, 140,
        "Ajustes",
        HMI_MENU_AJUSTES_NODES,
        sizeof(HMI_MENU_AJUSTES_NODES) / sizeof(HMI_MENU_AJUSTES_NODES[0]),
        PAGE_AJUSTES,
        1,
        true,
        JWPLC_UIMenuColors(
            0xFFFF,
            0x0000,
            0x07FF,
            0x0000,
            0x07FF,
            0x0000,
            0xFD20,
            0x0000,
            0xFFFF,
            0xFFFF))
};

enum HMIPixelMapId : uint8_t
{
    PIXEL_TROFEO = 0
};

// PixelMaps estáticos RGB565 · JWPLC HMI Designer
// Formato: PACKED_SPAN16 · 50 px -> 14 spans · 1 entradas de paleta
// Estimado: 78 B vs 124 B RGB565_RUN · ahorro 37%

static const uint16_t HMI_PM_1_PAL[] =
{
    0xFEA0
};

static const uint32_t HMI_PM_1_DATA[] =
{
    0x0010D06Eu, 0x0000D26Eu, 0x0008D270u, 0x0000D276u, 0x0000D46Eu, 0x0008D470u, 0x0000D476u, 0x000CD66Fu,
    0x0008D870u, 0x0004DA71u, 0x0000DC72u, 0x0000DE72u, 0x0008E070u, 0x0008E270u
};

static const JWPLC_UIPixelPackedMap HMI_PIXEL_MAPS[] =
{
    JWPLC_UIPixelPackedMap(1, HMI_PM_1_PAL, sizeof(HMI_PM_1_PAL) / sizeof(HMI_PM_1_PAL[0]), HMI_PM_1_DATA, sizeof(HMI_PM_1_DATA) / sizeof(HMI_PM_1_DATA[0]))
};

void jwplcHMISetup()
{
    JWPLC_Display.setMenus(
        HMI_MENUS,
        sizeof(HMI_MENUS) / sizeof(HMI_MENUS[0]));
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(4);
    JWPLC_Display.setUserPage(PAGE_RECETA);
    // Indicador NN/TT (barra de navegación)
#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG
    JWPLC_Display.setPageIndicator(false);
#endif
    JWPLC_Display.setPackedPixelMaps(HMI_PIXEL_MAPS, sizeof(HMI_PIXEL_MAPS) / sizeof(HMI_PIXEL_MAPS[0]));
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.enterUserUI();
}

// Primitivas auxiliares (sólo usan API pública de JWPLC_TFT).
#if defined(JWPLC_TFT_VERSION_MAJOR) && \
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 2))
#define JWPLC_HMI_TFT_NATIVE_SHAPES 1
#else
#define JWPLC_HMI_TFT_NATIVE_SHAPES 0
#endif
#if defined(JWPLC_TFT_VERSION_MAJOR) && \
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 3))
#define JWPLC_HMI_TFT_FONTS 1
#else
#define JWPLC_HMI_TFT_FONTS 0
#endif

static inline void jwplcHMISwap(int16_t &a, int16_t &b)
{
    const int16_t t = a;
    a = b;
    b = t;
}

static void jwplcHMIFillTriangle(JWPLC_TFTClass &tft,
    int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
#else
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }
    if (y1 > y2) { jwplcHMISwap(y2, y1); jwplcHMISwap(x2, x1); }
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }

    int16_t a;
    int16_t b;
    if (y0 == y2)
    {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        tft.drawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    const int32_t dx01 = x1 - x0, dy01 = y1 - y0;
    const int32_t dx02 = x2 - x0, dy02 = y2 - y0;
    const int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0;
    int32_t sb = 0;
    const int16_t last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);
    int16_t y = y0;

    for (; y <= last; y++)
    {
        a = (int16_t)(x0 + sa / dy01);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx01;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = (int16_t)(x1 + sa / dy12);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx12;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }
#endif
}

// Barra de Navegación HMI Autogenerada (ST7789)
// Pestaña de la página actual: posicionada en el selector (◀ ▶, fondo de
// color) o seleccionada dentro de la página (OK, fondo blanco y doble marco).
static void jwplcDrawPageNavTab(uint8_t activePage, bool selecting)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.setTextSize(1);

    switch (activePage)
    {
    case 0:
        if (selecting)
        {
            tft.fillRoundRect(5, 148, 75, 20, 3, 0xEBBE);
            tft.setCursor(25, 154);
            tft.setTextColor(0x0000);
            tft.print("RECETA");
        }
        else
        {
            tft.fillRoundRect(5, 148, 75, 20, 3, 0xFFFF);
            tft.drawRoundRect(5, 148, 75, 20, 3, 0x69AD);
            tft.drawRoundRect(7, 150, 71, 16, 1, 0x69AD);
            tft.setCursor(25, 154);
            tft.setTextColor(0x69AD);
            tft.print("RECETA");
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(83, 148, 75, 20, 3, 0xFF20);
            tft.setCursor(106, 154);
            tft.setTextColor(0x0000);
            tft.print("JUEGO");
        }
        else
        {
            tft.fillRoundRect(83, 148, 75, 20, 3, 0xFFFF);
            tft.drawRoundRect(83, 148, 75, 20, 3, 0x7340);
            tft.drawRoundRect(85, 150, 71, 16, 1, 0x7340);
            tft.setCursor(106, 154);
            tft.setTextColor(0x7340);
            tft.print("JUEGO");
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(161, 148, 75, 20, 3, 0x073F);
            tft.setCursor(184, 154);
            tft.setTextColor(0x0000);
            tft.print("MOTOR");
        }
        else
        {
            tft.fillRoundRect(161, 148, 75, 20, 3, 0xFFFF);
            tft.drawRoundRect(161, 148, 75, 20, 3, 0x032E);
            tft.drawRoundRect(163, 150, 71, 16, 1, 0x032E);
            tft.setCursor(184, 154);
            tft.setTextColor(0x032E);
            tft.print("MOTOR");
        }
        break;
    case 3:
        if (selecting)
        {
            tft.fillRoundRect(239, 148, 75, 20, 3, 0xFC80);
            tft.setCursor(256, 154);
            tft.setTextColor(0x0000);
            tft.print("AJUSTES");
        }
        else
        {
            tft.fillRoundRect(239, 148, 75, 20, 3, 0xFFFF);
            tft.drawRoundRect(239, 148, 75, 20, 3, 0x7200);
            tft.drawRoundRect(241, 150, 71, 16, 1, 0x7200);
            tft.setCursor(256, 154);
            tft.setTextColor(0x7200);
            tft.print("AJUSTES");
        }
        break;
    default:
        break;
    }
}

// Se dibuja una vez por página desde jwplcUIPageEnter().
static void jwplcDrawPageNavBar(uint8_t activePage)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.setTextSize(1);

    switch (activePage)
    {
    case 0:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(83, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(83, 148, 75, 20, 3, 0xFF20);
        tft.setCursor(106, 154);
        tft.setTextColor(0xFF20);
        tft.print("JUEGO");
        tft.fillRoundRect(161, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(161, 148, 75, 20, 3, 0x073F);
        tft.setCursor(184, 154);
        tft.setTextColor(0x073F);
        tft.print("MOTOR");
        tft.fillRoundRect(239, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(239, 148, 75, 20, 3, 0xFC80);
        tft.setCursor(256, 154);
        tft.setTextColor(0xFC80);
        tft.print("AJUSTES");
        break;
    case 1:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(5, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(5, 148, 75, 20, 3, 0xEBBE);
        tft.setCursor(25, 154);
        tft.setTextColor(0xEBBE);
        tft.print("RECETA");
        tft.fillRoundRect(161, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(161, 148, 75, 20, 3, 0x073F);
        tft.setCursor(184, 154);
        tft.setTextColor(0x073F);
        tft.print("MOTOR");
        tft.fillRoundRect(239, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(239, 148, 75, 20, 3, 0xFC80);
        tft.setCursor(256, 154);
        tft.setTextColor(0xFC80);
        tft.print("AJUSTES");
        break;
    case 2:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(5, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(5, 148, 75, 20, 3, 0xEBBE);
        tft.setCursor(25, 154);
        tft.setTextColor(0xEBBE);
        tft.print("RECETA");
        tft.fillRoundRect(83, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(83, 148, 75, 20, 3, 0xFF20);
        tft.setCursor(106, 154);
        tft.setTextColor(0xFF20);
        tft.print("JUEGO");
        tft.fillRoundRect(239, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(239, 148, 75, 20, 3, 0xFC80);
        tft.setCursor(256, 154);
        tft.setTextColor(0xFC80);
        tft.print("AJUSTES");
        break;
    case 3:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(5, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(5, 148, 75, 20, 3, 0xEBBE);
        tft.setCursor(25, 154);
        tft.setTextColor(0xEBBE);
        tft.print("RECETA");
        tft.fillRoundRect(83, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(83, 148, 75, 20, 3, 0xFF20);
        tft.setCursor(106, 154);
        tft.setTextColor(0xFF20);
        tft.print("JUEGO");
        tft.fillRoundRect(161, 148, 75, 20, 3, 0x0000);
        tft.drawRoundRect(161, 148, 75, 20, 3, 0x073F);
        tft.setCursor(184, 154);
        tft.setTextColor(0x073F);
        tft.print("MOTOR");
        break;
    default:
        break;
    }

    jwplcDrawPageNavTab(activePage, JWPLC_Display.isUserPageSelection());
}

// Selector <-> contenido sin cambiar de página: el firmware redibuja sólo la
// pestaña activa, con el bus tomado (Alpha15).
extern "C" void jwplcUIPageModeChanged(uint8_t page, bool selecting)
{
    jwplcDrawPageNavTab(page, selecting);
}

// Gráfica estática por página (se dibuja una vez al entrar a la página).
// Para lógica propia al cambiar de página, define en el sketch:
//   extern "C" void jwplcHMIUserPageEnter(uint8_t page) { ... }
extern "C" void jwplcHMIUserPageEnter(uint8_t page) __attribute__((weak));

void jwplcUIPageEnter(uint8_t page)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();

    switch (page)
    {
    case PAGE_RECETA:
        // FIELD_RECT_2
        tft.fillRect(0, 0, 319, 16, 0x780F);
        tft.drawRect(0, 0, 319, 16, 0x780F);
        // SELECTOR DE RECETA
        tft.setCursor(4, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("SELECTOR DE RECETA");
        // FIELD_TRIANGLE_6
        jwplcHMIFillTriangle(tft, 10, 24, 14, 32, 6, 32, 0xF81F);
        tft.drawLine(10, 24, 14, 32, 0xF81F);
        tft.drawLine(14, 32, 6, 32, 0xF81F);
        tft.drawLine(6, 32, 10, 24, 0xF81F);
        // FIELD_TRIANGLE_7
        jwplcHMIFillTriangle(tft, 10, 44, 6, 36, 14, 36, 0xF81F);
        tft.drawLine(10, 44, 6, 36, 0xF81F);
        tft.drawLine(6, 36, 14, 36, 0xF81F);
        tft.drawLine(14, 36, 10, 44, 0xF81F);
        // arriba/abajo receta   OK
        tft.setCursor(4, 126);
        tft.setTextSize(1);
        tft.setTextColor(0x7BEF);
        tft.print("arriba/abajo receta   OK hornear/parar");
        break;

    case PAGE_REFLEJOS:
        // REFLEJOS
        #if JWPLC_HMI_TFT_FONTS
        tft.setFont(JWPLC_FONT_SANS_BOLD_12);
        tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
        tft.setTextSize(1);
        tft.setTextColor(0xFFE0);
        tft.drawString("REFLEJOS", 4, 2);
        tft.setFont(JWPLC_FONT_GLCD);
        #else
        tft.setCursor(4, 2);
        tft.setTextSize(1);
        tft.setTextColor(0xFFE0);
        tft.print("REFLEJOS");
        #endif
        // OK cuando la luz esté ve
        tft.setCursor(4, 128);
        tft.setTextSize(1);
        tft.setTextColor(0x7BEF);
        tft.print("OK cuando la luz est\202 verde");
        break;

    case PAGE_MOTOR:
        // CONTROL DE MOTOR
        #if JWPLC_HMI_TFT_FONTS
        tft.setFont(JWPLC_FONT_SANS_BOLD_9);
        tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
        tft.setTextSize(1);
        tft.setTextColor(0x07FF);
        tft.drawString("CONTROL DE MOTOR", 4, 2);
        tft.setFont(JWPLC_FONT_GLCD);
        #else
        tft.setCursor(4, 2);
        tft.setTextSize(1);
        tft.setTextColor(0x07FF);
        tft.print("CONTROL DE MOTOR");
        #endif
        // FIELD_TRIANGLE_24
        jwplcHMIFillTriangle(tft, 8, 124, 12, 132, 4, 132, 0xD69A);
        tft.drawLine(8, 124, 12, 132, 0xD69A);
        tft.drawLine(12, 132, 4, 132, 0xD69A);
        tft.drawLine(4, 132, 8, 124, 0xD69A);
        // FIELD_TRIANGLE_25
        jwplcHMIFillTriangle(tft, 18, 132, 14, 124, 22, 124, 0xD69A);
        tft.drawLine(18, 132, 14, 124, 0xD69A);
        tft.drawLine(14, 124, 22, 124, 0xD69A);
        tft.drawLine(22, 124, 18, 132, 0xD69A);
        // valor
        tft.setCursor(26, 124);
        tft.setTextSize(1);
        tft.setTextColor(0x7BEF);
        tft.print("valor");
        // FIELD_TRIANGLE_27
        jwplcHMIFillTriangle(tft, 70, 128, 78, 124, 78, 132, 0xD69A);
        tft.drawLine(70, 128, 78, 124, 0xD69A);
        tft.drawLine(78, 124, 78, 132, 0xD69A);
        tft.drawLine(78, 132, 70, 128, 0xD69A);
        // FIELD_TRIANGLE_28
        jwplcHMIFillTriangle(tft, 88, 128, 80, 132, 80, 124, 0xD69A);
        tft.drawLine(88, 128, 80, 132, 0xD69A);
        tft.drawLine(80, 132, 80, 124, 0xD69A);
        tft.drawLine(80, 124, 88, 128, 0xD69A);
        // paso   OK marcha
        tft.setCursor(92, 124);
        tft.setTextSize(1);
        tft.setTextColor(0x7BEF);
        tft.print("paso   OK marcha");
        break;

    default:
        break;
    }

    jwplcDrawPageNavBar(page);

    if (jwplcHMIUserPageEnter)
    {
        jwplcHMIUserPageEnter(page);
    }
}

// Sincronización gráfica autogenerada (cada tick de refresco).
// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.
// La lógica de aplicación debe permanecer en loop().
void jwplcUIUpdate()
{
    switch (JWPLC_Display.userPage())
    {
    case PAGE_RECETA:
        // Página 01 · Receta
        JWPLC_Display.setValue(FIELD_R_NUM, recetaNum);
        JWPLC_Display.setText(FIELD_RECETA, receta);
        JWPLC_Display.setValue(FIELD_R_TEMP, recetaTemp);
        JWPLC_Display.setValue(FIELD_R_TIEMPO, recetaTiempo);
        JWPLC_Display.setBool(FIELD_R_CARGADA, recetaCargada);
        break;

    case PAGE_REFLEJOS:
        // Página 02 · Reflejos
        JWPLC_Display.setBool(FIELD_LUZ, luz);
        JWPLC_Display.setText(FIELD_ESTADO_JUEGO, estadoJuego);
        JWPLC_Display.setValue(FIELD_REACCION, tiempoReaccion);
        JWPLC_Display.setValue(FIELD_RECORD, record);
        break;

    case PAGE_MOTOR:
        // Página 03 · Motor
        JWPLC_Display.setBool(FIELD_MARCHA, marcha);
        JWPLC_Display.setValue(FIELD_CONSIGNA, consigna);
        JWPLC_Display.setText(FIELD_PASO, paso);
        JWPLC_Display.setBar(FIELD_BAR_CONSIGNA, barConsigna);
        JWPLC_Display.setBar(FIELD_BAR_VELOCIDAD, velocidad);
        break;

    default:
        break;
    }
}
