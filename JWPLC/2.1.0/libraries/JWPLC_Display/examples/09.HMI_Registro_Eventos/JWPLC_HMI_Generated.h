// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PROCESO = 0,
    PAGE_EVENTOS = 1
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Proceso
    FIELD_BOMBA = 1,
    FIELD_ALARMA = 2,
    FIELD_ARRANQUES = 3,
    FIELD_EVENTOS = 4,
};

// Variables HMI
// Página 01 · Proceso
bool bombaMarcha = false;
bool alarmaNivel = false;
float arranques = 0.0f;
float eventos = 0.0f;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIBoolField(
        FIELD_BOMBA,
        JWPLC_UIRect(6, 30),
        JWPLC_UIText("Bomba", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x4208,
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
    JWPLC_UIBoolField(
        FIELD_ALARMA,
        JWPLC_UIRect(6, 60),
        JWPLC_UIText("Nivel alto", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x4208,
            0xF800),
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
    JWPLC_UIValueField(
        FIELD_ARRANQUES,
        JWPLC_UIRect(6, 90),
        JWPLC_UIText("Arranques", nullptr),
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
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_EVENTOS,
        JWPLC_UIRect(6, 120),
        JWPLC_UIText("Eventos", nullptr),
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
            0xFFFF))
};

// =====================================================
// Registro de Eventos (EVENT_LOG) · Alpha16
// =====================================================
#if JWPLC_UI_HAS_EVENT_LOG
static const JWPLC_UIEventDescriptor HMI_EVENT_DESCRIPTORS[] PROGMEM = {
    { 100, "Arranque del equipo", JWPLC_EVENT_SEV_INFO, JWPLC_EVENT_CAT_SYSTEM },
    { 201, "Bomba en marcha", JWPLC_EVENT_SEV_INFO, JWPLC_EVENT_CAT_PROCESS },
    { 202, "Bomba parada", JWPLC_EVENT_SEV_INFO, JWPLC_EVENT_CAT_PROCESS },
    { 301, "Nivel alto", JWPLC_EVENT_SEV_CRIT, JWPLC_EVENT_CAT_ALARM },
    { 302, "Paro por operador", JWPLC_EVENT_SEV_WARN, JWPLC_EVENT_CAT_INTERLOCK }
};

static const JWPLC_UIEventLog HMI_EVENT_LOG = {
    /* page */ 1,
    /* x */ 0,
    /* y */ 0,
    /* width */ 320,
    /* height */ 170,
    /* frame */ true,
    /* ramCapacity */ 64,
    /* enableFram */ true,
    /* enableSd */ true,
    /* sdFileName */ "/EVENTOS.CSV",
    /* descriptors */ HMI_EVENT_DESCRIPTORS,
    /* descriptorCount */ 5,
    /* colors */ {
        0x2124,
        0xFFFF,
        0x0,
        0x10A2,
        0xC618,
        0x3FF,
        0xFFFF,
        0x7E0,
        0xFD20,
        0xF800,
        0xFFFF
    }
};
#endif

void jwplcHMISetup()
{
    #if JWPLC_UI_HAS_EVENT_LOG
    JWPLC_Display.setEventLog(HMI_EVENT_LOG);
    #endif

    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.setUserPage(PAGE_PROCESO);
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



// Gráfica estática por página (se dibuja una vez al entrar a la página).
// Para lógica propia al cambiar de página, define en el sketch:
//   extern "C" void jwplcHMIUserPageEnter(uint8_t page) { ... }
extern "C" void jwplcHMIUserPageEnter(uint8_t page) __attribute__((weak));

void jwplcUIPageEnter(uint8_t page)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();

    switch (page)
    {
    case PAGE_PROCESO:
        // Estacion de bombeo
        #if JWPLC_HMI_TFT_FONTS
        tft.setFont(JWPLC_FONT_SANS_BOLD_9);
        tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.drawString("Estacion de bombeo", 4, 2);
        tft.setFont(JWPLC_FONT_GLCD);
        #else
        tft.setCursor(4, 2);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("Estacion de bombeo");
        #endif
        break;

    default:
        break;
    }

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
    case PAGE_PROCESO:
        // Página 01 · Proceso
        JWPLC_Display.setBool(FIELD_BOMBA, bombaMarcha);
        JWPLC_Display.setBool(FIELD_ALARMA, alarmaNivel);
        JWPLC_Display.setValue(FIELD_ARRANQUES, arranques);
        JWPLC_Display.setValue(FIELD_EVENTOS, eventos);
        break;

    default:
        break;
    }
}
