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
    FIELD_RTC = 1,

    // Página 02 · Vista_General
    FIELD_GEN_TCOD = 2,
    FIELD_GEN_COD = 3,
    FIELD_GEN_BOD = 4,
    FIELD_GEN_TOC = 5,
    FIELD_GEN_TURB = 6,
    FIELD_GEN_STATE = 7,

    // Página 03 · Temp_COD
    FIELD_ST_REP = 8,
    FIELD_TREND_TCOD = 9,
    FIELD_VALUE_9 = 10,

    // Página 04 · Graf_COD
    FIELD_TREND_COD = 11,
    FIELD_VALUE_10 = 12,

    // Página 05 · Graf_BOD
    FIELD_TREND_BOD = 13,
    FIELD_VALUE_11 = 14,

    // Página 06 · Graf_TOC
    FIELD_TREND_TOC = 15,
    FIELD_VALUE_12 = 16,

    // Página 07 · Graf_TURB
    FIELD_TREND_TURB = 17,
    FIELD_VALUE_37 = 18,
};

// Variables HMI
// Página 02 · Vista_General
float valTCod = 0.0f;
float valCod = 0.0f;
float valBod = 0.0f;
float valToc = 0.0f;
float valTurb = 0.0f;
bool stGen = false;

// Página 03 · Temp_COD
bool stRep = false;
float trendTCod = 0.0f;

// Página 04 · Graf_COD
float trendCod = 0.0f;

// Página 05 · Graf_BOD
float trendBod = 0.0f;

// Página 06 · Graf_TOC
float trendToc = 0.0f;

// Página 07 · Graf_TURB
float trendTurb = 0.0f;

// MENU MENU_MEDICION
bool measRun = false;
int32_t measTag = 1;
int32_t measMins = 5;
bool wifiDownload = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIFieldPages(
        JWPLC_UIValueField(
            FIELD_RTC,
            JWPLC_UIRect(218, 0),
            JWPLC_UIText(nullptr, nullptr),
            JWPLC_UIDateTimeFormat(
                JWPLC_UI_DATETIME_HH_MM_SS),
            JWPLC_UIValueStyle(
                2,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_RIGHT),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0xFFFF,
                0x0000,
                0xFFFF)),
        0x007FU),
    JWPLC_UIFieldPages(
        JWPLC_UIBoolField(
            FIELD_ST_REP,
            JWPLC_UIRect(176, 5),
            JWPLC_UIText(nullptr, nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
            JWPLC_UIBoolText(
                "[REP]",
                "[MID]",
                0xFFE0,
                0x07E0,
                0x0000,
                0x0000),
#else
            JWPLC_UIBoolText(
                "[REP]",
                "[MID]"),
#endif
            JWPLC_UIBoolStyle(
                1,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_CENTER),
            2,
            JWPLC_UIColors(
                0xFFFF,
                0xFFE0,
                0x0000,
                0x530E)),
        0x007CU),
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
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xF81F,
            0x0000,
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
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFF00,
            0x0000,
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
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
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
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x0000,
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
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFC40,
            0x0000,
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
            false,
            "SP",
            "CH3", false),
        2,
        JWPLC_UIColors(
            0xFFFF,
            0xF81F,
            0x0000,
            0x4208)),
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
            false,
            "SP",
            "CH3", false),
        3,
        JWPLC_UIColors(
            0xFFFF,
            0xFF00,
            0x0000,
            0x4208)),
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
            false,
            "SP",
            "CH3", false),
        4,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0x4208)),
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
            false,
            "SP",
            "CH3", false),
        5,
        JWPLC_UIColors(
            0xFFFF,
            0x07E0,
            0x0000,
            0x4208)),
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
            false,
            "SP",
            "CH3", false),
        6,
        JWPLC_UIColors(
            0xFFFF,
            0xFC40,
            0x0000,
            0x4208)),
    JWPLC_UIValueField(
        FIELD_VALUE_9,
        JWPLC_UIRect(6, 0),
        JWPLC_UIText("T_CD", "C"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            2,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        2,
        JWPLC_UIColors(
            0xF81F,
            0xF81F,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VALUE_10,
        JWPLC_UIRect(6, 0),
        JWPLC_UIText("COD", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            2,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        3,
        JWPLC_UIColors(
            0xFF00,
            0xFF00,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VALUE_11,
        JWPLC_UIRect(6, 0),
        JWPLC_UIText("BOD", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            2,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        4,
        JWPLC_UIColors(
            0x07FF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VALUE_12,
        JWPLC_UIRect(6, 0),
        JWPLC_UIText("TOC", "mg/L"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            2,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        5,
        JWPLC_UIColors(
            0x07E0,
            0x07E0,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_VALUE_37,
        JWPLC_UIRect(6, 0),
        JWPLC_UIText("TURB", "NTU"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            2,
            2,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        6,
        JWPLC_UIColors(
            0xFC40,
            0xFC40,
            0x0000,
            0xFFFF))
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
    MENU_MEDICION_INICIAR_MEDICION = 0,
    MENU_MEDICION_ETIQUETA_ = 1,
    MENU_MEDICION_TIEMPO_ = 2,
    MENU_MEDICION_DESCARGA_WIFI = 3,
};

static const JWPLC_UIMenuNode HMI_MENU_MEDICION_NODES[] =
{
    JWPLC_UIMenuNodePill(JWPLC_UIMenuNodeDirectToggle(JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "", &measRun, "INICIAR MEDICION", "DETENER MEDICION"), true), 0x05E0, 0xD800, 0xFFFF),
    JWPLC_UIMenuNodeLRStep(JWPLC_UIMenuNodeFormat(JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "ETIQUETA:", &measTag, 1, 99, 1, nullptr), "P", 2), 1),
    JWPLC_UIMenuNodeLRStep(JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "TIEMPO:", &measMins, 1, 60, 1, "min"), 1),
    JWPLC_UIMenuNodeDirectToggle(JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "DESCARGA WIFI", &wifiDownload, "", "ACTIVA"), true)
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuCardStyle(
        JWPLC_UIMenuField(
            MENU_MEDICION,
            11, 22, 298, 122,
            nullptr,
            HMI_MENU_MEDICION_NODES,
            sizeof(HMI_MENU_MEDICION_NODES) / sizeof(HMI_MENU_MEDICION_NODES[0]),
            PAGE_MEDICION,
            2,
            false,
            JWPLC_UIMenuColors(
                0xFFFF,
                0x0000,
                0xFFFF,
                0x07E0,
                0x07E0,
                0x0000,
                0xFD20,
                0xFFFF,
                0x0000,
                0x39E7)),
        4, 4, 5, 8, true, true, 0x18E3, 0x39E7)
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
            tft.fillRoundRect(8, 149, 42, 20, 3, 0x06DF);
            tft.setCursor(20, 155);
            tft.setTextColor(0x0000);
            tft.print("CFG");
        }
        else
        {
            tft.fillRoundRect(8, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(8, 149, 42, 20, 3, 0x030E);
            tft.drawRoundRect(10, 151, 38, 16, 1, 0x030E);
            tft.setCursor(20, 155);
            tft.setTextColor(0x030E);
            tft.print("CFG");
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(52, 149, 42, 20, 3, 0x7BEF);
            tft.setCursor(64, 155);
            tft.setTextColor(0xFFFF);
            tft.print("GEN");
        }
        else
        {
            tft.fillRoundRect(52, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(52, 149, 42, 20, 3, 0x738E);
            tft.drawRoundRect(54, 151, 38, 16, 1, 0x738E);
            tft.setCursor(64, 155);
            tft.setTextColor(0x738E);
            tft.print("GEN");
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(96, 149, 42, 20, 3, 0xEBBE);
            tft.setCursor(105, 155);
            tft.setTextColor(0x0000);
            tft.print("T_CD");
        }
        else
        {
            tft.fillRoundRect(96, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(96, 149, 42, 20, 3, 0x69AD);
            tft.drawRoundRect(98, 151, 38, 16, 1, 0x69AD);
            tft.setCursor(105, 155);
            tft.setTextColor(0x69AD);
            tft.print("T_CD");
        }
        break;
    case 3:
        if (selecting)
        {
            tft.fillRoundRect(140, 149, 42, 20, 3, 0xFF00);
            tft.setCursor(152, 155);
            tft.setTextColor(0x0000);
            tft.print("COD");
        }
        else
        {
            tft.fillRoundRect(140, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(140, 149, 42, 20, 3, 0x7320);
            tft.drawRoundRect(142, 151, 38, 16, 1, 0x7320);
            tft.setCursor(152, 155);
            tft.setTextColor(0x7320);
            tft.print("COD");
        }
        break;
    case 4:
        if (selecting)
        {
            tft.fillRoundRect(184, 149, 42, 20, 3, 0x073F);
            tft.setCursor(196, 155);
            tft.setTextColor(0x0000);
            tft.print("BOD");
        }
        else
        {
            tft.fillRoundRect(184, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(184, 149, 42, 20, 3, 0x032E);
            tft.drawRoundRect(186, 151, 38, 16, 1, 0x032E);
            tft.setCursor(196, 155);
            tft.setTextColor(0x032E);
            tft.print("BOD");
        }
        break;
    case 5:
        if (selecting)
        {
            tft.fillRoundRect(228, 149, 42, 20, 3, 0x072E);
            tft.setCursor(240, 155);
            tft.setTextColor(0x0000);
            tft.print("TOC");
        }
        else
        {
            tft.fillRoundRect(228, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(228, 149, 42, 20, 3, 0x0346);
            tft.drawRoundRect(230, 151, 38, 16, 1, 0x0346);
            tft.setCursor(240, 155);
            tft.setTextColor(0x0346);
            tft.print("TOC");
        }
        break;
    case 6:
        if (selecting)
        {
            tft.fillRoundRect(272, 149, 42, 20, 3, 0xFC40);
            tft.setCursor(284, 155);
            tft.setTextColor(0x0000);
            tft.print("TUR");
        }
        else
        {
            tft.fillRoundRect(272, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(272, 149, 42, 20, 3, 0x71E0);
            tft.drawRoundRect(274, 151, 38, 16, 1, 0x71E0);
            tft.setCursor(284, 155);
            tft.setTextColor(0x71E0);
            tft.print("TUR");
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
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 1:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 2:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 3:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 4:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 5:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(272, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(272, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(284, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 6:
        tft.fillRoundRect(1, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(1, 148, 320, 0x21E9);
        tft.fillRoundRect(8, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(8, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(20, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(52, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(52, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(64, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(96, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(96, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(105, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(140, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(140, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(152, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(184, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(184, 149, 42, 20, 3, 0x073F);
        tft.setCursor(196, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(228, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(228, 149, 42, 20, 3, 0x072E);
        tft.setCursor(240, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
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
        // CONFIGURACION DE MEDICION
        tft.setCursor(10, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("CONFIGURACION DE MEDICION");
        break;

    case PAGE_VISTA_GENERAL:
        // VISTA GENERAL
        tft.setCursor(4, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("VISTA GENERAL");
        // Rect 1
        tft.drawRoundRect(3, 21, 136, 33, 4, 0xF81F);
        // Rect 2
        tft.drawRoundRect(3, 61, 136, 35, 4, 0x07DF);
        // Rect 3
        tft.drawRoundRect(4, 103, 136, 35, 4, 0xFCC0);
        // Rect 4
        tft.drawRoundRect(162, 20, 136, 34, 4, 0xFFE0);
        // Rect 5
        tft.drawRoundRect(162, 62, 136, 34, 4, 0x07E3);
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
        JWPLC_Display.setBool(FIELD_ST_REP, stRep);
        JWPLC_Display.setValue(FIELD_VALUE_9, trendTCod);
        break;

    case PAGE_GRAF_COD:
        // Página 04 · Graf_COD
        JWPLC_Display.setBool(FIELD_ST_REP, stRep);
        JWPLC_Display.setValue(FIELD_VALUE_10, trendCod);
        break;

    case PAGE_GRAF_BOD:
        // Página 05 · Graf_BOD
        JWPLC_Display.setBool(FIELD_ST_REP, stRep);
        JWPLC_Display.setValue(FIELD_VALUE_11, trendBod);
        break;

    case PAGE_GRAF_TOC:
        // Página 06 · Graf_TOC
        JWPLC_Display.setBool(FIELD_ST_REP, stRep);
        JWPLC_Display.setValue(FIELD_VALUE_12, trendToc);
        break;

    case PAGE_GRAF_TURB:
        // Página 07 · Graf_TURB
        JWPLC_Display.setBool(FIELD_ST_REP, stRep);
        JWPLC_Display.setValue(FIELD_VALUE_37, trendTurb);
        break;

    default:
        break;
    }
}
