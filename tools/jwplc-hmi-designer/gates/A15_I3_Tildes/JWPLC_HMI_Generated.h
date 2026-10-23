// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_CONFIGURACION = 0
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Configuración
    FIELD_ESTADO = 1,
    FIELD_PRESION = 2,
    FIELD_VALVULA = 3,
};

// Variables HMI
// Página 01 · Configuración
char estado[25] = {}; // 12 caracteres (UTF-8: cada tilde ocupa 2 bytes)
float presion = 0.0f;
bool valvula = false;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_ESTADO,
        JWPLC_UIRect(4, 4),
        JWPLC_UIText("Se\244al", nullptr, 12),
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
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_PRESION,
        JWPLC_UIRect(4, 30),
        JWPLC_UIText("Presi\242n", "\367C"),
        JWPLC_UIValueFormat(
            3,
            1,
            false,
            false),
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
    JWPLC_UIBoolField(
        FIELD_VALVULA,
        JWPLC_UIRect(4, 70),
        JWPLC_UIText("V\240lvula", nullptr),
        JWPLC_UIBoolText(
            "NO",
            "SI"),
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
            0xFFFF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(PAGE_CONFIGURACION);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

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
    case PAGE_CONFIGURACION:
        // Titulo
        tft.fillRect(2, 118, 105, 11, 0x0000);
        tft.setCursor(4, 120);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("L\241nea 2 \371 \250Listo?");
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
    case PAGE_CONFIGURACION:
        // Página 01 · Configuración
        JWPLC_Display.setText(FIELD_ESTADO, estado);
        JWPLC_Display.setValue(FIELD_PRESION, presion);
        JWPLC_Display.setBool(FIELD_VALVULA, valvula);
        break;

    default:
        break;
    }
}