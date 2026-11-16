<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «Tendencias_04»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `Tendencias_04.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `Tendencias_04.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    // Página 01 · Proceso
    tempHorno = 0.0f; // p. ej. la lectura de tu sensor
    consignaHorno = 0.0f; // p. ej. la lectura de tu sensor
    potenciaSalida = 0.0f; // p. ej. la lectura de tu sensor
    // Página 02 · Compacta
    presionLinea = 0.0f; // p. ej. la lectura de tu sensor
    presionActual = 0.0f; // p. ej. la lectura de tu sensor
    caudalLinea = 0.0f; // p. ej. la lectura de tu sensor
    valvAlivio = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    nivelTanque = 0.0f; // p. ej. la lectura de tu sensor

    // MENU_CONTROL: el operador cambió un valor (OK en la botonera).
    if (JWPLC_Display.menuChanged(MENU_CONTROL))
    {
        switch (JWPLC_Display.menuNode())
        {
        case MENU_CONTROL_CONSIGNA:
            // «Consigna» -> consignaSetpoint ya tiene el valor nuevo
            break;
        case MENU_CONTROL_MARCHA:
            // «Marcha» -> marchaHorno ya tiene el valor nuevo
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
| `tempHorno` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `consignaHorno` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `potenciaSalida` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `presionLinea` | `float` | Página 02 · Compacta | Valor inicial 0.0f |
| `presionActual` | `float` | Página 02 · Compacta | Valor inicial 0.0f |
| `caudalLinea` | `float` | Página 02 · Compacta | Valor inicial 0.0f |
| `valvAlivio` | `bool` | Página 02 · Compacta | Valor inicial false |
| `nivelTanque` | `float` | Página 02 · Compacta | Valor inicial 0.0f |
| `consignaSetpoint` | `float` | MENU Parametros Horno | La cambia el operador desde el MENU; léela en tu lógica |
| `marchaHorno` | `bool` | MENU Parametros Horno | La cambia el operador desde el MENU; léela en tu lógica |

## MENU

`MENU_CONTROL`: `JWPLC_Display.menuChanged(MENU_CONTROL)` devuelve `true` una vez cuando el operador confirma un valor; `JWPLC_Display.menuNode()` dice cuál: `MENU_CONTROL_CONSIGNA`, `MENU_CONTROL_MARCHA`.

## Fecha y hora

Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.

Generado el 2026-11-13 por el JWPLC HMI Designer.
