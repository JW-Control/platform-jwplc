<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «07.HMI_Iconos_Planta»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `07.HMI_Iconos_Planta.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `07.HMI_Iconos_Planta.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    tanque = 0; // ICON: 0=Vacío 1=Bajo 2=Medio 3=Lleno
    valvulaEntrada = 0; // ICON: 0=Cerrada 1=Abierta 2=Falla
    bomba = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    agitador = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    paro = false; // ICON: false=Normal true=Activado
    nivel = 0.0f; // p. ej. la lectura de tu sensor
    modoAuto = 0; // ICON: 0=Apagado 1=Verde 2=Amarillo 3=Rojo
    alarma = false; // ICON: false=Normal true=Activa
    caudal = 0.0f; // p. ej. la lectura de tu sensor
    // Página 02 · Equipos
    cinta = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    silo = 0; // ICON: 0=Vacío 1=Con material 2=Falla
    ventilador = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    compresor = 0; // ICON: 0=Parado 1=En marcha 2=Falla
    resistencia = false; // ICON: false=Apagada true=Encendida
    temperatura = 0; // ICON: 0=Baja 1=Normal 2=Alta
    presion = 0; // ICON: 0=Baja 1=Normal 2=Alta
    baliza = 0; // ICON: 0=Apagada 1=Verde 2=Amarilla 3=Roja
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
| `tanque` | `uint8_t` | Página 01 · Proceso | ICON: 0=Vacío 1=Bajo 2=Medio 3=Lleno |
| `valvulaEntrada` | `uint8_t` | Página 01 · Proceso | ICON: 0=Cerrada 1=Abierta 2=Falla |
| `bomba` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `agitador` | `uint8_t` | Página 01 · Proceso | ICON: 0=Parado 1=En marcha 2=Falla |
| `paro` | `bool` | Página 01 · Proceso | ICON: false=Normal true=Activado |
| `nivel` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `modoAuto` | `uint8_t` | Página 01 · Proceso | ICON: 0=Apagado 1=Verde 2=Amarillo 3=Rojo |
| `alarma` | `bool` | Página 01 · Proceso | ICON: false=Normal true=Activa |
| `caudal` | `float` | Página 01 · Proceso | Valor inicial 0.0f |
| `cinta` | `uint8_t` | Página 02 · Equipos | ICON: 0=Parado 1=En marcha 2=Falla |
| `silo` | `uint8_t` | Página 02 · Equipos | ICON: 0=Vacío 1=Con material 2=Falla |
| `ventilador` | `uint8_t` | Página 02 · Equipos | ICON: 0=Parado 1=En marcha 2=Falla |
| `compresor` | `uint8_t` | Página 02 · Equipos | ICON: 0=Parado 1=En marcha 2=Falla |
| `resistencia` | `bool` | Página 02 · Equipos | ICON: false=Apagada true=Encendida |
| `temperatura` | `uint8_t` | Página 02 · Equipos | ICON: 0=Baja 1=Normal 2=Alta |
| `presion` | `uint8_t` | Página 02 · Equipos | ICON: 0=Baja 1=Normal 2=Alta |
| `baliza` | `uint8_t` | Página 02 · Equipos | ICON: 0=Apagada 1=Verde 2=Amarilla 3=Roja |

## Fecha y hora

Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.

Generado el 2026-10-23 por el JWPLC HMI Designer.
