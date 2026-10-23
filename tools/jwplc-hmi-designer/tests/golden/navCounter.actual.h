// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PRINCIPAL = 0,
    PAGE_PAGINA_2 = 1
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Principal
    FIELD_T = 1,
};

// Variables HMI
// Página 01 · Principal
char t[13] = {}; // 6 caracteres (UTF-8: cada tilde ocupa 2 bytes)

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_T,
        JWPLC_UIRect(4, 20),
        JWPLC_UIText("T", nullptr, 6),
        JWPLC_UITextFieldStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
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
    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
    // Indicador NN/TT (barra de navegación)
#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG
    JWPLC_Display.setPageIndicator(true, 2, 155);
#endif
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
        JWPLC_Display.setText(FIELD_T, t);
        break;

    default:
        break;
    }
}