#ifndef JWPLC_UI_MENU_H
#define JWPLC_UI_MENU_H

#include <Arduino.h>
#include <stddef.h>

// MENU declarativo (Alpha15, MVP): árbol de menús/submenús dentro de una
// página USER para ver y configurar variables con la botonera.
//
// - Profundidad máxima 3 (raíz = nivel 1).
// - Nodos: SUBMENU, VIEW (sólo lectura), EDIT_NUM y EDIT_BOOL (lectura/escritura).
// - Las variables se vinculan por puntero (float, int32_t o bool) y deben vivir
//   todo el programa (globales o static). La tabla de nodos también.
// - El MENU es dueño de UP/DOWN/LEFT/RIGHT/OK mientras su página está en
//   PAGE_CONTENT (o si sólo hay una página). ESC cancela la edición o sube un
//   nivel; en la raíz se deja al sistema (vuelve a PAGE_SELECT / IDLE).
// - Sólo se escribe la variable al confirmar con OK; ESC descarta.
// - Un MENU por página: si hay varios en la misma página sólo el primero
//   recibe teclas.
//
// Teclas:
//   UP/DOWN     mover selección / cambiar el valor en edición
//   OK, RIGHT   entrar al submenú; OK empieza y confirma la edición
//   LEFT        subir un nivel; en edición numérica, -10 pasos (RIGHT +10)
//   ESC         cancelar edición / subir un nivel

// El header generado por el Designer comprueba esta macro.
#define JWPLC_UI_HAS_MENU 1

static constexpr uint8_t JWPLC_UI_MENU_MAX_MENUS = 4;
static constexpr uint8_t JWPLC_UI_MENU_MAX_NODES = 64;
static constexpr uint8_t JWPLC_UI_MENU_MAX_DEPTH = 3;
static constexpr uint8_t JWPLC_UI_MENU_ROOT = 0xFF;

enum JWPLC_UIMenuNodeType : uint8_t
{
    JWPLC_UI_MENU_SUBMENU = 0,
    JWPLC_UI_MENU_VIEW,
    JWPLC_UI_MENU_EDIT_NUM,
    JWPLC_UI_MENU_EDIT_BOOL,
    // Alpha15: acción "volver a la pantalla IDLE" (OK -> JWPLC_Display.goIdle()).
    // Sirve con el retorno "Nunca" del Designer: la salida la decide el menú.
    JWPLC_UI_MENU_GO_IDLE
};

#define JWPLC_UI_HAS_MENU_GO_IDLE 1

enum JWPLC_UIMenuValueKind : uint8_t
{
    JWPLC_UI_MENU_VALUE_NONE = 0,
    JWPLC_UI_MENU_VALUE_FLOAT,
    JWPLC_UI_MENU_VALUE_INT32,
    JWPLC_UI_MENU_VALUE_BOOL
};

// Un nodo del árbol. parent es el índice (en la misma tabla) de un nodo
// SUBMENU declarado antes, o JWPLC_UI_MENU_ROOT. Se construye con los
// helpers JWPLC_UIMenuSubmenu/View/EditNum/EditBool.
struct JWPLC_UIMenuNode
{
    JWPLC_UIMenuNodeType type;
    JWPLC_UIMenuValueKind kind;
    uint8_t parent;
    uint8_t decimals;
    const char *label;
    void *value;
    float min;
    float max;
    float step;
    const char *unit;
    const char *falseText;
    const char *trueText;
};

struct JWPLC_UIMenuColors
{
    uint16_t text;
    uint16_t background;
    uint16_t value;
    uint16_t selectText;
    uint16_t selectBackground;
    uint16_t editText;
    uint16_t editBackground;
    uint16_t title;
    uint16_t titleBackground;
    uint16_t frame;

    JWPLC_UIMenuColors(
        uint16_t textValue = 0xFFFF,
        uint16_t backgroundValue = 0x0000,
        uint16_t valueValue = 0x07FF,
        uint16_t selectTextValue = 0x0000,
        uint16_t selectBackgroundValue = 0x07FF,
        uint16_t editTextValue = 0x0000,
        uint16_t editBackgroundValue = 0xFD20,
        uint16_t titleValue = 0x0000,
        uint16_t titleBackgroundValue = 0xFFFF,
        uint16_t frameValue = 0xFFFF);
};

struct JWPLC_UIMenu
{
    uint8_t id;
    uint8_t page;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    const char *title;
    const JWPLC_UIMenuNode *nodes;
    uint8_t nodeCount;
    uint8_t textSize;
    bool frame;
    JWPLC_UIMenuColors colors;
};

JWPLC_UIMenuNode JWPLC_UIMenuSubmenu(
    uint8_t parent,
    const char *label);

JWPLC_UIMenuNode JWPLC_UIMenuView(
    uint8_t parent,
    const char *label,
    const float *value,
    uint8_t decimals = 1,
    const char *unit = nullptr);

JWPLC_UIMenuNode JWPLC_UIMenuView(
    uint8_t parent,
    const char *label,
    const int32_t *value,
    const char *unit = nullptr);

JWPLC_UIMenuNode JWPLC_UIMenuView(
    uint8_t parent,
    const char *label,
    const bool *value,
    const char *falseText = "OFF",
    const char *trueText = "ON");

JWPLC_UIMenuNode JWPLC_UIMenuEditNum(
    uint8_t parent,
    const char *label,
    float *value,
    float min,
    float max,
    float step = 1.0f,
    uint8_t decimals = 1,
    const char *unit = nullptr);

JWPLC_UIMenuNode JWPLC_UIMenuEditNum(
    uint8_t parent,
    const char *label,
    int32_t *value,
    int32_t min,
    int32_t max,
    int32_t step = 1,
    const char *unit = nullptr);

JWPLC_UIMenuNode JWPLC_UIMenuEditBool(
    uint8_t parent,
    const char *label,
    bool *value,
    const char *falseText = "OFF",
    const char *trueText = "ON");

// Ítem de acción: con OK vuelve a la pantalla IDLE. Se dibuja con "<" a la derecha.
JWPLC_UIMenuNode JWPLC_UIMenuGoIdle(
    uint8_t parent,
    const char *label);

// width/height obligatorios (> 0). textSize 1 (filas de 12 px) o 2 (20 px).
JWPLC_UIMenu JWPLC_UIMenuField(
    uint8_t id,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char *title,
    const JWPLC_UIMenuNode *nodes,
    uint8_t nodeCount,
    uint8_t page = 0,
    uint8_t textSize = 1,
    bool frame = true,
    JWPLC_UIMenuColors colors = JWPLC_UIMenuColors());

namespace JWPLCUI
{
    bool setMenus(const JWPLC_UIMenu *menus, size_t count);
    void clearMenus();
    size_t menuCount();
    bool menuChanged(uint8_t menuId, uint8_t *nodeIndex);
    bool menuEditing();
}

#endif // JWPLC_UI_MENU_H
