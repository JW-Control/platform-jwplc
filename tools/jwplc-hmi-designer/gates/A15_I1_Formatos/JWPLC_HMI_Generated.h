// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PRINCIPAL = 0
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Principal
    FIELD_PIEZAS = 1,
    FIELD_ENERGIA = 2,
    FIELD_RESTANTE = 3,
    FIELD_MARCHA = 4,
    FIELD_CICLO = 5,
    FIELD_BOMBA = 6,
    FIELD_ALARMA = 7,
};

// Variables HMI
// Página 01 · Principal
uint32_t piezas = 0;
float energia = 0.0f;
int32_t t_restante = 0;
uint32_t marcha_ms = 0;
uint32_t ciclo = 0;
bool bomba = false;
bool alarma = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_PIEZAS,
        JWPLC_UIRect(4, 4),
        JWPLC_UIText("Piezas", nullptr),
        JWPLC_UIValueFormat(
            7,
            0,
            false,
            false,
            ','),
        JWPLC_UIValueStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_ENERGIA,
        JWPLC_UIRect(4, 40),
        JWPLC_UIText("kWh", nullptr),
        JWPLC_UIValueFormat(
            6,
            1,
            false,
            false,
            '.'),
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
        FIELD_RESTANTE,
        JWPLC_UIRect(4, 76),
        JWPLC_UIText("Resta", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_MM_SS,
            JWPLC_UI_DURATION_SECONDS,
            2,
            true),
        JWPLC_UIValueStyle(
            3,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_MARCHA,
        JWPLC_UIRect(160, 76),
        JWPLC_UIText("Marcha", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_HH_MM_SS,
            JWPLC_UI_DURATION_MILLIS,
            3,
            false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_CICLO,
        JWPLC_UIRect(160, 130),
        JWPLC_UIText("Ciclo", nullptr),
        JWPLC_UIDurationFormat(
            JWPLC_UI_DURATION_COMPACT,
            JWPLC_UI_DURATION_SECONDS,
            2,
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
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_BOMBA,
        JWPLC_UIRect(4, 140),
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
        JWPLC_UIRect(90, 140),
        JWPLC_UIText("Alarma", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x4208,
            0xFFE0,
            0x0000,
            0xF800),
#else
        JWPLC_UIBoolText(
            "OFF",
            "ON"),
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
            0xFFFF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
}

// Sincronización gráfica autogenerada (cada tick de refresco).
// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.
// La lógica de aplicación debe permanecer en loop().
void jwplcUIUpdate()
{
    switch (JWPLC_Display.userPage())
    {
    case PAGE_PRINCIPAL:
        // Página 01 · Principal
        JWPLC_Display.setValue(FIELD_PIEZAS, piezas);
        JWPLC_Display.setValue(FIELD_ENERGIA, energia);
        JWPLC_Display.setValue(FIELD_RESTANTE, t_restante);
        JWPLC_Display.setValue(FIELD_MARCHA, marcha_ms);
        JWPLC_Display.setValue(FIELD_CICLO, ciclo);
        JWPLC_Display.setBool(FIELD_BOMBA, bomba);
        JWPLC_Display.setBool(FIELD_ALARMA, alarma);
        break;

    default:
        break;
    }
}