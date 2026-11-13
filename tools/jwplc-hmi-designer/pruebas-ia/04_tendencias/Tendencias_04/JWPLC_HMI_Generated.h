// Código generado por JWPLC HMI Designer
// API pública JWPLC_UI · Alpha11 A11-4
// Destino recomendado: JWPLC_HMI_Generated.h
// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.

#pragma once

#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PROCESO = 0,
    PAGE_COMPACTA = 1,
    PAGE_AJUSTES = 2
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Proceso
    FIELD_HORA = 1,
    FIELD_TEND_HORNO = 2,

    // Página 02 · Compacta
    FIELD_TEND_PRESION = 3,
    FIELD_PRESION_VAL = 4,
    FIELD_CAUDAL_VAL = 5,
    FIELD_VALV_ALIVIO = 6,
    FIELD_NIVEL_TQ = 7,
};

// Variables HMI
// Página 01 · Proceso
float tempHorno = 0.0f;
float consignaHorno = 0.0f;
float potenciaSalida = 0.0f;

// Página 02 · Compacta
float presionLinea = 0.0f;
float presionActual = 0.0f;
float caudalLinea = 0.0f;
bool valvAlivio = false;
float nivelTanque = 0.0f;

// MENU Parametros Horno
float consignaSetpoint = 65.0f;
bool marchaHorno = true;

// Definición declarativa
static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_HORA,
        JWPLC_UIRect(244, 2),
        JWPLC_UIText(nullptr, nullptr),
        JWPLC_UIDateTimeFormat(
            JWPLC_UI_DATETIME_HH_MM_SS),
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
            0x000F,
            0xFFFF)),
    // Gráfica de Tendencia (312x118) - Muestreo: 250ms - Escala: AUTO - Canales: 3 - Umbrales: 2
    JWPLC_UITrendField(
        FIELD_TEND_HORNO,
        JWPLC_UIRect(4, 24, 312, 118),
        JWPLC_UIText("HORNO", "\367C"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UITrendConfig(
            250,
            60,
            0,
            1,
            1,
            true,
            true,
            true,
            500,
            0xBBE7,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0000,
            true,
            3,
            0x07FF,
            0x07E0,
            2,
            85.0f,
            0xF800,
            25.0f,
            0xFFE0),
        0,
        JWPLC_UIColors(
            0xFFFF,
            0xBBE7,
            0x0000,
            0xFFFF)),
    // Gráfica de Tendencia (176x86) - Muestreo: 200ms - Escala: MANUAL - Umbrales: 1
    JWPLC_UITrendField(
        FIELD_TEND_PRESION,
        JWPLC_UIRect(4, 24, 176, 86),
        JWPLC_UIText("PRES", "bar"),
        JWPLC_UIRange(0.0f, 10.0f),
        JWPLC_UITrendConfig(
            200,
            50,
            1,
            1,
            1,
            true,
            true,
            false,
            1000,
            0x07FF,
            0xFFFF,
            0x4208,
            0xFFFF,
            0x0883,
            true,
            1,
            0x07FF,
            0x07E0,
            1,
            8.0f,
            0xF800,
            20.0f,
            0xFFE0),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0883,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_PRESION_VAL,
        JWPLC_UIRect(186, 24),
        JWPLC_UIText("Pres", "bar"),
        JWPLC_UIValueFormat(
            2,
            1,
            false,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    JWPLC_UIValueField(
        FIELD_CAUDAL_VAL,
        JWPLC_UIRect(186, 56),
        JWPLC_UIText("Caudal", "L/m"),
        JWPLC_UIValueFormat(
            3,
            0,
            false,
            false),
        JWPLC_UIValueStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_RIGHT),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0xFFFF,
            0x0000,
            0xFFFF)),
    JWPLC_UIBoolField(
        FIELD_VALV_ALIVIO,
        JWPLC_UIRect(186, 88),
        JWPLC_UIText("Alivio", nullptr),
#ifdef JWPLC_UI_HAS_BOOL_PILOT
        JWPLC_UIBoolPilot(
            0x07E0,
            0xF800),
#else
        JWPLC_UIBoolText(
            "OK",
            "ABR"),
#endif
        JWPLC_UIBoolStyle(
            1,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_CENTER),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x07FF,
            0x0000,
            0xFFFF)),
    // Barra sólida (HORIZONTAL) - Radio: 0
    JWPLC_UIBarField(
        FIELD_NIVEL_TQ,
        JWPLC_UIRect(4, 116, 250, JWPLC_UI_AUTO),
        JWPLC_UIText("Nivel", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_INLINE),
        1,
        JWPLC_UIColors(
            0xFFFF,
            0x055F,
            0x0000,
            0xFFFF),
        JWPLC_UIBarConfig(
            JWPLC_UI_BAR_HORIZONTAL,
            JWPLC_UI_BAR_STYLE_SOLID,
            0,
            0,
            false,
            0xFFFF,
            1,
            16,
            0))
};

// MENU (A15-8): requiere JWPLC_Display con soporte de MENU.
#ifndef JWPLC_UI_HAS_MENU
#error "El MENU requiere JWPLC_Display con soporte de MENU (package JWPLC Alpha15 o posterior)."
#endif

enum HMIMenuId : uint8_t
{
    MENU_CONTROL = 0,
};

// Nodos de MENU_CONTROL: el índice es el que informa JWPLC_Display.menuNode().
enum : uint8_t
{
    MENU_CONTROL_CONSIGNA = 0,
    MENU_CONTROL_MARCHA = 1,
    MENU_CONTROL_VOLVER_A_INICIO = 2,
};

static const JWPLC_UIMenuNode HMI_MENU_CONTROL_NODES[] =
{
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna", &consignaSetpoint, 20.0f, 95.0f, 1.0f, 1, "\367C"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Marcha", &marchaHorno, "PARO", "MARCHA"),
    JWPLC_UIMenuGoIdle(JWPLC_UI_MENU_ROOT, "Volver a inicio")
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuField(
        MENU_CONTROL,
        6, 26, 308, 114,
        "Parametros Horno",
        HMI_MENU_CONTROL_NODES,
        sizeof(HMI_MENU_CONTROL_NODES) / sizeof(HMI_MENU_CONTROL_NODES[0]),
        PAGE_AJUSTES,
        1,
        true,
        JWPLC_UIMenuColors(
            0xFFFF,
            0x0000,
            0x07FF,
            0x0000,
            0x07FF,
            0x0000,
            0xFD20,
            0x0000,
            0xFFFF,
            0xFFFF))
};

void jwplcHMISetup()
{
    JWPLC_Display.setMenus(
        HMI_MENUS,
        sizeof(HMI_MENUS) / sizeof(HMI_MENUS[0]));
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserPageCount(3);
    JWPLC_Display.setUserPage(PAGE_PROCESO);
    // Indicador NN/TT (barra de navegación)
#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG
    JWPLC_Display.setPageIndicator(false);
#endif
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);

    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.enterUserUI();
}

// Barra de Navegación HMI Autogenerada (ST7789)
// Pestaña de la página actual: posicionada en el selector (◀ ▶, fondo de
// color) o seleccionada dentro de la página (OK, fondo blanco y doble marco).
static void jwplcDrawPageNavTab(uint8_t activePage, bool selecting)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.setTextSize(1);

    switch (activePage)
    {
    case 0:
        if (selecting)
        {
            tft.fillRoundRect(22, 148, 90, 20, 3, 0x045F);
            tft.setCursor(55, 154);
            tft.setTextColor(0xFFFF);
            tft.print("PRCS");
        }
        else
        {
            tft.fillRoundRect(22, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(22, 148, 90, 20, 3, 0x045F);
            tft.drawRoundRect(24, 150, 86, 16, 1, 0x045F);
            tft.setCursor(55, 154);
            tft.setTextColor(0x045F);
            tft.print("PRCS");
        }
        break;
    case 1:
        if (selecting)
        {
            tft.fillRoundRect(115, 148, 90, 20, 3, 0x0551);
            tft.setCursor(148, 154);
            tft.setTextColor(0xFFFF);
            tft.print("CMPC");
        }
        else
        {
            tft.fillRoundRect(115, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(115, 148, 90, 20, 3, 0x0551);
            tft.drawRoundRect(117, 150, 86, 16, 1, 0x0551);
            tft.setCursor(148, 154);
            tft.setTextColor(0x0551);
            tft.print("CMPC");
        }
        break;
    case 2:
        if (selecting)
        {
            tft.fillRoundRect(208, 148, 90, 20, 3, 0xFD40);
            tft.setCursor(241, 154);
            tft.setTextColor(0x0000);
            tft.print("AJST");
        }
        else
        {
            tft.fillRoundRect(208, 148, 90, 20, 3, 0xFFFF);
            tft.drawRoundRect(208, 148, 90, 20, 3, 0x7260);
            tft.drawRoundRect(210, 150, 86, 16, 1, 0x7260);
            tft.setCursor(241, 154);
            tft.setTextColor(0x7260);
            tft.print("AJST");
        }
        break;
    default:
        break;
    }
}

// Se dibuja una vez por página desde jwplcUIPageEnter().
static void jwplcDrawPageNavBar(uint8_t activePage)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    tft.setTextSize(1);

    switch (activePage)
    {
    case 0:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(115, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(115, 148, 90, 20, 3, 0x0551);
        tft.setCursor(148, 154);
        tft.setTextColor(0x0551);
        tft.print("CMPC");
        tft.fillRoundRect(208, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(208, 148, 90, 20, 3, 0xFD40);
        tft.setCursor(241, 154);
        tft.setTextColor(0xFD40);
        tft.print("AJST");
        break;
    case 1:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(22, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(22, 148, 90, 20, 3, 0x045F);
        tft.setCursor(55, 154);
        tft.setTextColor(0x045F);
        tft.print("PRCS");
        tft.fillRoundRect(208, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(208, 148, 90, 20, 3, 0xFD40);
        tft.setCursor(241, 154);
        tft.setTextColor(0xFD40);
        tft.print("AJST");
        break;
    case 2:
        tft.fillRoundRect(0, 147, 320, 22, 3, 0x0000);
        tft.fillRoundRect(22, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(22, 148, 90, 20, 3, 0x045F);
        tft.setCursor(55, 154);
        tft.setTextColor(0x045F);
        tft.print("PRCS");
        tft.fillRoundRect(115, 148, 90, 20, 3, 0x0000);
        tft.drawRoundRect(115, 148, 90, 20, 3, 0x0551);
        tft.setCursor(148, 154);
        tft.setTextColor(0x0551);
        tft.print("CMPC");
        break;
    default:
        break;
    }

    jwplcDrawPageNavTab(activePage, JWPLC_Display.isUserPageSelection());
}

// Selector <-> contenido sin cambiar de página: el firmware redibuja sólo la
// pestaña activa, con el bus tomado (Alpha15).
extern "C" void jwplcUIPageModeChanged(uint8_t page, bool selecting)
{
    jwplcDrawPageNavTab(page, selecting);
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
    case PAGE_PROCESO:
        // FIELD_RECT_2
        tft.fillRect(0, 0, 319, 20, 0x000F);
        tft.drawRect(0, 0, 319, 20, 0x000F);
        // PLANTA TERMICA
        tft.setCursor(4, 6);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("PLANTA TERMICA");
        break;

    case PAGE_COMPACTA:
        // FIELD_RECT_6
        tft.fillRect(0, 0, 319, 20, 0x000F);
        tft.drawRect(0, 0, 319, 20, 0x000F);
        // PRESION Y NIVEL
        tft.setCursor(4, 6);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("PRESION Y NIVEL");
        break;

    case PAGE_AJUSTES:
        // FIELD_RECT_13
        tft.fillRect(0, 0, 319, 20, 0x000F);
        tft.drawRect(0, 0, 319, 20, 0x000F);
        // AJUSTES DE CONTROL
        tft.setCursor(4, 6);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.print("AJUSTES DE CONTROL");
        break;

    default:
        break;
    }

    jwplcDrawPageNavBar(page);

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
        JWPLC_Display.setTrend(FIELD_TEND_HORNO, tempHorno, consignaHorno, potenciaSalida);
        break;

    case PAGE_COMPACTA:
        // Página 02 · Compacta
        JWPLC_Display.setTrend(FIELD_TEND_PRESION, presionLinea);
        JWPLC_Display.setValue(FIELD_PRESION_VAL, presionActual);
        JWPLC_Display.setValue(FIELD_CAUDAL_VAL, caudalLinea);
        JWPLC_Display.setBool(FIELD_VALV_ALIVIO, valvAlivio);
        JWPLC_Display.setBar(FIELD_NIVEL_TQ, nivelTanque);
        break;

    default:
        break;
    }
}
