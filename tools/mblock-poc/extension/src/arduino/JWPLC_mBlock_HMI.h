// JWPLC_mBlock_HMI.h
//
// Pantallas del JWPLC HMI Designer en los bloques de mBlock. El niño diseña
// la pantalla en el Designer y la escribe («Escribir en sketch») en
// %USERPROFILE%\JWPLC-HMI\<Nombre>\. El bridge copia ese JWPLC_HMI_Generated.h
// junto al sketch cuando el programa usa JWPLC_HMI.begin("<Nombre>").
//
//   JWPLC_HMI.begin("Caldera");          // páginas, campos, MENU e IDLE (jwplcHMISetup)
//   JWPLC_HmiSet(temperatura, 25.5);     // variable que declara el header del Designer
//   JWPLC_HmiGet(consignaPresion);       // valor que el operador cambió en el MENU
//   JWPLC_HMI.changed(&consignaPresion); // el operador la confirmó con OK en el MENU
//
// Las variables las declara JWPLC_HMI_Generated.h: en mBlock no crear
// variables con el mismo nombre. No mezclar con «escribir en la línea» (JWPLC_Screen):
// las dos registran los campos de la pantalla.
//
// IDLE y mi pantalla: el diseño trae su «Pantalla y arranque» (entrar con
// cualquier botón / un botón / nunca; volver con ESC / por tiempo / nunca).
// Los bloques «abrir mi pantalla con…» y «volver a la pantalla de
// entradas/salidas…» la cambian, estén antes o después de «usar la pantalla».

#ifndef JWPLC_MBLOCK_HMI_H
#define JWPLC_MBLOCK_HMI_H

#include <type_traits>
#include "JWPLC_mBlock.h"

// Los bloques de botón no toman teclas en una página con MENU (el MENU usa
// ▲ ▼ ◀ ▶ OK). El header registra sus MENU con JWPLC_Display.setMenus(lista, n);
// mientras se incluye, esta macro anota la lista antes de pasarla. La
// declaración de setMenus en JWPLC_Display_API.h ya está incluida y no cambia.
#define setMenus(list, count) setMenus(jwplc_mblock::rememberMenus((list), (count)), (count))
#include "JWPLC_HMI_Generated.h"
#undef setMenus

class JWPLC_HmiHelper
{
public:
    // El nombre sólo lo usa el bridge para elegir la carpeta del diseño.
    void begin(const char *designName)
    {
        (void)designName;
        jwplcHMISetup();
        JWPLC_Screen.applyProgramSettings();
    }

    // «cuando el operador cambie [variable] en el MENU»: true una vez por cada
    // valor de esa variable confirmado con OK. Cada MENU guarda sólo su último
    // cambio y menuChanged() lo consume, así que se recogen los de todos los
    // MENU y cada bloque toma el de su variable (los nodos apuntan a ella).
    bool changed(const void *variable)
    {
        collectMenuChanges();
        for (uint8_t i = 0; i < _pendingCount; i++)
        {
            if (_pending[i] == variable)
            {
                _pending[i] = _pending[--_pendingCount];
                return true;
            }
        }
        return false;
    }

private:
    const void *_pending[JWPLC_UI_MENU_MAX_NODES] = {};
    uint8_t _pendingCount = 0;

    void collectMenuChanges()
    {
        for (size_t i = 0; i < jwplc_mblock::menuCount; i++)
        {
            const JWPLC_UIMenu &menu = jwplc_mblock::menus[i];
            uint8_t node;
            if (JWPLC_Display.menuChanged(menu.id, &node) && node < menu.nodeCount)
                remember(menu.nodes[node].value);
        }
    }

    // Un cambio de una variable sin bloque queda anotado una sola vez.
    void remember(const void *variable)
    {
        for (uint8_t i = 0; i < _pendingCount; i++)
            if (_pending[i] == variable)
                return;
        if (_pendingCount < JWPLC_UI_MENU_MAX_NODES)
            _pending[_pendingCount++] = variable;
    }
};

inline JWPLC_HmiHelper JWPLC_HMI;

// Asignar una variable del Designer desde un bloque. La variable puede ser
// texto (char[N]), bool o número; el valor, lo que el niño ponga en el hueco:
// número, verdadero/falso o texto (un número escrito llega como "25").
// La pantalla redibuja sólo los campos que cambiaron.
template <typename V, typename T>
inline void JWPLC_HmiSet(V &variable, T value)
{
    using Value = typename std::decay<T>::type;
    constexpr bool isText = std::is_same<Value, const char *>::value || std::is_same<Value, char *>::value ||
                            std::is_same<Value, String>::value;
    if constexpr (std::is_array<V>::value)
    {
        const String text = jwplc_mblock::text(value);
        strncpy(variable, text.c_str(), sizeof(V) - 1);
        variable[sizeof(V) - 1] = 0;
    }
    else if constexpr (isText)
    {
        const double number = String(value).toDouble();
        if constexpr (std::is_same<V, bool>::value)
            variable = (number != 0);
        else
            variable = (V)number;
    }
    else if constexpr (std::is_same<V, bool>::value)
    {
        variable = (value != 0);
    }
    else
    {
        variable = (V)value;
    }
}

// Leer una variable del Designer (p. ej. una consigna del MENU) como número.
template <typename V>
inline double JWPLC_HmiGet(const V &variable)
{
    if constexpr (std::is_array<V>::value)
        return atof(variable);
    else
        return (double)variable;
}

#endif // JWPLC_MBLOCK_HMI_H
