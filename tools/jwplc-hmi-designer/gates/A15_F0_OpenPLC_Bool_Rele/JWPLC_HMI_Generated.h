// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_RELE = 0
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Rele
    FIELD_RELE_Q0_0 = 1,
};

// Variables HMI
// Página 01 · Rele
bool rele_q0_0 = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIBoolField(
        FIELD_RELE_Q0_0,
        JWPLC_UIRect(40, 50),
        JWPLC_UIText("Rele Q0.0", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS
        JWPLC_UIBoolText(
            "OFF",
            "ON",
            0xFFFF,
            0x0000,
            0xF800,
            0x07E0),
#else
        JWPLC_UIBoolText(
            "OFF",
            "ON"),
#endif
        JWPLC_UIBoolStyle(
            4,
            2,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x0000,
            0xFFFF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(PAGE_RELE);
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
    case PAGE_RELE:
        // Página 01 · Rele
        JWPLC_Display.setBool(FIELD_RELE_Q0_0, rele_q0_0);
        break;

    default:
        break;
    }
}