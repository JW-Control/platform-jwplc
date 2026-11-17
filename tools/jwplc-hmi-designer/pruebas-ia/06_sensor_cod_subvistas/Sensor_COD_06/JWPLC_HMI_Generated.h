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
    FIELD_V2_VAL_TAG = 2,
    FIELD_V2_VAL_TIME = 3,
    FIELD_V2_STATUS_TEXT = 4,
    FIELD_V2_PROGRESS_BAR = 5,
    FIELD_V2_SUBTEXT = 6,
    FIELD_V3_WIFI_STATE = 7,

    // Página 02 · Vista_General
    FIELD_GEN_TCOD = 8,
    FIELD_GEN_COD = 9,
    FIELD_GEN_BOD = 10,
    FIELD_GEN_TOC = 11,
    FIELD_GEN_TURB = 12,
    FIELD_GEN_STATE = 13,

    // Página 03 · Temp_COD
    FIELD_ST_REP = 14,
    FIELD_TREND_TCOD = 15,
    FIELD_VALUE_9 = 16,

    // Página 04 · Graf_COD
    FIELD_TREND_COD = 17,
    FIELD_VALUE_10 = 18,

    // Página 05 · Graf_BOD
    FIELD_TREND_BOD = 19,
    FIELD_VALUE_11 = 20,

    // Página 06 · Graf_TOC
    FIELD_TREND_TOC = 21,
    FIELD_VALUE_12 = 22,

    // Página 07 · Graf_TURB
    FIELD_TREND_TURB = 23,
    FIELD_VALUE_37 = 24,
};

// Variables HMI
// Página 01 · Medicion
char measTagText[7] = {}; // 3 caracteres (UTF-8: cada tilde ocupa 2 bytes)
uint32_t measElapsed = 0;
char measStatus[17] = {}; // 8 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float measProgress = 0.0f;
char measSubtext[73] = {}; // 36 caracteres (UTF-8: cada tilde ocupa 2 bytes)
bool wifiDownload = false;

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
int32_t measTag = 1;
int32_t measMins = 5;
bool popupAvisoReactivo = false;
bool popupFallaSensor = false;

// Ventanas emergentes (I5-I6)
bool measRun = false;
bool popupFinMedicion = false;

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
            0xFFFF)),
    JWPLC_UIFieldViews(
        JWPLC_UITextField(
            FIELD_V2_VAL_TAG,
            JWPLC_UIRect(34, 52),
            JWPLC_UIText(nullptr, nullptr, 3),
            JWPLC_UITextFieldStyle(
                3,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_LEFT),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0x07FF,
                0x0000,
                0xFFFF)),
        0x02U),
    JWPLC_UIFieldViews(
        JWPLC_UIValueField(
            FIELD_V2_VAL_TIME,
            JWPLC_UIRect(184, 24),
            JWPLC_UIText(nullptr, nullptr),
            JWPLC_UIDurationFormat(
                JWPLC_UI_DURATION_MM_SS,
                JWPLC_UI_DURATION_SECONDS,
                2,
                false),
            JWPLC_UIValueStyle(
                3,
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
        0x02U),
    JWPLC_UIFieldViews(
        JWPLC_UITextField(
            FIELD_V2_STATUS_TEXT,
            JWPLC_UIRect(181, 71),
            JWPLC_UIText(nullptr, nullptr, 8),
            JWPLC_UITextFieldStyle(
                2,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_CENTER),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0xFFFF,
                0xFD00,
                0xFFFF)),
        0x02U),
    // Barra sólida (HORIZONTAL) - Radio: 0, Borde: 0xFFFF (1px)
    JWPLC_UIFieldViews(
        JWPLC_UIBarField(
            FIELD_V2_PROGRESS_BAR,
            JWPLC_UIRect(12, 112, 296, JWPLC_UI_AUTO),
            JWPLC_UIText(nullptr, nullptr),
            JWPLC_UIRange(0.0f, 100.0f),
            JWPLC_UIBarStyle(
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0x07E0,
                0x0000,
                0xFFFF),
            JWPLC_UIBarConfig(
                JWPLC_UI_BAR_HORIZONTAL,
                JWPLC_UI_BAR_STYLE_SOLID,
                0,
                0,
                true,
                0xFFFF,
                1,
                10,
                0)),
        0x02U),
    JWPLC_UIFieldViews(
        JWPLC_UITextField(
            FIELD_V2_SUBTEXT,
            JWPLC_UIRect(14, 128),
            JWPLC_UIText(nullptr, nullptr, 36),
            JWPLC_UITextFieldStyle(
                1,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_LEFT),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0xFFFF,
                0x0000,
                0xFFFF)),
        0x02U),
    JWPLC_UIFieldViews(
        JWPLC_UIBoolField(
            FIELD_V3_WIFI_STATE,
            JWPLC_UIRect(18, 42),
            JWPLC_UIText("AP:", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
            JWPLC_UIBoolText(
                "[INACTIVO]",
                "[ACTIVO]  ",
                0xB5D8,
                0x07E0,
                0x18E3,
                0x18E3),
#else
            JWPLC_UIBoolText(
                "[INACTIVO]",
                "[ACTIVO]  "),
#endif
            JWPLC_UIBoolStyle(
                1,
                1,
                false,
                JWPLC_UI_LAYOUT_INLINE,
                JWPLC_UI_ALIGN_LEFT),
            0,
            JWPLC_UIColors(
                0xFFFF,
                0x07E0,
                0x18E3,
                0x39E7)),
        0x04U)
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
    MENU_MEDICION_AVISO_REACTIVO = 4,
    MENU_MEDICION_FALLA_SENSOR = 5,
};

static const JWPLC_UIMenuNode HMI_MENU_MEDICION_NODES[] =
{
    JWPLC_UIMenuNodeConfirm(JWPLC_UIMenuNodePill(JWPLC_UIMenuGoView(JWPLC_UI_MENU_ROOT, "INICIAR MEDICION", 1), 0x05E0, 0xD800, 0xFFFF), 0),
    JWPLC_UIMenuNodeLRStep(JWPLC_UIMenuNodeFormat(JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "ETIQUETA:", &measTag, 1, 99, 1, nullptr), "P", 2), 1),
    JWPLC_UIMenuNodeLRStep(JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "TIEMPO:", &measMins, 1, 60, 1, "min"), 1),
    JWPLC_UIMenuNodeTargetView(JWPLC_UIMenuNodeDirectToggle(JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "DESCARGA WIFI", &wifiDownload, "", "ACTIVA"), true), 2),
    JWPLC_UIMenuNodeDirectToggle(JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "AVISO REACTIVO", &popupAvisoReactivo, "OFF", "ON"), true),
    JWPLC_UIMenuNodeDirectToggle(JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "FALLA SENSOR", &popupFallaSensor, "OFF", "ON"), true)
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuViews(
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
            4, 4, 5, 8, true, true, 0x18E3, 0x39E7),
        0x01U)
};

// Ventanas emergentes (I5-I6): requiere JWPLC_Display con soporte de Popups.
#ifndef JWPLC_UI_HAS_POPUP
#error "Las ventanas emergentes requieren JWPLC_Display con soporte I5-I6."
#endif

enum HMIPopupId : uint8_t
{
    POPUP_CONFIRM_MEDICION = 0,
    POPUP_FIN_MEDICION = 1,
    POPUP_AVISO_REACTIVO = 2,
    POPUP_ERROR_SENSOR = 3
};

static const JWPLC_UIPopup HMI_POPUPS[] =
{
    JWPLC_UIPopupTextSize(
            JWPLC_UIPopupNavTarget(
            JWPLC_UIPopupTrigger(
            JWPLC_UIPopupConfirm(POPUP_CONFIRM_MEDICION, "INICIAR MEDICION", "Desea iniciar el ciclo de medicion de COD?", &measRun, "CANCELAR", "INICIAR"),
            nullptr, true, false),
            PAGE_MEDICION, 1),
            2),
    JWPLC_UIPopupOptions(
            JWPLC_UIPopupTrigger(
            JWPLC_UIPopupInfo(POPUP_FIN_MEDICION, "CICLO COMPLETO", "Medicion finalizada con exito. Muestra guardada.", nullptr),
            &popupFinMedicion, false, false),
            3500, false, false),
    JWPLC_UIPopupButtons(JWPLC_UIPopupTrigger(
            JWPLC_UIPopupWarn(POPUP_AVISO_REACTIVO, "NIVEL REACTIVO", "Nivel de reactivo bajo. Se recomienda recarga pronto.", nullptr),
            &popupAvisoReactivo, false, true), "ENTENDIDO"),
    JWPLC_UIPopupButtons(JWPLC_UIPopupTrigger(
            JWPLC_UIPopupCrit(POPUP_ERROR_SENSOR, "FALLA SENSOR COD", "Error optico: senal fuera de rango o cubeta sucia.", nullptr, "E104"),
            &popupFallaSensor, true, true), "RESET")
};

void jwplcHMISetup()
{
    JWPLC_Display.setPopups(
        HMI_POPUPS,
        sizeof(HMI_POPUPS) / sizeof(HMI_POPUPS[0]));
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

static void jwplcHMIDrawQR(JWPLC_TFTClass &tft,
    int16_t x, int16_t y, uint8_t size, uint8_t scale, uint8_t quiet,
    uint16_t fg, uint16_t bg, bool frame, uint16_t frameColor, const uint8_t *bits)
{
    const int16_t pad = (frame && quiet == 0) ? 1 : 0;
    const int16_t qrSide = (int16_t)((size + 2 * quiet) * scale);
    const int16_t totalSide = (int16_t)(qrSide + pad * 2);
    tft.fillRect(x, y, totalSide, totalSide, bg);
    const int16_t ox = (int16_t)(x + pad + quiet * scale);
    const int16_t oy = (int16_t)(y + pad + quiet * scale);
    for (uint8_t my = 0; my < size; my++)
    {
        uint8_t mx = 0;
        while (mx < size)
        {
            const uint16_t idx = (uint16_t)my * size + mx;
            const bool dark = (bits[idx >> 3] & (0x80u >> (idx & 7))) != 0;
            if (!dark)
            {
                mx++;
                continue;
            }
            uint8_t run = 1;
            while ((uint8_t)(mx + run) < size)
            {
                const uint16_t nidx = (uint16_t)my * size + (uint8_t)(mx + run);
                if ((bits[nidx >> 3] & (0x80u >> (nidx & 7))) == 0) break;
                run++;
            }
            tft.fillRect((int16_t)(ox + mx * scale), (int16_t)(oy + my * scale),
                         (int16_t)(run * scale), scale, fg);
            mx = (uint8_t)(mx + run);
        }
    }
    if (frame)
    {
        tft.drawRect(x, y, totalSide, totalSide, frameColor);
    }
}

// QR estático FIELD_V3_QR_WIFI (29x29, v3, nivel undefined, 106 B)
static const uint8_t QR_FIELD_V3_QR_WIFI_BITS[] =
{
    0xFE, 0x5E, 0x4B, 0xFC, 0x11, 0x6A, 0x10, 0x6E, 0xA8, 0xC2, 0xBB, 0x75, 0x69, 0x05, 0xDB, 0xAC,
    0x62, 0xAE, 0xC1, 0x7A, 0xD5, 0x07, 0xFA, 0xAA, 0xAF, 0xE0, 0x15, 0x53, 0x00, 0xBE, 0x31, 0x93,
    0xE2, 0x4B, 0xE0, 0x55, 0xBB, 0xFF, 0xCB, 0xD0, 0xCB, 0xD9, 0x6D, 0x36, 0x6E, 0x3E, 0x1E, 0x6E,
    0xA4, 0x26, 0xDB, 0xFA, 0x87, 0xBC, 0x80, 0xBA, 0x73, 0xC8, 0x5F, 0x41, 0x99, 0x5F, 0x6E, 0xB0,
    0x46, 0xAB, 0xC5, 0xCA, 0x61, 0x40, 0x3B, 0x65, 0x19, 0xAE, 0xBE, 0xFE, 0x00, 0x50, 0x24, 0x53,
    0xF8, 0xC7, 0xAA, 0x90, 0x5E, 0x47, 0x1B, 0xBA, 0xB1, 0xEF, 0xC5, 0xD4, 0x50, 0x06, 0xEE, 0xB3,
    0x4E, 0x7D, 0x04, 0x38, 0x09, 0xAF, 0xEB, 0x9F, 0x4C, 0x00
};

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
            tft.fillRoundRect(7, 149, 42, 20, 3, 0x06DF);
            tft.setCursor(19, 155);
            tft.setTextColor(0x0000);
            tft.print("CFG");
        }
        else
        {
            tft.fillRoundRect(7, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(7, 149, 42, 20, 3, 0x030E);
            tft.drawRoundRect(9, 151, 38, 16, 1, 0x030E);
            tft.setCursor(19, 155);
            tft.setTextColor(0x030E);
            tft.print("CFG");
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(51, 149, 42, 20, 3, 0x7BEF);
            tft.setCursor(63, 155);
            tft.setTextColor(0xFFFF);
            tft.print("GEN");
        }
        else
        {
            tft.fillRoundRect(51, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(51, 149, 42, 20, 3, 0x738E);
            tft.drawRoundRect(53, 151, 38, 16, 1, 0x738E);
            tft.setCursor(63, 155);
            tft.setTextColor(0x738E);
            tft.print("GEN");
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(95, 149, 42, 20, 3, 0xEBBE);
            tft.setCursor(104, 155);
            tft.setTextColor(0x0000);
            tft.print("T_CD");
        }
        else
        {
            tft.fillRoundRect(95, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(95, 149, 42, 20, 3, 0x69AD);
            tft.drawRoundRect(97, 151, 38, 16, 1, 0x69AD);
            tft.setCursor(104, 155);
            tft.setTextColor(0x69AD);
            tft.print("T_CD");
        }
        break;
    case 3:
        if (selecting)
        {
            tft.fillRoundRect(139, 149, 42, 20, 3, 0xFF00);
            tft.setCursor(151, 155);
            tft.setTextColor(0x0000);
            tft.print("COD");
        }
        else
        {
            tft.fillRoundRect(139, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(139, 149, 42, 20, 3, 0x7320);
            tft.drawRoundRect(141, 151, 38, 16, 1, 0x7320);
            tft.setCursor(151, 155);
            tft.setTextColor(0x7320);
            tft.print("COD");
        }
        break;
    case 4:
        if (selecting)
        {
            tft.fillRoundRect(183, 149, 42, 20, 3, 0x073F);
            tft.setCursor(195, 155);
            tft.setTextColor(0x0000);
            tft.print("BOD");
        }
        else
        {
            tft.fillRoundRect(183, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(183, 149, 42, 20, 3, 0x032E);
            tft.drawRoundRect(185, 151, 38, 16, 1, 0x032E);
            tft.setCursor(195, 155);
            tft.setTextColor(0x032E);
            tft.print("BOD");
        }
        break;
    case 5:
        if (selecting)
        {
            tft.fillRoundRect(227, 149, 42, 20, 3, 0x072E);
            tft.setCursor(239, 155);
            tft.setTextColor(0x0000);
            tft.print("TOC");
        }
        else
        {
            tft.fillRoundRect(227, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(227, 149, 42, 20, 3, 0x0346);
            tft.drawRoundRect(229, 151, 38, 16, 1, 0x0346);
            tft.setCursor(239, 155);
            tft.setTextColor(0x0346);
            tft.print("TOC");
        }
        break;
    case 6:
        if (selecting)
        {
            tft.fillRoundRect(271, 149, 42, 20, 3, 0xFC40);
            tft.setCursor(283, 155);
            tft.setTextColor(0x0000);
            tft.print("TUR");
        }
        else
        {
            tft.fillRoundRect(271, 149, 42, 20, 3, 0xFFFF);
            tft.drawRoundRect(271, 149, 42, 20, 3, 0x71E0);
            tft.drawRoundRect(273, 151, 38, 16, 1, 0x71E0);
            tft.setCursor(283, 155);
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
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 1:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 2:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 3:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 4:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
        tft.setTextColor(0x072E);
        tft.print("TOC");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 5:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(271, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(271, 149, 42, 20, 3, 0xFC40);
        tft.setCursor(283, 155);
        tft.setTextColor(0xFC40);
        tft.print("TUR");
        break;
    case 6:
        tft.fillRoundRect(0, 148, 320, 22, 3, 0x0000);
        tft.drawFastHLine(0, 148, 320, 0x21E9);
        tft.fillRoundRect(7, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(7, 149, 42, 20, 3, 0x06DF);
        tft.setCursor(19, 155);
        tft.setTextColor(0x06DF);
        tft.print("CFG");
        tft.fillRoundRect(51, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(51, 149, 42, 20, 3, 0xFFFF);
        tft.setCursor(63, 155);
        tft.setTextColor(0xFFFF);
        tft.print("GEN");
        tft.fillRoundRect(95, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(95, 149, 42, 20, 3, 0xEBBE);
        tft.setCursor(104, 155);
        tft.setTextColor(0xEBBE);
        tft.print("T_CD");
        tft.fillRoundRect(139, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(139, 149, 42, 20, 3, 0xFF00);
        tft.setCursor(151, 155);
        tft.setTextColor(0xFF00);
        tft.print("COD");
        tft.fillRoundRect(183, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(183, 149, 42, 20, 3, 0x073F);
        tft.setCursor(195, 155);
        tft.setTextColor(0x073F);
        tft.print("BOD");
        tft.fillRoundRect(227, 149, 42, 20, 3, 0x0000);
        tft.drawRoundRect(227, 149, 42, 20, 3, 0x072E);
        tft.setCursor(239, 155);
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
        if (JWPLC_Display.pageView(PAGE_MEDICION) == 1)
        {
            // Etiqueta Label
            tft.setCursor(20, 28);
            tft.setTextSize(2);
            tft.setTextColor(0xFFFF);
            tft.print("ETIQUETA:");
            // Rectangulo Estado
            tft.fillRoundRect(160, 62, 138, 34, 6, 0xFD00);
            tft.drawRoundRect(160, 62, 138, 34, 6, 0xFFFF);
        }
        // CONFIGURACION DE MEDICION
        tft.setCursor(10, 5);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("CONFIGURACION DE MEDICION");
        if (JWPLC_Display.pageView(PAGE_MEDICION) == 2)
        {
            // Titulo WiFi QR
            tft.setCursor(12, 20);
            tft.setTextSize(1);
            tft.setTextColor(0x07FF);
            tft.print("PORTAL DESCARGA WIFI");
            // Panel Datos WiFi
            tft.fillRoundRect(10, 34, 184, 90, 5, 0x18E3);
            tft.drawRoundRect(10, 34, 184, 90, 5, 0x39E7);
            // SSID WiFi
            tft.setCursor(18, 60);
            tft.setTextSize(1);
            tft.setTextColor(0xFFFF);
            tft.print("RED:  JWPLC_COD_AP");
            // Clave WiFi
            tft.setCursor(18, 75);
            tft.setTextSize(1);
            tft.setTextColor(0xFFFF);
            tft.print("PASS: 12345678");
            // IP WiFi
            tft.setCursor(18, 90);
            tft.setTextSize(1);
            tft.setTextColor(0xFFE0);
            tft.print("IP:   192.168.4.1");
            // Nota QR Prueba
            tft.setCursor(18, 108);
            tft.setTextSize(1);
            tft.setTextColor(0xB5D8);
            tft.print("QR generico de prueba");
            // Ayuda Volver V3
            tft.setCursor(12, 132);
            tft.setTextSize(1);
            tft.setTextColor(0xB5D8);
            tft.print("OK: Volver a Ajustes   ESC: Inicio");
            // QR WiFi Prueba
            jwplcHMIDrawQR(tft, 204, 22, 29, 3, 2, 0x0000, 0xFFFF, true, 0x07FF, QR_FIELD_V3_QR_WIFI_BITS);
        }
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
    case PAGE_MEDICION:
        // Página 01 · Medicion
        JWPLC_Display.setText(FIELD_V2_VAL_TAG, measTagText);
        JWPLC_Display.setValue(FIELD_V2_VAL_TIME, measElapsed);
        JWPLC_Display.setText(FIELD_V2_STATUS_TEXT, measStatus);
        JWPLC_Display.setBar(FIELD_V2_PROGRESS_BAR, measProgress);
        JWPLC_Display.setText(FIELD_V2_SUBTEXT, measSubtext);
        JWPLC_Display.setBool(FIELD_V3_WIFI_STATE, wifiDownload);
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
