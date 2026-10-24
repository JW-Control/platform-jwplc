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

};

// Variables HMI


// Definición declarativa
// Sin campos dinámicos: sólo gráfica estática por página.

void jwplcHMISetup()
{
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
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
    case PAGE_PRINCIPAL:
        // Marco
        tft.drawRect(2, 2, 148, 28, 0xF800);
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
    // Sin campos dinámicos.
}