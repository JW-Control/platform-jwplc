<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «A14_F2_Combinado»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `A14_F2_Combinado.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `A14_F2_Combinado.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    temperatura = 0.0f; // p. ej. la lectura de tu sensor
    estadoMotor = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    nivel = 0.0f; // p. ej. la lectura de tu sensor
    // Página 02 · Comunicaciones
    ethLista = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    tcpPeticiones = 0; // p. ej. un contador de tu lógica
    rtuOk = 0; // p. ej. un contador de tu lógica
    rtuErr = 0; // p. ej. un contador de tu lógica
    esclavoHR0 = 0.0f; // p. ej. la lectura de tu sensor

    // MENU_AJUSTES: el operador cambió un valor (OK en la botonera).
    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        switch (nodo)
        {
        case MENU_AJUSTES_CONSIGNA:
            // «Consigna» -> consigna ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_BOMBA:
            // «Bomba» -> bomba ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_ESCRIBIR_HR1:
            // «Escribir HR1» -> valorHR1 ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_PITIDO:
            // «Pitido» -> pitido ya tiene el valor nuevo
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
| `temperatura` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `bomba` | `bool` | Página 01 · Proceso | La cambia el operador desde el MENU; léela en tu lógica |
| `estadoMotor` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `nivel` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `ethLista` | `bool` | Página 02 · Comunicaciones | Valor inicial false |
| `tcpPeticiones` | `uint32_t` | Página 02 · Comunicaciones | Valor inicial 0 |
| `rtuOk` | `uint32_t` | Página 02 · Comunicaciones | Valor inicial 0 |
| `rtuErr` | `uint32_t` | Página 02 · Comunicaciones | Valor inicial 0 |
| `esclavoHR0` | `float` | Página 02 · Comunicaciones | Valor inicial 0.0f |
| `consigna` | `float` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |
| `valorHR1` | `int32_t` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |
| `pitido` | `bool` | MENU Ajustes | La cambia el operador desde el MENU; léela en tu lógica |

## MENU

`MENU_AJUSTES`: `JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo)` devuelve `true` una vez cuando el operador confirma un valor; `nodo` es uno de: `MENU_AJUSTES_CONSIGNA`, `MENU_AJUSTES_BOMBA`, `MENU_AJUSTES_ESCRIBIR_HR1`, `MENU_AJUSTES_PITIDO`.

## Fecha y hora

Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.

Generado el 2026-10-24 por el JWPLC HMI Designer.
