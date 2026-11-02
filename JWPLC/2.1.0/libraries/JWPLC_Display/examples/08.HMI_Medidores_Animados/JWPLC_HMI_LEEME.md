<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «08.HMI_Medidores_Animados»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `08.HMI_Medidores_Animados.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `08.HMI_Medidores_Animados.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    nivel = 0.0f; // p. ej. la lectura de tu sensor
    presion = 0.0f; // p. ej. la lectura de tu sensor
    bomba = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    motor = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    ventilador = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    caudal = 0.0f; // p. ej. la lectura de tu sensor
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
| `nivel` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `presion` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `bomba` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `motor` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `ventilador` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `caudal` | `float` | Página 01 · Proceso | Valor inicial 0.0f |

Generado el 2026-11-01 por el JWPLC HMI Designer.
