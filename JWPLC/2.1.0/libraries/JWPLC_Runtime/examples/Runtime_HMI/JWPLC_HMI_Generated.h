// Código generado por JWPLC HMI Designer
// Proyecto «Runtime»: objetos enlazados a variables del JWPLC_Runtime.
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Runtime.h>

enum HMIPageId : uint8_t
{
    PAGE_TABLERO = 0,
    PAGE_ENTRADAS = 1
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Tablero
    FIELD_MOTOR = 1,
    FIELD_BOMBA = 2,
    FIELD_RETARDO = 3,
    FIELD_PIEZAS = 4,
    // Página 02 · Entradas
    FIELD_I1 = 5,
    FIELD_I2 = 6,
    FIELD_I3 = 7,
    FIELD_I4 = 8,
};

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    // Página 01 · Tablero
    JWPLC_UIBoolField(
        FIELD_MOTOR,
        JWPLC_UIRect(8, 34),
        JWPLC_UIText("Motor (Q1)", nullptr),
        JWPLC_UIBoolText("PARADO", "MARCHA"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_TABLERO,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_BOMBA,
        JWPLC_UIRect(166, 34),
        JWPLC_UIText("Bomba (Q2)", nullptr),
        JWPLC_UIBoolText("PARADA", "MARCHA"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_TABLERO,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    JWPLC_UIBarField(
        FIELD_RETARDO,
        JWPLC_UIRect(8, 100, 150, JWPLC_UI_AUTO),
        JWPLC_UIText("Retardo B02", "s"),
        JWPLC_UIRange(0.0f, 3.0f),
        JWPLC_UIBarStyle(1, false, JWPLC_UI_LAYOUT_STACKED),
        PAGE_TABLERO,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x18C3, 0xFFFF)),
    JWPLC_UIValueField(
        FIELD_PIEZAS,
        JWPLC_UIRect(178, 100),
        JWPLC_UIText("Piezas B03", nullptr),
        JWPLC_UIValueFormat(3, 0, false, false),
        JWPLC_UIValueStyle(3, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_RIGHT),
        PAGE_TABLERO,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    // Página 02 · Entradas
    JWPLC_UIBoolField(
        FIELD_I1,
        JWPLC_UIRect(8, 34),
        JWPLC_UIText("Marcha (I1)", nullptr),
        JWPLC_UIBoolText("0", "1"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_ENTRADAS,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_I2,
        JWPLC_UIRect(166, 34),
        JWPLC_UIText("Parada (I2)", nullptr),
        JWPLC_UIBoolText("0", "1"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_ENTRADAS,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_I3,
        JWPLC_UIRect(8, 100),
        JWPLC_UIText("Pieza (I3)", nullptr),
        JWPLC_UIBoolText("0", "1"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_ENTRADAS,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_I4,
        JWPLC_UIRect(166, 100),
        JWPLC_UIText("Reset (I4)", nullptr),
        JWPLC_UIBoolText("0", "1"),
        JWPLC_UIBoolStyle(2, 1, true, JWPLC_UI_LAYOUT_STACKED, JWPLC_UI_ALIGN_CENTER),
        PAGE_ENTRADAS,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF))
};

// El Runtime la llama al arrancar (JWPLC_Runtime.begin()).
extern "C" void jwplcRuntimeHMISetup(void)
{
    JWPLC_Runtime.setHMI(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]),
        2);
}

// Gráfica estática: una vez al entrar a la página, con la pantalla borrada.
extern "C" void jwplcRuntimeHMIPageEnter(uint8_t page)
{
    JWPLC_TFT.fillRect(0, 0, 320, 24, 0x2945);
    JWPLC_TFT.setTextColor(0xFFFF, 0x2945);
    JWPLC_TFT.setTextSize(2);
    switch (page)
    {
    case PAGE_TABLERO:
        JWPLC_TFT.drawString("Tablero", 8, 4);
        break;

    case PAGE_ENTRADAS:
        JWPLC_TFT.drawString("Entradas", 8, 4);
        break;

    default:
        break;
    }
    JWPLC_TFT.setTextSize(1);
}

// Cada ciclo: valores del programa en marcha. Solo setters; JWPLC_Display
// redibuja únicamente los campos que cambiaron.
extern "C" void jwplcRuntimeHMIUpdate(uint8_t page)
{
    switch (page)
    {
    case PAGE_TABLERO:
        JWPLC_Display.setBool(FIELD_MOTOR, JWPLC_Runtime.output(1));
        JWPLC_Display.setBool(FIELD_BOMBA, JWPLC_Runtime.output(2));
        JWPLC_Display.setBar(FIELD_RETARDO, JWPLC_Runtime.blockSeconds(2));
        JWPLC_Display.setValue(FIELD_PIEZAS, JWPLC_Runtime.counter(3));
        break;

    case PAGE_ENTRADAS:
        JWPLC_Display.setBool(FIELD_I1, JWPLC_Runtime.input(1));
        JWPLC_Display.setBool(FIELD_I2, JWPLC_Runtime.input(2));
        JWPLC_Display.setBool(FIELD_I3, JWPLC_Runtime.input(3));
        JWPLC_Display.setBool(FIELD_I4, JWPLC_Runtime.input(4));
        break;

    default:
        break;
    }
}
