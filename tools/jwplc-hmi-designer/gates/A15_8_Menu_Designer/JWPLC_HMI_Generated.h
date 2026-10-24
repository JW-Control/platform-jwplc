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

// MENU Ajustes
float temperatura = 21.5f;
float consigna = 45.0f;
bool bomba = false;
int32_t ciclos = 0;
int32_t retardo = 30;
float alarmaAlta = 80.0f;
bool alarmaHabilitada = true;

// Definición declarativa
// Sin campos dinámicos: sólo gráfica estática por página.

// MENU (A15-8): requiere JWPLC_Display con soporte de MENU.
#ifndef JWPLC_UI_HAS_MENU
#error "El MENU requiere JWPLC_Display con soporte de MENU (package JWPLC Alpha15 o posterior)."
#endif

enum HMIMenuId : uint8_t
{
    MENU_AJUSTES = 0,
};

// Nodos de MENU_AJUSTES: el índice es el que informa JWPLC_Display.menuChanged().
enum : uint8_t
{
    MENU_AJUSTES_TEMPERATURA = 0,
    MENU_AJUSTES_CONSIGNA = 1,
    MENU_AJUSTES_BOMBA = 2,
    MENU_AJUSTES_AVANZADO = 3,
    MENU_AJUSTES_CICLOS = 4,
    MENU_AJUSTES_RETARDO = 5,
    MENU_AJUSTES_ALARMAS = 6,
    MENU_AJUSTES_ALTA = 7,
    MENU_AJUSTES_HABILITADA = 8,
};

static const JWPLC_UIMenuNode HMI_MENU_AJUSTES_NODES[] =
{
    JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Temperatura", &temperatura, 1, "C"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna", &consigna, 0.0f, 100.0f, 0.5f, 1, "C"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Bomba", &bomba, "PARADA", "MARCHA"),
    JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "Avanzado"),
    JWPLC_UIMenuView(MENU_AJUSTES_AVANZADO, "Ciclos", &ciclos, nullptr),
    JWPLC_UIMenuEditNum(MENU_AJUSTES_AVANZADO, "Retardo", &retardo, 0, 600, 5, "s"),
    JWPLC_UIMenuSubmenu(MENU_AJUSTES_AVANZADO, "Alarmas"),
    JWPLC_UIMenuEditNum(MENU_AJUSTES_ALARMAS, "Alta", &alarmaAlta, 0.0f, 150.0f, 1.0f, 0, "C"),
    JWPLC_UIMenuEditBool(MENU_AJUSTES_ALARMAS, "Habilitada", &alarmaHabilitada, "NO", "SI")
};

static const JWPLC_UIMenu HMI_MENUS[] =
{
    JWPLC_UIMenuField(
        MENU_AJUSTES,
        10, 20, 300, 140,
        "Ajustes",
        HMI_MENU_AJUSTES_NODES,
        sizeof(HMI_MENU_AJUSTES_NODES) / sizeof(HMI_MENU_AJUSTES_NODES[0]),
        PAGE_PRINCIPAL,
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
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
}

// Sincronización gráfica autogenerada (cada tick de refresco).
// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.
// La lógica de aplicación debe permanecer en loop().
void jwplcUIUpdate()
{
    // Sin campos dinámicos.
}