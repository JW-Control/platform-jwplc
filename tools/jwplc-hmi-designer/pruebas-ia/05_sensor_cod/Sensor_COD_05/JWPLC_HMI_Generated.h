// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_MEDICION = 0,
    PAGE_VISTA_GENERAL = 1,
    PAGE_TEMP_COD = 2,
    PAGE_GRAF_COD = 3,
    PAGE_GRAF_BOD = 4,
    PAGE_GRAF_TOC = 5,
    PAGE_GRAF_TURB = 6
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Medicion
    FIELD_RTC_CFG = 1,
    FIELD_MEAS_PROGRESS = 2,
    FIELD_STATE_CFG = 3,
    FIELD_MEAS_ELAPSED = 4,

    // Página 02 · Vista_General
    FIELD_RTC_GEN = 5,
    FIELD_GEN_TCOD = 6,
    FIELD_GEN_COD = 7,
    FIELD_GEN_BOD = 8,
    FIELD_GEN_TOC = 9,
    FIELD_GEN_TURB = 10,
    FIELD_GEN_STATE = 11,

    // Página 03 · Temp_COD
    FIELD_ST_TCOD = 12,
    FIELD_TREND_TCOD = 13,

    // Página 04 · Graf_COD
    FIELD_ST_COD = 14,
    FIELD_TREND_COD = 15,

    // Página 05 · Graf_BOD
    FIELD_ST_BOD = 16,
    FIELD_TREND_BOD = 17,

    // Página 06 · Graf_TOC
    FIELD_ST_TOC = 18,
    FIELD_TREND_TOC = 19,

    // Página 07 · Graf_TURB
    FIELD_ST_TURB = 20,
    FIELD_TREND_TURB = 21,
};

// Variables HMI
// Página 01 · Medicion
float measProgress = 0.0f;
bool stCfg = false;
uint32_t measElapsedS = 0;

// Página 02 · Vista_General
float valTCod = 0.0f;
float valCod = 0.0f;
float valBod = 0.0f;
float valToc = 0.0f;
float valTurb = 0.0f;
bool stGen = false;

// Página 03 · Temp_COD
bool stTCod = false;
float trendTCod = 0.0f;

// Página 04 · Graf_COD
bool stCod = false;
float trendCod = 0.0f;

// Página 05 · Graf_BOD
bool stBod = false;
float trendBod = 0.0f;

// Página 06 · Graf_TOC
bool stToc = false;
float trendToc = 0.0f;

// Página 07 · Graf_TURB
bool stTurb = false;
float trendTurb = 0.0f;

// MENU Control de Medicion
bool measRun = false;
int32_t measTag = 1;
int32_t measMins = 2;
bool simActive = true;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_RTC_CFG,
        JWPLC_UIRect(262, 4),
        JWPLC_UIText(nullptr, nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_HH_MM_SS),
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
            0x0906,
            0xFFFF)),
    // Barra sólida (HORIZONTAL) - Radio: 0
    JWPLC_UIBarField(
        FIELD_MEAS_PROGRESS,
        JWPLC_UIRect(4, 93, 312, JWPLC_UI_AUTO),
        JWPLC_UIText("Progreso", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07EC,
            0x1905,
            0x428C),
        JWPLC_UIBarConfig(
            JWPLC_UI_BAR_HORIZONTAL,
            JWPLC_UI_BAR_STYLE_SOLID,
            0,
            0,
            false,
            0xFFFF,
            1,
            14,
            0)),
    JWPLC_UIBoolField(
        FIELD_STATE_CFG,
        JWPLC_UIRect(4, 124),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xB5D8,
            0x07EC,
            0x18E4,
            0x0183),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x530E)),
    JWPLC_UIValueField(
        FIELD_MEAS_ELAPSED,
        JWPLC_UIRect(164, 124),
        JWPLC_UIText("Tiempo", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_MM_SS,
            JWPLC_UI_DURATION_SECONDS,
            3,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xC65A,
            0x07FF,
            0x0000,
            0x428C)),
    JWPLC_UIValueField(
        FIELD_RTC_GEN,
        JWPLC_UIRect(262, 4),
        JWPLC_UIText(nullptr, nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_HH_MM_SS),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x10C4,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_GEN_TCOD,
        JWPLC_UIRect(6, 22),
        JWPLC_UIText("T_COD", "C"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xF81F,
            0x18E3,
            0xF81F)),
    JWPLC_UIValueField(
        FIELD_GEN_COD,
        JWPLC_UIRect(164, 22),
        JWPLC_UIText("COD", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFF00,
            0x18E3,
            0xFF00)),
    JWPLC_UIValueField(
        FIELD_GEN_BOD,
        JWPLC_UIRect(6, 63),
        JWPLC_UIText("BOD", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x18E3,
            0x07FF)),
    JWPLC_UIValueField(
        FIELD_GEN_TOC,
        JWPLC_UIRect(164, 63),
        JWPLC_UIText("TOC", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x18E3,
            0x07E0)),
    JWPLC_UIValueField(
        FIELD_GEN_TURB,
        JWPLC_UIRect(6, 104),
        JWPLC_UIText("TURB", "NTU"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFC40,
            0x18E3,
            0xFC40)),
    JWPLC_UIBoolField(
        FIELD_GEN_STATE,
        JWPLC_UIRect(164, 104),
        JWPLC_UIText("MEDICION", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REPOSO] ",
            "[MIDIENDO]",
            0xB5D8,
            0x07EC,
            0x18E3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REPOSO] ",
            "[MIDIENDO]"),
#endif
        JWPLC_UIBoolStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x18E3,
            0x8494)),
    JWPLC_UIBoolField(
        FIELD_ST_TCOD,
        JWPLC_UIRect(274, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xA556,
            0x07EC,
            0x10A3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
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
            0x530E)),
    // Gráfica de Tendencia (316x127) - Muestreo: 500ms - Escala: AUTO - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TREND_TCOD,
        JWPLC_UIRect(2, 18, 316, 127),
        JWPLC_UIText("T_CD", "C"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            500,
            120,
            0,
            1,
            1,
            true,
            true,
            false,
            1000,
            0xF81F,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            35.0f,
            0xF800,
            20.0f,
            0xFFE0,
            true,
            "SP",
            "CH3"),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0xF81F,
            0x0000,
            0x4208)),
    JWPLC_UIBoolField(
        FIELD_ST_COD,
        JWPLC_UIRect(274, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xA556,
            0x07EC,
            0x10A3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x530E)),
    // Gráfica de Tendencia (316x127) - Muestreo: 500ms - Escala: AUTO - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TREND_COD,
        JWPLC_UIRect(2, 18, 316, 127),
        JWPLC_UIText("COD", "mg/L"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            500,
            120,
            0,
            1,
            1,
            true,
            true,
            false,
            1000,
            0xFF00,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            80.0f,
            0xF800,
            20.0f,
            0xFFE0,
            true,
            "SP",
            "CH3"),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0xFF00,
            0x0000,
            0x4208)),
    JWPLC_UIBoolField(
        FIELD_ST_BOD,
        JWPLC_UIRect(274, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xA556,
            0x07EC,
            0x10A3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        4,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x530E)),
    // Gráfica de Tendencia (316x127) - Muestreo: 500ms - Escala: AUTO - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TREND_BOD,
        JWPLC_UIRect(2, 18, 316, 127),
        JWPLC_UIText("BOD", "mg/L"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            500,
            120,
            0,
            1,
            1,
            true,
            true,
            false,
            1000,
            0x07FF,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            45.0f,
            0xF800,
            20.0f,
            0xFFE0,
            true,
            "SP",
            "CH3"),
        4,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x4208)),
    JWPLC_UIBoolField(
        FIELD_ST_TOC,
        JWPLC_UIRect(274, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xA556,
            0x07EC,
            0x10A3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        5,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x530E)),
    // Gráfica de Tendencia (316x127) - Muestreo: 500ms - Escala: AUTO - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TREND_TOC,
        JWPLC_UIRect(2, 18, 316, 127),
        JWPLC_UIText("TOC", "mg/L"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            500,
            120,
            0,
            1,
            1,
            true,
            true,
            false,
            1000,
            0x07E0,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            30.0f,
            0xF800,
            20.0f,
            0xFFE0,
            true,
            "SP",
            "CH3"),
        5,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x0000,
            0x4208)),
    JWPLC_UIBoolField(
        FIELD_ST_TURB,
        JWPLC_UIRect(274, 2),
        JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]",
            0xA556,
            0x07EC,
            0x10A3,
            0x0142),
#else
        JWPLC_UIBoolText(
            "[REP]",
            "[MID]"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        6,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x530E)),
    // Gráfica de Tendencia (316x127) - Muestreo: 500ms - Escala: AUTO - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TREND_TURB,
        JWPLC_UIRect(2, 18, 316, 127),
        JWPLC_UIText("TURB", "NTU"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            500,
            120,
            0,
            1,
            1,
            true,
            true,
            false,
            1000,
            0xFC40,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            25.0f,
            0xF800,
            20.0f,
            0xFFE0,
            true,
            "SP",
            "CH3"),
        6,
        JWPLC_UIColors(
            0xFFFF,
            0xFC40,
            0x0000,
            0x4208))
};

// MENU (A15-8): requiere JWPLC_Display con soporte de MENU.
#ifndef JWPLC_UI_HAS_MENU
#error "El MENU requiere JWPLC_Display con soporte de MENU (package JWPLC Alpha15 o posterior)."
#endif

enum HMIMenuId : uint8_t
{
    MENU_MEDICION = 0,
};

// Nodos de MENU_MEDICION: el índice es el que informa JWPLC_Display.menuNode().
enum : uint8_t
{
    MENU_MEDICION_MEDICION = 0,
    MENU_MEDICION_ETIQUETA_P = 1,
    MENU_MEDICION_DURACION = 2,
    MENU_MEDICION_SIMULACION = 3,
};

static const JWPLC_UIMenuNode HMI_MENU_MEDICION_NODES[] =
{
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Medicion", &measRun, "REPOSO", "INICIADA"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Etiqueta P", &measTag, 1, 99, 1, nullptr),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Duracion", &measMins, 1, 30, 1, "min"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Simulacion", &simActive, "PAUSA", "ACTIVA")
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuField(
        MENU_MEDICION,
        4, 20, 312, 68,
        "Control de Medicion",
        HMI_MENU_MEDICION_NODES,
        sizeof(HMI_MENU_MEDICION_NODES) / sizeof(HMI_MENU_MEDICION_NODES[0]),
        PAGE_MEDICION,
        1,
        true,
        JWPLC_UIMenuColors(
            0xC618,
            0x0862,
            0x07FF,
            0xFFFF,
            0x0455,
            0x0000,
            0xFD20,
            0xFFFF,
            0x1147,
            0x06DF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setMenus(
        HMI_MENUS,
        sizeof(HMI_MENUS) / sizeof(HMI_MENUS[0]));
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(7);
    JWPLC_Display.setUserPage(PAGE_MEDICION);
    // Indicador NN/TT (barra de navegación)
#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG
    JWPLC_Display.setPageIndicator(false);
#endif
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
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

// Microfuente 3x5 (celda 4x6) usada por la barra de navegación.
static const uint8_t JWPLC_HMI_MICRO_FONT[][4] =
{
    {'0', 0x1F, 0x11, 0x1F}, {'1', 0x00, 0x1F, 0x00}, {'2', 0x1D, 0x15, 0x17},
    {'3', 0x15, 0x15, 0x1F}, {'4', 0x07, 0x04, 0x1F}, {'5', 0x17, 0x15, 0x1D},
    {'6', 0x1F, 0x15, 0x1D}, {'7', 0x01, 0x1D, 0x03}, {'8', 0x1F, 0x15, 0x1F},
    {'9', 0x17, 0x15, 0x1F}, {'A', 0x1E, 0x05, 0x1E}, {'B', 0x1F, 0x15, 0x0A},
    {'C', 0x0E, 0x11, 0x11}, {'D', 0x1F, 0x11, 0x0E}, {'E', 0x1F, 0x15, 0x11},
    {'F', 0x1F, 0x05, 0x01}, {'G', 0x0E, 0x15, 0x1D}, {'H', 0x1F, 0x04, 0x1F},
    {'I', 0x11, 0x1F, 0x11}, {'J', 0x08, 0x10, 0x0F}, {'K', 0x1F, 0x04, 0x1B},
    {'L', 0x1F, 0x10, 0x10}, {'M', 0x1F, 0x02, 0x1F}, {'N', 0x1F, 0x06, 0x1F},
    {'O', 0x0E, 0x11, 0x0E}, {'P', 0x1F, 0x05, 0x02}, {'Q', 0x0E, 0x11, 0x1E},
    {'R', 0x1F, 0x05, 0x1A}, {'S', 0x12, 0x15, 0x09}, {'T', 0x01, 0x1F, 0x01},
    {'U', 0x0F, 0x10, 0x0F}, {'V', 0x07, 0x18, 0x07}, {'W', 0x1F, 0x08, 0x1F},
    {'X', 0x1B, 0x04, 0x1B}, {'Y', 0x03, 0x1C, 0x03}, {'Z', 0x19, 0x15, 0x13},
    {':', 0x00, 0x0A, 0x00}, {'.', 0x00, 0x10, 0x00}, {'-', 0x04, 0x04, 0x04},
    {'/', 0x18, 0x06, 0x01}, {'_', 0x10, 0x10, 0x10}, {'<', 0x04, 0x0A, 0x11},
    {'>', 0x11, 0x0A, 0x04}
};

static void jwplcHMIDrawMicroText(JWPLC_TFTClass &tft, int16_t x, int16_t y, const char *text, uint16_t color)
{
    for (; *text; ++text, x += 4)
    {
        char c = *text;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        for (size_t g = 0; g < sizeof(JWPLC_HMI_MICRO_FONT) / sizeof(JWPLC_HMI_MICRO_FONT[0]); ++g)
        {
            if (JWPLC_HMI_MICRO_FONT[g][0] != (uint8_t)c) continue;
            for (int16_t col = 0; col < 3; ++col)
                for (int16_t row = 0; row < 5; ++row)
                    if (JWPLC_HMI_MICRO_FONT[g][1 + col] & (1u << row))
                        tft.drawPixel(x + col, y + row, color);
            break;
        }
    }
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
            tft.fillRoundRect(7, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x0000);
        }
        else
        {
            tft.fillRoundRect(7, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(7, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(9, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x02CE);
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(51, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x0000);
        }
        else
        {
            tft.fillRoundRect(51, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(51, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(53, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x02CE);
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(95, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x0000);
        }
        else
        {
            tft.fillRoundRect(95, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(95, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(97, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x02CE);
        }
        break;
    case 3:
        if (selecting)
        {
            tft.fillRoundRect(139, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x0000);
        }
        else
        {
            tft.fillRoundRect(139, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(139, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(141, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x02CE);
        }
        break;
    case 4:
        if (selecting)
        {
            tft.fillRoundRect(183, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x0000);
        }
        else
        {
            tft.fillRoundRect(183, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(183, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(185, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x02CE);
        }
        break;
    case 5:
        if (selecting)
        {
            tft.fillRoundRect(227, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x0000);
        }
        else
        {
            tft.fillRoundRect(227, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(227, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(229, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x02CE);
        }
        break;
    case 6:
        if (selecting)
        {
            tft.fillRoundRect(271, 148, 42, 20, 4, 0x065F);
            jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x0000);
        }
        else
        {
            tft.fillRoundRect(271, 148, 42, 20, 4, 0xFFFF);
            tft.drawRoundRect(271, 148, 42, 20, 4, 0x02CE);
            tft.drawRoundRect(273, 150, 38, 16, 2, 0x02CE);
            jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x02CE);
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
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 1:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 2:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 3:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 4:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 5:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(271, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(271, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 286, 155, "TUR", 0x8D16);
        break;
    case 6:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 147, 320, 0x21E9);
        tft.fillRoundRect(7, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(7, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 22, 155, "CFG", 0x8D16);
        tft.fillRoundRect(51, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(51, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 66, 155, "GEN", 0x8D16);
        tft.fillRoundRect(95, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(95, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 108, 155, "T_CD", 0x8D16);
        tft.fillRoundRect(139, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(139, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 154, 155, "COD", 0x8D16);
        tft.fillRoundRect(183, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(183, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 198, 155, "BOD", 0x8D16);
        tft.fillRoundRect(227, 148, 42, 20, 4, 0x10E5);
        tft.drawRoundRect(227, 148, 42, 20, 4, 0x21C9);
        jwplcHMIDrawMicroText(tft, 242, 155, "TOC", 0x8D16);
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
    case PAGE_MEDICION:
        // FIELD_RECT_2
        tft.fillRect(0, 0, 319, 17, 0x0906);
        tft.drawRect(0, 0, 319, 17, 0x06DF);
        // MEDICION DE COD
        tft.setCursor(4, 5);
        tft.setTextSize(1);
        tft.setTextColor(0x07FF);
        tft.print("MEDICION DE COD");
        break;

    case PAGE_VISTA_GENERAL:
        // FIELD_RECT_9
        tft.fillRect(0, 0, 319, 17, 0x10C4);
        tft.drawRect(0, 0, 319, 17, 0x8494);
        // VISTA GENERAL
        tft.setCursor(4, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("VISTA GENERAL");
        break;

    case PAGE_TEMP_COD:
        // TEMPERATURA T_COD
        tft.setCursor(4, 4);
        tft.setTextSize(1);
        tft.setTextColor(0xF81F);
        tft.print("TEMPERATURA T_COD");
        break;

    case PAGE_GRAF_COD:
        // DEMANDA QUIMICA (COD)
        tft.setCursor(4, 4);
        tft.setTextSize(1);
        tft.setTextColor(0xFF00);
        tft.print("DEMANDA QUIMICA (COD)");
        break;

    case PAGE_GRAF_BOD:
        // DEMANDA BIOQUIMICA (BOD)
        tft.setCursor(4, 4);
        tft.setTextSize(1);
        tft.setTextColor(0x07FF);
        tft.print("DEMANDA BIOQUIMICA (BOD)");
        break;

    case PAGE_GRAF_TOC:
        // CARBONO ORGANICO (TOC)
        tft.setCursor(4, 4);
        tft.setTextSize(1);
        tft.setTextColor(0x07E0);
        tft.print("CARBONO ORGANICO (TOC)");
        break;

    case PAGE_GRAF_TURB:
        // TURBIDEZ (TURB)
        tft.setCursor(4, 4);
        tft.setTextSize(1);
        tft.setTextColor(0xFC40);
        tft.print("TURBIDEZ (TURB)");
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
    // Tendencias en segundo plano (se actualizan aunque estés en otra página)
    JWPLC_Display.setTrend(FIELD_TREND_TCOD, trendTCod);
    JWPLC_Display.setTrend(FIELD_TREND_COD, trendCod);
    JWPLC_Display.setTrend(FIELD_TREND_BOD, trendBod);
    JWPLC_Display.setTrend(FIELD_TREND_TOC, trendToc);
    JWPLC_Display.setTrend(FIELD_TREND_TURB, trendTurb);

    switch (JWPLC_Display.userPage())
    {
    case PAGE_MEDICION:
        // Página 01 · Medicion
        JWPLC_Display.setBar(FIELD_MEAS_PROGRESS, measProgress);
        JWPLC_Display.setBool(FIELD_STATE_CFG, stCfg);
        JWPLC_Display.setValue(FIELD_MEAS_ELAPSED, measElapsedS);
        break;

    case PAGE_VISTA_GENERAL:
        // Página 02 · Vista_General
        JWPLC_Display.setValue(FIELD_GEN_TCOD, valTCod);
        JWPLC_Display.setValue(FIELD_GEN_COD, valCod);
        JWPLC_Display.setValue(FIELD_GEN_BOD, valBod);
        JWPLC_Display.setValue(FIELD_GEN_TOC, valToc);
        JWPLC_Display.setValue(FIELD_GEN_TURB, valTurb);
        JWPLC_Display.setBool(FIELD_GEN_STATE, stGen);
        break;

    case PAGE_TEMP_COD:
        // Página 03 · Temp_COD
        JWPLC_Display.setBool(FIELD_ST_TCOD, stTCod);
        break;

    case PAGE_GRAF_COD:
        // Página 04 · Graf_COD
        JWPLC_Display.setBool(FIELD_ST_COD, stCod);
        break;

    case PAGE_GRAF_BOD:
        // Página 05 · Graf_BOD
        JWPLC_Display.setBool(FIELD_ST_BOD, stBod);
        break;

    case PAGE_GRAF_TOC:
        // Página 06 · Graf_TOC
        JWPLC_Display.setBool(FIELD_ST_TOC, stToc);
        break;

    case PAGE_GRAF_TURB:
        // Página 07 · Graf_TURB
        JWPLC_Display.setBool(FIELD_ST_TURB, stTurb);
        break;

    default:
        break;
    }
}
