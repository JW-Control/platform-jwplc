<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «09.HMI_Registro_Eventos»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `09.HMI_Registro_Eventos.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `09.HMI_Registro_Eventos.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    bombaMarcha = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    alarmaNivel = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    arranques = 0.0f; // p. ej. la lectura de tu sensor
    eventos = 0.0f; // p. ej. la lectura de tu sensor
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
| `bombaMarcha` | `bool` | Página 01 · Proceso | Valor inicial false |
| `alarmaNivel` | `bool` | Página 01 · Proceso | Valor inicial false |
| `arranques` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `eventos` | `float` | Página 01 · Proceso | Valor inicial 0.0f |

Generado el 2026-11-01 por el JWPLC HMI Designer.
