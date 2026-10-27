// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_VALORES = 0
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Valores
    FIELD_RELE_Q0_0 = 1,
    FIELD_CONTADOR = 2,
    FIELD_NIVEL = 3,
};

// Variables HMI
// Página 01 · Valores
bool rele_q0_0 = false;
float contador = 0.0f;
float nivel = 0.0f;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIBoolField(
        FIELD_RELE_Q0_0,
        JWPLC_UIRect(8, 6),
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
            2,
            2,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_CONTADOR,
        JWPLC_UIRect(8, 52),
        JWPLC_UIText("Contador", nullptr),
        JWPLC_UIValueFormat(
            5,
            0,
            true,
            false),
        JWPLC_UIValueStyle(
            3,
            2,
            true,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    // Barra Sólida (HORIZONTAL) - Estilo: SOLID, Radio: 0
    JWPLC_UIBarField(
        FIELD_NIVEL,
        JWPLC_UIRect(8, 105, 300, JWPLC_UI_AUTO),
        JWPLC_UIText("Nivel", nullptr),
        JWPLC_UIRange(0.0f, 10.0f),
        JWPLC_UIBarStyle(
            2,
            true,
            JWPLC_UI_LAYOUT_STACKED),
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
    JWPLC_Display.setUserPage(PAGE_VALORES);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
}

// Sincronización gráfica autogenerada (cada tick de refresco).
// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.
// La lógica de aplicación debe permanecer en loop().
void jwplcUIUpdate()
{
    switch (JWPLC_Display.userPage())
    {
    case PAGE_VALORES:
        // Página 01 · Valores
        JWPLC_Display.setBool(FIELD_RELE_Q0_0, rele_q0_0);
        JWPLC_Display.setValue(FIELD_CONTADOR, contador);
        JWPLC_Display.setBar(FIELD_NIVEL, nivel);
        break;

    default:
        break;
    }
}