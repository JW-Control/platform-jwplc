<!-- Generado por JWPLC HMI Designer: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->
# HMI del sketch «05.HMI_Plantilla_Caldera»

Archivos que escribe el JWPLC HMI Designer en esta carpeta:

| Archivo | Qué es | ¿Se edita a mano? |
|---|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |
| `05.HMI_Plantilla_Caldera.jwhmi` | El diseño, para volver a abrirlo en el Designer | No |
| `JWPLC_HMI_LEEME.md` | Esta guía | No |

**Tu `05.HMI_Plantilla_Caldera.ino` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el
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
    // Página 01 · Resumen
    temperatura = 0.0f; // p. ej. la lectura de tu sensor
    presion = 0.0f; // p. ej. la lectura de tu sensor
    nivelAgua = 0.0f; // p. ej. la lectura de tu sensor
    quemador = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    bomba = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    strncpy(modo, "Listo", sizeof(modo) - 1); // TEXT: 6 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    // Página 02 · Proceso
    tempHogar = 0.0f; // p. ej. la lectura de tu sensor
    caudalVapor = 0.0f; // p. ej. la lectura de tu sensor
    caudalAgua = 0.0f; // p. ej. la lectura de tu sensor
    valvulaVapor = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    // Página 03 · Alarmas
    alNivelBajo = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    alSobrepresion = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    alFallaLlama = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    strncpy(ultimaAlarma, "Listo", sizeof(ultimaAlarma) - 1); // TEXT: 24 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    // Página 04 · Producción
    vaporTotalKg = 0; // p. ej. un contador de tu lógica
    horometroSeg = 0; // p. ej. un contador de tu lógica
    eficiencia = 0.0f; // p. ej. la lectura de tu sensor
    combustible = 0.0f; // p. ej. la lectura de tu sensor

    // MENU_AJUSTES: el operador cambió un valor (OK en la botonera).
    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        switch (nodo)
        {
        case MENU_AJUSTES_CONSIGNA_PRESION:
            // «Consigna presi\242n» -> consignaPresion ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_HISTERESIS:
            // «Hist\202resis» -> histeresis ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_MODO_AUTOMATICO:
            // «Modo autom\240tico» -> modoAuto ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_TEMP__MAXIMA:
            // «Temp. m\240xima» -> tempMax ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_PRESION_MAXIMA:
            // «Presi\242n m\240xima» -> presionMax ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_RETARDO_LLAMA:
            // «Retardo llama» -> retardoLlama ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_PURGA_MANUAL:
            // «Purga manual» -> purga ya tiene el valor nuevo
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
| `temperatura` | `float` | Página 01 · Resumen | Valor inicial 0.0f |
| `presion` | `float` | Página 01 · Resumen | Valor inicial 0.0f |
| `nivelAgua` | `float` | Página 01 · Resumen | Valor inicial 0.0f |
| `quemador` | `bool` | Página 01 · Resumen | Valor inicial false |
| `bomba` | `bool` | Página 01 · Resumen | Valor inicial false |
| `modo` | `char[13]` | Página 01 · Resumen | 6 caracteres (UTF-8: cada tilde ocupa 2 bytes) |
| `tempHogar` | `float` | Página 02 · Proceso | Valor inicial 0.0f |
| `caudalVapor` | `float` | Página 02 · Proceso | Valor inicial 0.0f |
| `caudalAgua` | `float` | Página 02 · Proceso | Valor inicial 0.0f |
| `valvulaVapor` | `bool` | Página 02 · Proceso | Valor inicial false |
| `alNivelBajo` | `bool` | Página 03 · Alarmas | Valor inicial false |
| `alSobrepresion` | `bool` | Página 03 · Alarmas | Valor inicial false |
| `alFallaLlama` | `bool` | Página 03 · Alarmas | Valor inicial false |
| `ultimaAlarma` | `char[49]` | Página 03 · Alarmas | 24 caracteres (UTF-8: cada tilde ocupa 2 bytes) |
| `vaporTotalKg` | `uint32_t` | Página 04 · Producción | Valor inicial 0 |
| `horometroSeg` | `uint32_t` | Página 04 · Producción | Valor inicial 0 |
| `eficiencia` | `float` | Página 04 · Producción | Valor inicial 0.0f |
| `combustible` | `float` | Página 04 · Producción | Valor inicial 0.0f |
| `consignaPresion` | `float` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `histeresis` | `float` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `modoAuto` | `bool` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `tempMax` | `float` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `presionMax` | `float` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `retardoLlama` | `int32_t` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |
| `purga` | `bool` | MENU Ajustes de caldera | La cambia el operador desde el MENU; léela en tu lógica |

## MENU

`MENU_AJUSTES`: `JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo)` devuelve `true` una vez cuando el operador confirma un valor; `nodo` es uno de: `MENU_AJUSTES_CONSIGNA_PRESION`, `MENU_AJUSTES_HISTERESIS`, `MENU_AJUSTES_MODO_AUTOMATICO`, `MENU_AJUSTES_TEMP__MAXIMA`, `MENU_AJUSTES_PRESION_MAXIMA`, `MENU_AJUSTES_RETARDO_LLAMA`, `MENU_AJUSTES_PURGA_MANUAL`.

## Fecha y hora

Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.

Generado el 2026-10-23 por el JWPLC HMI Designer.
