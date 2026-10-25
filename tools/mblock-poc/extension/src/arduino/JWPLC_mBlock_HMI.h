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
