<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «Botonera_Interactiva»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `Botonera_Interactiva.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `Botonera_Interactiva.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
sketch base de abajo; si ya tienes uno, sigue los tres pasos.

## Sketch base

```cpp
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer
}

void loop()
{
    // Escribe las variables: la pantalla redibuja sólo lo que cambió.
    // Página 01 · Receta
    recetaNum = 0.0f; // p. ej. la lectura de tu sensor
    strncpy(receta, "Listo", sizeof(receta) - 1); // TEXT: 14 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    recetaTemp = 0.0f; // p. ej. la lectura de tu sensor
    recetaTiempo = 0; // p. ej. un contador de tu lógica
    recetaCargada = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    // Página 02 · Reflejos
    luz = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    strncpy(estadoJuego, "Listo", sizeof(estadoJuego) - 1); // TEXT: 12 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    tiempoReaccion = 0.0f; // p. ej. la lectura de tu sensor
    record = 0.0f; // p. ej. la lectura de tu sensor
    // Página 03 · Motor
    marcha = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    consigna = 0.0f; // p. ej. la lectura de tu sensor
    strncpy(paso, "Listo", sizeof(paso) - 1); // TEXT: 4 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    barConsigna = 0.0f; // p. ej. la lectura de tu sensor
    velocidad = 0.0f; // p. ej. la lectura de tu sensor
    // MENU Ajustes
    partidas = 0; // p. ej. un contador de tu lógica

    // MENU_AJUSTES: el operador cambió un valor (OK en la botonera).
    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        switch (nodo)
        {
        case MENU_AJUSTES_RAMPA_MOTOR:
            // «Rampa motor» -> rampa ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_ESPERA_MAX_JUEGO:
            // «Espera max juego» -> esperaMaxMs ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_BORRAR_RECORD:
            // «Borrar record» -> borrarRecord ya tiene el valor nuevo
            break;
        default:
            break;
        }
    }
}
```

## Si ya tienes un sketch

1. Arriba de todo: `#include "JWPLC_HMI_Generated.h"`.
2. Dentro de `setup()`, una vez: `jwplcHMISetup();`.
3. En `loop()` asigna las variables de la tabla. No hace falta dibujar nada: el runtime redibuja
   sólo los campos que cambiaron. No uses `delay()`: la botonera y la pantalla siguen solas.

No vuelvas a declarar estas variables en tu `.ino`: ya están en `JWPLC_HMI_Generated.h` (declararlas
otra vez da error de redefinición). Si cambias un nombre en el Designer, cámbialo también en tu código.

## Variables del HMI

| Variable | Tipo C++ | Dónde | Nota |
|---|---|---|---|
| `recetaNum` | `float` | Página 01 · Receta | Valor inicial 0.0f |
| `receta` | `char[29]` | Página 01 · Receta | 14 caracteres (UTF-8: cada tilde ocupa 2 bytes) |
| `recetaTemp` | `float` | Página 01 · Receta | Valor inicial 0.0f |
| `recetaTiempo` | `uint32_t` | Página 01 · Receta | Valor inicial 0 |
| `recetaCargada` | `bool` | Página 01 · Receta | Valor inicial false |
| `luz` | `bool` | Página 02 · Reflejos | Valor inicial false |
| `estadoJuego` | `char[25]` | Página 02 · Reflejos | 12 caracteres (UTF-8: cada tilde ocupa 2 bytes) |
| `tiempoReaccion` | `float` | Página 02 · Reflejos | Valor inicial 0.0f |
| `record` | `float` | Página 02 · Reflejos | Valor inicial 0.0f |
| `marcha` | `bool` | Página 03 · Motor | Valor inicial false |
| `consigna` | `float` | Página 03 · Motor | Valor inicial 0.0f |
| `paso` | `char[9]` | Página 03 · Motor | 4 caracteres (UTF-8: cada tilde ocupa 2 bytes) |
| `barConsigna` | `float` | Página 03 · Motor | Valor inicial 0.0f |
| `velocidad` | `float` | Página 03 · Motor | Valor inicial 0.0f |
| `rampa` | `float` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |
| `esperaMaxMs` | `int32_t` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |
| `partidas` | `int32_t` | MENU Ajustes | El MENU la muestra («Ver»); escríbela en tu lógica |
| `borrarRecord` | `bool` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |

## MENU

`MENU_AJUSTES`: `JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo)` devuelve `true` una vez cuando el operador confirma un valor; `nodo` es uno de: `MENU_AJUSTES_RAMPA_MOTOR`, `MENU_AJUSTES_ESPERA_MAX_JUEGO`, `MENU_AJUSTES_BORRAR_RECORD`.

Generado el 2026-10-23 por el JWPLC HMI Designer.
