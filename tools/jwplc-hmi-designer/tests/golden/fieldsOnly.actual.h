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
    FIELD_MSG = 1,
    FIELD_P = 2,
    FIELD_B = 3,
    FIELD_TANQUE = 4,
};

// Variables HMI
// Página 01 · Principal
char mensaje[33] = {}; // 16 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float presion = 0.0f;
bool valvula = false;
float tanque = 0.0f;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_MSG,
        JWPLC_UIRect(4, 4),
        JWPLC_UIText("Msg", nullptr, 16),
        JWPLC_UITextFieldStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_CENTER),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_P,
        JWPLC_UIRect(4, 40),
        JWPLC_UIText("P", "bar"),
        JWPLC_UIValueFormat(
            2,
            2,
            true,
            true),
        JWPLC_UIValueStyle(
            3,
            2,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_B,
        JWPLC_UIRect(4, 100),
        JWPLC_UIText("Valvula", nullptr),
        JWPLC_UIBoolText(
            "CERRADA",
            "ABIERTA"),
        JWPLC_UIBoolStyle(
            1,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_TANQUE,
        JWPLC_UIRect(160, 4, 150, JWPLC_UI_AUTO),
        JWPLC_UIText("Nivel", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x18C3,
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
        JWPLC_Display.setText(FIELD_MSG, mensaje);
        JWPLC_Display.setValue(FIELD_P, presion);
        JWPLC_Display.setBool(FIELD_B, valvula);
        JWPLC_Display.setBar(FIELD_TANQUE, tanque);
        break;

    default:
        break;
    }
}