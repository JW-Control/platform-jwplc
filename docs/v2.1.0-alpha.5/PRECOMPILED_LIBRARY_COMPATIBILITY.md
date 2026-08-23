# Alpha5 - Compatibilidad de librerias precompiladas

## Objetivo

Corregir incompatibilidades al reutilizar librerias precompiladas entre placas del package que comparten `build.mcu=esp32` pero usan cores distintos.

Caso que expuso la regresion original:

```txt
JWPLC Laundry
FQBN: jwplc_local:esp32:esp32
Core: esp32
Variant: esp32
```

Los archives P1 se habian generado previamente para JWPLC Basic bajo el entorno JWPLC, con `JWPLC_BASIC` definido y los remapeos de GPIO activos.

En ese contexto, `Arduino.h` remapea:

```txt
pinMode       -> jwplc_pinMode
digitalWrite  -> jwplc_digitalWrite
digitalRead   -> jwplc_digitalRead
```

Si una libreria que usa esas APIs se compila como `.a` dentro de `src/esp32`, Arduino puede reutilizar el mismo archive tambien en `ESP32 Board`, porque los targets comparten `build.mcu=esp32` aunque no compartan core ni los simbolos `jwplc_*`.

## Correccion Alpha5

### JW_MatrixButtons

Se aplica la correccion minima y conservadora:

1. retirar `libJW_MatrixButtons.a` generado bajo el entorno JWPLC;
2. retirar `precompiled=full` de `JW_MatrixButtons/library.properties`;
3. excluir `JW_MatrixButtons` de la lista P1 de `Build-JWPLCPrecompiledLibraries.ps1`.

No se modifica la API publica de `JW_MatrixButtons` ni sus llamadas GPIO.

### JW_SD

La auditoria posterior de symbols P1 detecto el mismo patron en `JW_SD`:

```txt
jwplc_pinMode
jwplc_digitalRead
```

El origen esta en `JW_SD.cpp`, donde la funcionalidad de deteccion de tarjeta usa `pinMode()` y `digitalRead()`. Bajo `JWPLC_BASIC`, esas llamadas quedan remapeadas al core JWPLC al generar el archive.

Se aplica la misma correccion conservadora:

1. retirar `libJW_SD.a` de `JW_SD/src/esp32`;
2. retirar `precompiled=full` de `JW_SD/library.properties`;
3. excluir `JW_SD` de la lista P1 por defecto;
4. mantener intacta la API y la implementacion GPIO de `JW_SD`.

No se modifica el firmware JWPLC Laundry.
No se modifica `platform.txt`.
No se modifica `build.mcu`.
No se quitan perifericos del autoload normal.

## Validacion requerida en Arduino IDE

### A. ESP32 Board - caso JWPLC Laundry

Seleccionar la placa generica del package:

```txt
ESP32 Board
FQBN esperado: jwplc_local:esp32:esp32
```

Compilar el mismo firmware JWPLC Laundry que presentaba el fallo.

Resultado esperado:

```txt
COMPILACION OK
```

No deben aparecer errores de enlazado para:

```txt
jwplc_pinMode
jwplc_digitalWrite
jwplc_digitalRead
```

En salida verbose, `JW_MatrixButtons.cpp` debe compilarse desde fuente y no debe aparecer un archive precompilado de `JW_MatrixButtons/src/esp32`.

### B. JWPLC Basic

Seleccionar:

```txt
JWPLC Basic
FQBN esperado: jwplc_local:esp32:jwplcbasic
```

En el estado actual del package, Arduino IDE reporta como core efectivo:

```txt
jwcontrol_p2
```

Compilar al menos un sketch que active el autoload normal del JWPLC Basic.

Resultado esperado:

```txt
COMPILACION OK
```

Confirmar que `JW_MatrixButtons.cpp` se compila desde fuente. Confirmar la botonera en hardware en una prueba posterior con subida.

### C. JWPLC Basic Core

Seleccionar:

```txt
JWPLC Basic Core
FQBN esperado: jwplc_local:esp32:jwplcbasiccore
```

En el estado actual del package, Arduino IDE reporta como core efectivo:

```txt
jwcontrol
```

Compilar un sketch representativo compatible con esta variante.

Resultado esperado:

```txt
COMPILACION OK
```

### D. JW_SD desde fuente en target generico

Tras retirar el archive de `JW_SD`, compilar con `ESP32 Board` un sketch minimo que incluya y use la libreria:

```cpp
#include <JW_SD.h>

JW_SD sd;

void setup()
{
    (void)sd.isCardPresent();
}

void loop()
{
}
```

Resultado esperado:

```txt
COMPILACION OK
```

En salida verbose debe compilarse:

```txt
JW_SD/src/JW_SD.cpp
```

y no debe aparecer:

```txt
Using precompiled library in .../JW_SD/src/esp32
```

## Evidencia registrada

### 2026-08-22 - ESP32 Board / JWPLC Laundry

Resultado:

```txt
PASS
```

Configuracion observada:

```txt
FQBN: jwplc_local:esp32:esp32
Board: esp32
Core: esp32
Variant: esp32
Package local: 2.1.0-dev
```

El log Arduino IDE confirma que `JW_MatrixButtons` ya no se toma desde `src/esp32/libJW_MatrixButtons.a` y se compila desde:

```txt
JWPLC/2.1.0/libraries/JW_MatrixButtons/src/JW_MatrixButtons.cpp
```

Durante el link se utiliza:

```txt
libraries/JW_MatrixButtons/JW_MatrixButtons.cpp.o
```

No aparecen referencias indefinidas a:

```txt
jwplc_pinMode
jwplc_digitalWrite
jwplc_digitalRead
```

La compilacion completa genero correctamente ELF, BIN y merged BIN.

Resumen de memoria reportado por Arduino IDE:

```txt
Sketch: 492255 bytes (37%) de 1310720 bytes
RAM global: 34728 bytes (10%) de 327680 bytes
RAM libre estimada para variables locales: 292952 bytes
```

Esta prueba confirma la causa raiz original y valida la correccion para el target generico `ESP32 Board`.

### 2026-08-22 - JWPLC Basic / sketch vacio con autoload

Resultado:

```txt
PASS
```

Configuracion observada:

```txt
FQBN: jwplc_local:esp32:jwplcbasic
Board: jwplcbasic
Core reportado por Arduino IDE: jwcontrol_p2
Variant: jwplcbasic
Package local: 2.1.0-dev
JWPLC_BASIC: definido
```

El sketch vacio activo el autoload normal del target JWPLC Basic. El log confirma la deteccion e integracion de Display, GlobalPeripherals, RTC, FRAM, SD, Ethernet, RS-485, Modbus RTU y la botonera.

`JW_MatrixButtons` se compilo desde:

```txt
JWPLC/2.1.0/libraries/JW_MatrixButtons/src/JW_MatrixButtons.cpp
```

Durante el link se utiliza:

```txt
libraries/JW_MatrixButtons/JW_MatrixButtons.cpp.o
```

La compilacion completo correctamente el enlace y genero ELF, BIN y merged BIN.

Resumen de memoria reportado por Arduino IDE:

```txt
Sketch: 394329 bytes (9%) de 4063232 bytes
RAM global: 27612 bytes (8%) de 327680 bytes
RAM libre estimada para variables locales: 300068 bytes
```

La prueba valida que retirar el archive precompilado de `JW_MatrixButtons` no rompe la compilacion normal del target `JWPLC Basic`.

Nota: esta prueba valida compilacion. La operacion fisica de la botonera queda pendiente de una prueba de hardware con subida.

### 2026-08-22 - JWPLC Basic Core / sketch vacio con autoload

Resultado:

```txt
PASS
```

Configuracion observada:

```txt
FQBN: jwplc_local:esp32:jwplcbasiccore
Board: jwplcbasiccore
Core reportado por Arduino IDE: jwcontrol
Variant: jwplcbasic
Package local: 2.1.0-dev
JWPLC_BASIC: definido
JWPLC_HAS_RTC: 1
JWPLC_HAS_FRAM: 0
JWPLC_HAS_SD: 0
JWPLC_HAS_ETHERNET: 0
```

El log confirma que `JW_MatrixButtons` se compila desde fuente y no se reutiliza un archive `src/esp32/libJW_MatrixButtons.a`.

Durante el link se utiliza:

```txt
libraries/JW_MatrixButtons/JW_MatrixButtons.cpp.o
```

La compilacion completo correctamente el enlace, genero ELF, BIN y merged BIN y no reporto referencias `jwplc_*` no resueltas.

Resumen de memoria reportado por Arduino IDE:

```txt
Sketch: 339736 bytes (10%) de 3145728 bytes
RAM global: 26804 bytes (8%) de 327680 bytes
RAM libre estimada para variables locales: 300876 bytes
```

Esta prueba valida que la correccion tambien conserva compatibilidad con `JWPLC Basic Core`, que usa el core fuente `jwcontrol`.

### 2026-08-23 - Auditoria de simbolos de archives P1

Herramienta:

```txt
tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1
```

Toolchain observado:

```txt
xtensa-esp32-elf-nm.exe
esp-x32/2601
```

Resultado inicial:

| Libreria | Undefined | Acoplamiento `jwplc_*` | Resultado |
|---|---:|---|---|
| JW_RTC | 12 | ninguno | PASS |
| JW_FRAM | 22 | ninguno | PASS |
| JW_SD | 37 | `jwplc_digitalRead`, `jwplc_pinMode` | FAIL |
| JWPLC_ModbusRTU | 19 | ninguno | PASS |

Conclusion:

- `JW_RTC`, `JW_FRAM` y `JWPLC_ModbusRTU` no muestran el acoplamiento interno que causo la regresion;
- `JW_SD` si reproduce el mismo patron de incompatibilidad;
- `JW_SD` queda retirado de P1 y vuelve a compilacion desde fuente;
- la auditoria por defecto queda limitada a los archives P1 que permanecen activos.

La existencia de otros simbolos indefinidos de Arduino/ESP-IDF no se considera por si sola un fallo: se resuelven durante el link final. El criterio de esta auditoria es el acoplamiento a simbolos internos `jwplc_*`.

## Evidencia a guardar

Para cada prueba registrar:

- fecha;
- rama y commit;
- Arduino IDE usado;
- FQBN/placa seleccionada;
- sketch compilado;
- resultado;
- fragmento final del log verbose;
- tiempo de compilacion si se desea comparar rendimiento.

## Criterio para cerrar este ajuste

El ajuste puede considerarse validado cuando:

- [x] JWPLC Laundry compila con `ESP32 Board` sin referencias `jwplc_*` no resueltas.
- [x] JWPLC Basic compila correctamente tras retirar el precompilado de MatrixButtons.
- [x] JWPLC Basic Core compila correctamente tras retirar el precompilado de MatrixButtons.
- [x] Se auditaron los archives P1 y se identifico/retiró `JW_SD` por acoplamiento `jwplc_*`.
- [x] Los archives P1 que permanecen activos (`JW_RTC`, `JW_FRAM`, `JWPLC_ModbusRTU`) no presentan simbolos `jwplc_*` no resueltos.
- [ ] `JW_SD` compila correctamente desde fuente con `ESP32 Board`.
- [ ] JWPLC Basic conserva compilacion/autoload tras retirar el precompilado de `JW_SD`.
- [ ] La botonera conserva comportamiento funcional en JWPLC Basic.

## Pendiente de arquitectura

Este cambio no intenta volver a precompilar `JW_MatrixButtons` ni `JW_SD` de inmediato.

Antes de reintroducir cualquiera como archive comun para `esp32`, debe demostrarse que el binario es independiente del core o definirse una estrategia de precompilados que diferencie configuraciones ABI incompatibles.

No se propone reemplazar las llamadas `pinMode`/`digitalRead` de estas librerias por GPIO nativo del ESP32 sin una decision explicita, porque bajo JWPLC esos remapeos pueden formar parte de la semantica esperada de pines virtuales.

La prioridad de Alpha5 para este ajuste es compatibilidad y estabilidad, no recuperar a cualquier costo el ahorro de compilacion asociado a unos pocos translation units.
