<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «Sensor_COD_05»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `Sensor_COD_05.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `Sensor_COD_05.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    // Página 01 · Medicion
    measProgress = 0.0f; // p. ej. la lectura de tu sensor
    stCfg = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    measElapsedS = 0; // p. ej. un contador de tu lógica
    // Página 02 · Vista_General
    valTCod = 0.0f; // p. ej. la lectura de tu sensor
    valCod = 0.0f; // p. ej. la lectura de tu sensor
    valBod = 0.0f; // p. ej. la lectura de tu sensor
    valToc = 0.0f; // p. ej. la lectura de tu sensor
    valTurb = 0.0f; // p. ej. la lectura de tu sensor
    stGen = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    // Página 03 · Temp_COD
    stTCod = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    trendTCod = 0.0f; // p. ej. la lectura de tu sensor
    // Página 04 · Graf_COD
    stCod = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    trendCod = 0.0f; // p. ej. la lectura de tu sensor
    // Página 05 · Graf_BOD
    stBod = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    trendBod = 0.0f; // p. ej. la lectura de tu sensor
    // Página 06 · Graf_TOC
    stToc = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    trendToc = 0.0f; // p. ej. la lectura de tu sensor
    // Página 07 · Graf_TURB
    stTurb = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    trendTurb = 0.0f; // p. ej. la lectura de tu sensor

    // MENU_MEDICION: el operador cambió un valor (OK en la botonera).
    if (JWPLC_Display.menuChanged(MENU_MEDICION))
    {
        switch (JWPLC_Display.menuNode())
        {
        case MENU_MEDICION_MEDICION:
            // «Medicion» -> measRun ya tiene el valor nuevo
            break;
        case MENU_MEDICION_ETIQUETA_P:
            // «Etiqueta P» -> measTag ya tiene el valor nuevo
            break;
        case MENU_MEDICION_DURACION:
            // «Duracion» -> measMins ya tiene el valor nuevo
            break;
        case MENU_MEDICION_SIMULACION:
            // «Simulacion» -> simActive ya tiene el valor nuevo
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
| `measProgress` | `float` | Página 01 · Medicion | Valor inicial 0.0f |
| `stCfg` | `bool` | Página 01 · Medicion | Valor inicial false |
| `measElapsedS` | `uint32_t` | Página 01 · Medicion | Valor inicial 0 |
| `valTCod` | `float` | Página 02 · Vista_General | Valor inicial 0.0f |
| `valCod` | `float` | Página 02 · Vista_General | Valor inicial 0.0f |
| `valBod` | `float` | Página 02 · Vista_General | Valor inicial 0.0f |
| `valToc` | `float` | Página 02 · Vista_General | Valor inicial 0.0f |
| `valTurb` | `float` | Página 02 · Vista_General | Valor inicial 0.0f |
| `stGen` | `bool` | Página 02 · Vista_General | Valor inicial false |
| `stTCod` | `bool` | Página 03 · Temp_COD | Valor inicial false |
| `trendTCod` | `float` | Página 03 · Temp_COD | Valor inicial 0.0f |
| `stCod` | `bool` | Página 04 · Graf_COD | Valor inicial false |
| `trendCod` | `float` | Página 04 · Graf_COD | Valor inicial 0.0f |
| `stBod` | `bool` | Página 05 · Graf_BOD | Valor inicial false |
| `trendBod` | `float` | Página 05 · Graf_BOD | Valor inicial 0.0f |
| `stToc` | `bool` | Página 06 · Graf_TOC | Valor inicial false |
| `trendToc` | `float` | Página 06 · Graf_TOC | Valor inicial 0.0f |
| `stTurb` | `bool` | Página 07 · Graf_TURB | Valor inicial false |
| `trendTurb` | `float` | Página 07 · Graf_TURB | Valor inicial 0.0f |
| `measRun` | `bool` | MENU Control de Medicion | La cambia el operador desde el MENU; léela en tu lógica |
| `measTag` | `int32_t` | MENU Control de Medicion | La cambia el operador desde el MENU; léela en tu lógica |
| `measMins` | `int32_t` | MENU Control de Medicion | La cambia el operador desde el MENU; léela en tu lógica |
| `simActive` | `bool` | MENU Control de Medicion | La cambia el operador desde el MENU; léela en tu lógica |

## MENU

`MENU_MEDICION`: `JWPLC_Display.menuChanged(MENU_MEDICION)` devuelve `true` una vez cuando el operador confirma un valor; `JWPLC_Display.menuNode()` dice cuál: `MENU_MEDICION_MEDICION`, `MENU_MEDICION_ETIQUETA_P`, `MENU_MEDICION_DURACION`, `MENU_MEDICION_SIMULACION`.

## Fecha y hora

Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.

Generado el 2026-11-13 por el JWPLC HMI Designer.
