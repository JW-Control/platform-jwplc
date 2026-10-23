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
    FIELD_HORA = 1,
    FIELD_FECHA = 2,
    FIELD_TEMP = 3,
};

// Variables HMI
// Página 01 · Principal
float temperatura = 0.0f;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_HORA,
        JWPLC_UIRect(4, 4),
        JWPLC_UIText(nullptr, nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_HH_MM_SS),
        JWPLC_UIValueStyle(
            3,
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_FECHA,
        JWPLC_UIRect(4, 44),
        JWPLC_UIText("Fecha", nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_DD_MM_YYYY),
        JWPLC_UIValueStyle(
            2,
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
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(4, 90),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(
            3,
            1,
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
        JWPLC_Display.setValue(FIELD_TEMP, temperatura);
        break;

    default:
        break;
    }
}