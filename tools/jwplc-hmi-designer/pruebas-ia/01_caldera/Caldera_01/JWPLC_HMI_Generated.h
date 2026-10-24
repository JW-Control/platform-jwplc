// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_RESUMEN = 0,
    PAGE_PROCESO = 1,
    PAGE_ALARMAS = 2,
    PAGE_PRODUCCION = 3,
    PAGE_AJUSTES = 4
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Resumen
    FIELD_HORA = 1,
    FIELD_TEMP = 2,
    FIELD_PRESION = 3,
    FIELD_NIVEL = 4,
    FIELD_QUEMADOR = 5,
    FIELD_BOMBA = 6,
    FIELD_MODO = 7,

    // Página 02 · Proceso
    FIELD_TEMP_TANQUE = 8,
    FIELD_VAPOR = 9,
    FIELD_AGUA = 10,
    FIELD_VALVULA = 11,

    // Página 03 · Alarmas
    FIELD_AL_NIVEL = 12,
    FIELD_AL_PRESION = 13,
    FIELD_AL_LLAMA = 14,
    FIELD_ULTIMA = 15,

    // Página 04 · Producción
    FIELD_VAPOR_TOTAL = 16,
    FIELD_HOROMETRO = 17,
    FIELD_EFICIENCIA = 18,
    FIELD_COMBUSTIBLE = 19,
};

// Variables HMI
// Página 01 · Resumen
float temperatura = 0.0f;
float presion = 0.0f;
float nivelAgua = 0.0f;
bool quemador = false;
bool bomba = false;
char modo[13] = {}; // 6 caracteres (UTF-8: cada tilde ocupa 2 bytes)

// Página 02 · Proceso
float tempHogar = 0.0f;
float caudalVapor = 0.0f;
float caudalAgua = 0.0f;
bool valvulaVapor = false;

// Página 03 · Alarmas
bool alNivelBajo = false;
bool alSobrepresion = false;
bool alFallaLlama = false;
char ultimaAlarma[49] = {}; // 24 caracteres (UTF-8: cada tilde ocupa 2 bytes)

// Página 04 · Producción
uint32_t vaporTotalKg = 0;
uint32_t horometroSeg = 0;
float eficiencia = 0.0f;
float combustible = 0.0f;

// MENU Ajustes de caldera
float consignaPresion = 8.0f;
float histeresis = 0.5f;
bool modoAuto = true;
float tempMax = 180.0f;
float presionMax = 10.0f;
int32_t retardoLlama = 5;
bool purga = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_HORA,
        JWPLC_UIRect(282, 2),
        JWPLC_UIText(nullptr, nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_HH_MM),
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
            0x000F,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(4, 22),
        JWPLC_UIText("Temperatura", "\367C"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            3,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xFD20,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_PRESION,
        JWPLC_UIRect(172, 22),
        JWPLC_UIText("Presi\242n", "bar"),
        JWPLC_UIValueFormat(
            2,
            2,
            false,
            false),
        JWPLC_UIValueStyle(
            3,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_NIVEL,
        JWPLC_UIRect(4, 70, 312, JWPLC_UI_AUTO),
        JWPLC_UIText("Nivel de agua", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x34DF,
            0x10C5,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_QUEMADOR,
        JWPLC_UIRect(4, 110),
        JWPLC_UIText("Quemador", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x7BEF,
            0xFD20),
#else
        JWPLC_UIBoolText(
            "OFF",
            "ON"),
#endif
        JWPLC_UIBoolStyle(
            2,
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
        FIELD_BOMBA,
        JWPLC_UIRect(118, 110),
        JWPLC_UIText("Bomba", nullptr),
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
            2,
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
    JWPLC_UITextField(
        FIELD_MODO,
        JWPLC_UIRect(216, 112),
        JWPLC_UIText("Modo", nullptr, 6),
        JWPLC_UITextFieldStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_TEMP_TANQUE,
        JWPLC_UIRect(134, 44),
        JWPLC_UIText(nullptr, "\367C"),
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
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFD20,
            0x1106,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VAPOR,
        JWPLC_UIRect(212, 60),
        JWPLC_UIText("Vapor", "t/h"),
        JWPLC_UIValueFormat(
            2,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_AGUA,
        JWPLC_UIRect(4, 60),
        JWPLC_UIText("Agua", "m3/h"),
        JWPLC_UIValueFormat(
            2,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x34DF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_VALVULA,
        JWPLC_UIRect(212, 106),
        JWPLC_UIText("V\240lvula", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "CERRADA",
            "ABIERTA",
            0xF800,
            0x07E0,
            0x0000,
            0x0000),
#else
        JWPLC_UIBoolText(
            "CERRADA",
            "ABIERTA"),
#endif
        JWPLC_UIBoolStyle(
            1,
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
    JWPLC_UIBoolField(
        FIELD_AL_NIVEL,
        JWPLC_UIRect(30, 8),
        JWPLC_UIText("Nivel bajo de agua ", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "OK",
            "ALARMA",
            0x07E0,
            0xFFFF,
            0x0000,
            0xF800),
#else
        JWPLC_UIBoolText(
            "OK",
            "ALARMA"),
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
    JWPLC_UIBoolField(
        FIELD_AL_PRESION,
        JWPLC_UIRect(30, 34),
        JWPLC_UIText("Sobrepresi\242n       ", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "OK",
            "ALARMA",
            0x07E0,
            0xFFFF,
            0x0000,
            0xF800),
#else
        JWPLC_UIBoolText(
            "OK",
            "ALARMA"),
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
    JWPLC_UIBoolField(
        FIELD_AL_LLAMA,
        JWPLC_UIRect(30, 60),
        JWPLC_UIText("Falla de llama     ", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "OK",
            "ALARMA",
            0x07E0,
            0xFFFF,
            0x0000,
            0xF800),
#else
        JWPLC_UIBoolText(
            "OK",
            "ALARMA"),
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
    JWPLC_UITextField(
        FIELD_ULTIMA,
        JWPLC_UIRect(4, 96),
        JWPLC_UIText("Ultima", nullptr, 24),
        JWPLC_UITextFieldStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0xFD20,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VAPOR_TOTAL,
        JWPLC_UIRect(4, 6),
        JWPLC_UIText("Vapor total", "kg"),
        JWPLC_UIValueFormat(
            7,
            0,
            false,
            false,
            '.'),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_HOROMETRO,
        JWPLC_UIRect(4, 34),
        JWPLC_UIText("Marcha", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_HH_MM_SS,
            JWPLC_UI_DURATION_SECONDS,
            3,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0xD69A,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_EFICIENCIA,
        JWPLC_UIRect(4, 64),
        JWPLC_UIText("Eficiencia", "%"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            3,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_COMBUSTIBLE,
        JWPLC_UIRect(160, 72, 156, JWPLC_UI_AUTO),
        JWPLC_UIText("Combustible", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0xFFE0,
            0x2102,
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
    MENU_AJUSTES_PRESION_ACTUAL = 0,
    MENU_AJUSTES_CONSIGNA_PRESION = 1,
    MENU_AJUSTES_HISTERESIS = 2,
    MENU_AJUSTES_MODO_AUTOMATICO = 3,
    MENU_AJUSTES_PROTECCIONES = 4,
    MENU_AJUSTES_TEMP__MAXIMA = 5,
    MENU_AJUSTES_PRESION_MAXIMA = 6,
    MENU_AJUSTES_RETARDO_LLAMA = 7,
    MENU_AJUSTES_MANTENIMIENTO = 8,
    MENU_AJUSTES_EFICIENCIA = 9,
    MENU_AJUSTES_PURGA_MANUAL = 10,
    MENU_AJUSTES_SALIR_AL_INICIO = 11,
};

static const JWPLC_UIMenuNode HMI_MENU_AJUSTES_NODES[] =
{
    JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Presi\242n actual", &presion, 2, "bar"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna presi\242n", &consignaPresion, 1.0f, 12.0f, 0.1f, 1, "bar"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Hist\202resis", &histeresis, 0.1f, 2.0f, 0.1f, 1, "bar"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Modo autom\240tico", &modoAuto, "MANUAL", "AUTO"),
    JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "Protecciones"),
    JWPLC_UIMenuEditNum(MENU_AJUSTES_PROTECCIONES, "Temp. m\240xima", &tempMax, 100.0f, 220.0f, 1.0f, 0, "\367C"),
    JWPLC_UIMenuEditNum(MENU_AJUSTES_PROTECCIONES, "Presi\242n m\240xima", &presionMax, 2.0f, 14.0f, 0.1f, 1, "bar"),
    JWPLC_UIMenuEditNum(MENU_AJUSTES_PROTECCIONES, "Retardo llama", &retardoLlama, 1, 30, 1, "s"),
    JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "Mantenimiento"),
    JWPLC_UIMenuView(MENU_AJUSTES_MANTENIMIENTO, "Eficiencia", &eficiencia, 1, "%"),
    JWPLC_UIMenuEditBool(MENU_AJUSTES_MANTENIMIENTO, "Purga manual", &purga, "NO", "SI"),
    JWPLC_UIMenuGoIdle(JWPLC_UI_MENU_ROOT, "Salir al inicio")
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuField(
        MENU_AJUSTES,
        4, 2, 312, 140,
        "Ajustes de caldera",
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
    PIXEL_LLAMA_CONTORNO = 0,
    PIXEL_LLAMA_NUCLEO = 1,
    PIXEL_AVISO = 2
};

// PixelMaps estáticos RGB565 · JWPLC HMI Designer
// Formato: PACKED_SPAN16 · 115 px -> 43 spans · 3 entradas de paleta
// Estimado: 238 B vs 452 B RGB565_RUN · ahorro 47%

static const uint16_t HMI_PM_1_PAL[] =
{
    0xFD20
};

static const uint32_t HMI_PM_1_DATA[] =
{
    0x400704A6u, 0x400706A5u, 0x400508A7u, 0x40070AA4u, 0x40050CA8u, 0x400B0EA3u, 0x400910A9u, 0x400512A2u,
    0x400512AAu, 0x400318A4u, 0x400318A8u, 0x40011AA5u, 0x40011AA6u, 0x40011AA7u
};

static const uint16_t HMI_PM_2_PAL[] =
{
    0xFFE0
};

static const uint32_t HMI_PM_2_DATA[] =
{
    0x400D10A5u, 0x400B12A4u, 0x400B12A6u, 0x400516A3u, 0x400516A7u
};

static const uint16_t HMI_PM_3_PAL[] =
{
    0xFFE0
};

static const uint32_t HMI_PM_3_DATA[] =
{
    0x4002140Eu, 0x4004160Du, 0x4004160Fu, 0x40041A0Cu, 0x40041A10u, 0x40061C0Eu, 0x40041E0Bu, 0x40041E11u,
    0x4004220Au, 0x40042212u, 0x40042609u, 0x4000260Eu, 0x40042613u, 0x40002A08u, 0x40002A0Au, 0x40002A0Bu,
    0x40002A0Cu, 0x40002A0Du, 0x40002A0Eu, 0x40002A0Fu, 0x40002A10u, 0x40002A11u, 0x40002A12u, 0x40002A14u
};

static const JWPLC_UIPixelPackedMap HMI_PIXEL_MAPS[] =
{
    JWPLC_UIPixelPackedMap(1, HMI_PM_1_PAL, sizeof(HMI_PM_1_PAL) / sizeof(HMI_PM_1_PAL[0]), HMI_PM_1_DATA, sizeof(HMI_PM_1_DATA) / sizeof(HMI_PM_1_DATA[0])),
    JWPLC_UIPixelPackedMap(1, HMI_PM_2_PAL, sizeof(HMI_PM_2_PAL) / sizeof(HMI_PM_2_PAL[0]), HMI_PM_2_DATA, sizeof(HMI_PM_2_DATA) / sizeof(HMI_PM_2_DATA[0])),
    JWPLC_UIPixelPackedMap(2, HMI_PM_3_PAL, sizeof(HMI_PM_3_PAL) / sizeof(HMI_PM_3_PAL[0]), HMI_PM_3_DATA, sizeof(HMI_PM_3_DATA) / sizeof(HMI_PM_3_DATA[0]))
};

void jwplcHMISetup()
{
    JWPLC_Display.setMenus(
        HMI_MENUS,
        sizeof(HMI_MENUS) / sizeof(HMI_MENUS[0]));
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(5);
    JWPLC_Display.setUserPage(PAGE_RESUMEN);
    // Indicador NN/TT (barra de navegación)
#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG
    JWPLC_Display.setPageIndicator(false);
#endif
    JWPLC_Display.setPackedPixelMaps(HMI_PIXEL_MAPS, sizeof(HMI_PIXEL_MAPS) / sizeof(HMI_PIXEL_MAPS[0]));
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(300000UL);
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
            tft.fillRoundRect(6, 145, 60, 18, 3, 0x073F);
            tft.setCursor(15, 150);
            tft.setTextColor(0x0000);
            tft.print("RESUMEN");
        }
        else
        {
            tft.fillRoundRect(6, 145, 60, 18, 3, 0xFFFF);
            tft.drawRoundRect(6, 145, 60, 18, 3, 0x032E);
            tft.drawRoundRect(8, 147, 56, 14, 1, 0x032E);
            tft.setCursor(15, 150);
            tft.setTextColor(0x032E);
            tft.print("RESUMEN");
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(69, 145, 60, 18, 3, 0xFC80);
            tft.setCursor(78, 150);
            tft.setTextColor(0x0000);
            tft.print("PROCESO");
        }
        else
        {
            tft.fillRoundRect(69, 145, 60, 18, 3, 0xFFFF);
            tft.drawRoundRect(69, 145, 60, 18, 3, 0x7200);
            tft.drawRoundRect(71, 147, 56, 14, 1, 0x7200);
            tft.setCursor(78, 150);
            tft.setTextColor(0x7200);
            tft.print("PROCESO");
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(132, 145, 60, 18, 3, 0xF800);
            tft.setCursor(141, 150);
            tft.setTextColor(0xFFFF);
            tft.print("ALARMAS");
        }
        else
        {
            tft.fillRoundRect(132, 145, 60, 18, 3, 0xFFFF);
            tft.drawRoundRect(132, 145, 60, 18, 3, 0xF800);
            tft.drawRoundRect(134, 147, 56, 14, 1, 0xF800);
            tft.setCursor(141, 150);
            tft.setTextColor(0xF800);
            tft.print("ALARMAS");
        }
        break;
    case 3:
        if (selecting)
        {
            tft.fillRoundRect(195, 145, 60, 18, 3, 0x072E);
            tft.setCursor(207, 150);
            tft.setTextColor(0x0000);
            tft.print("PRODUC");
        }
        else
        {
            tft.fillRoundRect(195, 145, 60, 18, 3, 0xFFFF);
            tft.drawRoundRect(195, 145, 60, 18, 3, 0x0346);
            tft.drawRoundRect(197, 147, 56, 14, 1, 0x0346);
            tft.setCursor(207, 150);
            tft.setTextColor(0x0346);
            tft.print("PRODUC");
        }
        break;
    case 4:
        if (selecting)
        {
            tft.fillRoundRect(258, 145, 60, 18, 3, 0xFF20);
            tft.setCursor(267, 150);
            tft.setTextColor(0x0000);
            tft.print("AJUSTES");
        }
        else
        {
            tft.fillRoundRect(258, 145, 60, 18, 3, 0xFFFF);
            tft.drawRoundRect(258, 145, 60, 18, 3, 0x7340);
            tft.drawRoundRect(260, 147, 56, 14, 1, 0x7340);
            tft.setCursor(267, 150);
            tft.setTextColor(0x7340);
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
        tft.fillRoundRect(2, 144, 320, 20, 3, 0x0000);
        tft.fillRoundRect(69, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(69, 145, 60, 18, 3, 0xFC80);
        tft.setCursor(78, 150);
        tft.setTextColor(0xFC80);
        tft.print("PROCESO");
        tft.fillRoundRect(132, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(132, 145, 60, 18, 3, 0xF800);
        tft.setCursor(141, 150);
        tft.setTextColor(0xF800);
        tft.print("ALARMAS");
        tft.fillRoundRect(195, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(195, 145, 60, 18, 3, 0x072E);
        tft.setCursor(207, 150);
        tft.setTextColor(0x072E);
        tft.print("PRODUC");
        tft.fillRoundRect(258, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(258, 145, 60, 18, 3, 0xFF20);
        tft.setCursor(267, 150);
        tft.setTextColor(0xFF20);
        tft.print("AJUSTES");
        break;
    case 1:
        tft.fillRoundRect(2, 144, 320, 20, 3, 0x0000);
        tft.fillRoundRect(6, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(6, 145, 60, 18, 3, 0x073F);
        tft.setCursor(15, 150);
        tft.setTextColor(0x073F);
        tft.print("RESUMEN");
        tft.fillRoundRect(132, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(132, 145, 60, 18, 3, 0xF800);
        tft.setCursor(141, 150);
        tft.setTextColor(0xF800);
        tft.print("ALARMAS");
        tft.fillRoundRect(195, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(195, 145, 60, 18, 3, 0x072E);
        tft.setCursor(207, 150);
        tft.setTextColor(0x072E);
        tft.print("PRODUC");
        tft.fillRoundRect(258, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(258, 145, 60, 18, 3, 0xFF20);
        tft.setCursor(267, 150);
        tft.setTextColor(0xFF20);
        tft.print("AJUSTES");
        break;
    case 2:
        tft.fillRoundRect(2, 144, 320, 20, 3, 0x0000);
        tft.fillRoundRect(6, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(6, 145, 60, 18, 3, 0x073F);
        tft.setCursor(15, 150);
        tft.setTextColor(0x073F);
        tft.print("RESUMEN");
        tft.fillRoundRect(69, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(69, 145, 60, 18, 3, 0xFC80);
        tft.setCursor(78, 150);
        tft.setTextColor(0xFC80);
        tft.print("PROCESO");
        tft.fillRoundRect(195, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(195, 145, 60, 18, 3, 0x072E);
        tft.setCursor(207, 150);
        tft.setTextColor(0x072E);
        tft.print("PRODUC");
        tft.fillRoundRect(258, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(258, 145, 60, 18, 3, 0xFF20);
        tft.setCursor(267, 150);
        tft.setTextColor(0xFF20);
        tft.print("AJUSTES");
        break;
    case 3:
        tft.fillRoundRect(2, 144, 320, 20, 3, 0x0000);
        tft.fillRoundRect(6, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(6, 145, 60, 18, 3, 0x073F);
        tft.setCursor(15, 150);
        tft.setTextColor(0x073F);
        tft.print("RESUMEN");
        tft.fillRoundRect(69, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(69, 145, 60, 18, 3, 0xFC80);
        tft.setCursor(78, 150);
        tft.setTextColor(0xFC80);
        tft.print("PROCESO");
        tft.fillRoundRect(132, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(132, 145, 60, 18, 3, 0xF800);
        tft.setCursor(141, 150);
        tft.setTextColor(0xF800);
        tft.print("ALARMAS");
        tft.fillRoundRect(258, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(258, 145, 60, 18, 3, 0xFF20);
        tft.setCursor(267, 150);
        tft.setTextColor(0xFF20);
        tft.print("AJUSTES");
        break;
    case 4:
        tft.fillRoundRect(2, 144, 320, 20, 3, 0x0000);
        tft.fillRoundRect(6, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(6, 145, 60, 18, 3, 0x073F);
        tft.setCursor(15, 150);
        tft.setTextColor(0x073F);
        tft.print("RESUMEN");
        tft.fillRoundRect(69, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(69, 145, 60, 18, 3, 0xFC80);
        tft.setCursor(78, 150);
        tft.setTextColor(0xFC80);
        tft.print("PROCESO");
        tft.fillRoundRect(132, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(132, 145, 60, 18, 3, 0xF800);
        tft.setCursor(141, 150);
        tft.setTextColor(0xF800);
        tft.print("ALARMAS");
        tft.fillRoundRect(195, 145, 60, 18, 3, 0x0000);
        tft.drawRoundRect(195, 145, 60, 18, 3, 0x072E);
        tft.setCursor(207, 150);
        tft.setTextColor(0x072E);
        tft.print("PRODUC");
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
    case PAGE_RESUMEN:
        // FIELD_RECT_2
        tft.fillRect(0, 0, 319, 16, 0x000F);
        tft.drawRect(0, 0, 319, 16, 0x000F);
        // CALDERA 01 · Planta Nort
        tft.setCursor(4, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("CALDERA 01 \371 Planta Norte");
        break;

    case PAGE_PROCESO:
        // Diagrama de proceso
        #if JWPLC_HMI_TFT_FONTS
        tft.setFont(JWPLC_FONT_SANS_BOLD_9);
        tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.drawString("Diagrama de proceso", 4, 2);
        tft.setFont(JWPLC_FONT_GLCD);
        #else
        tft.setCursor(4, 2);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("Diagrama de proceso");
        #endif
        // FIELD_LINE_12
        tft.drawLine(8, 96, 78, 96, 0x34DF);
        // FIELD_ELLIPSE_13
        tft.fillCircle(88, 96, 10, 0x03E0);
        tft.drawCircle(88, 96, 10, 0x07E0);
        tft.drawCircle(88, 96, 9, 0x07E0);
        // FIELD_LINE_14
        tft.drawLine(98, 96, 126, 96, 0x34DF);
        // FIELD_RECT_15
        tft.fillRoundRect(126, 28, 80, 100, 10, 0x1106);
        tft.drawRoundRect(126, 28, 80, 100, 10, 0xD69A);
        tft.drawRoundRect(127, 29, 78, 98, 9, 0xD69A);
        // FIELD_LINE_16
        tft.drawLine(206, 44, 312, 44, 0xFD20);
        // FIELD_TRIANGLE_17
        jwplcHMIFillTriangle(tft, 260, 44, 244, 52, 244, 36, 0xD69A);
        tft.drawLine(260, 44, 244, 52, 0xD69A);
        tft.drawLine(244, 52, 244, 36, 0xD69A);
        tft.drawLine(244, 36, 260, 44, 0xD69A);
        // FIELD_TRIANGLE_18
        jwplcHMIFillTriangle(tft, 260, 44, 276, 36, 276, 52, 0xD69A);
        tft.drawLine(260, 44, 276, 36, 0xD69A);
        tft.drawLine(276, 36, 276, 52, 0xD69A);
        tft.drawLine(276, 52, 260, 44, 0xD69A);
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
    case PAGE_RESUMEN:
        // Página 01 · Resumen
        JWPLC_Display.setValue(FIELD_TEMP, temperatura);
        JWPLC_Display.setValue(FIELD_PRESION, presion);
        JWPLC_Display.setBar(FIELD_NIVEL, nivelAgua);
        JWPLC_Display.setBool(FIELD_QUEMADOR, quemador);
        JWPLC_Display.setBool(FIELD_BOMBA, bomba);
        JWPLC_Display.setText(FIELD_MODO, modo);
        break;

    case PAGE_PROCESO:
        // Página 02 · Proceso
        JWPLC_Display.setValue(FIELD_TEMP_TANQUE, tempHogar);
        JWPLC_Display.setValue(FIELD_VAPOR, caudalVapor);
        JWPLC_Display.setValue(FIELD_AGUA, caudalAgua);
        JWPLC_Display.setBool(FIELD_VALVULA, valvulaVapor);
        break;

    case PAGE_ALARMAS:
        // Página 03 · Alarmas
        JWPLC_Display.setBool(FIELD_AL_NIVEL, alNivelBajo);
        JWPLC_Display.setBool(FIELD_AL_PRESION, alSobrepresion);
        JWPLC_Display.setBool(FIELD_AL_LLAMA, alFallaLlama);
        JWPLC_Display.setText(FIELD_ULTIMA, ultimaAlarma);
        break;

    case PAGE_PRODUCCION:
        // Página 04 · Producción
        JWPLC_Display.setValue(FIELD_VAPOR_TOTAL, vaporTotalKg);
        JWPLC_Display.setValue(FIELD_HOROMETRO, horometroSeg);
        JWPLC_Display.setValue(FIELD_EFICIENCIA, eficiencia);
        JWPLC_Display.setBar(FIELD_COMBUSTIBLE, combustible);
        break;

    default:
        break;
    }
}
