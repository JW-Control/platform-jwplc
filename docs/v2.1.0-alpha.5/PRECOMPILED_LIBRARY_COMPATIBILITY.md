# Alpha5 - Compatibilidad de librerias precompiladas

## Objetivo

Corregir la incompatibilidad detectada al reutilizar `JW_MatrixButtons` precompilada entre las placas del package que comparten `build.mcu=esp32` pero usan cores distintos.

Caso que expuso la regresion:

```txt
JWPLC Laundry
FQBN: jwplc_local:esp32:esp32
Core: esp32
Variant: esp32
```

El archive generado previamente para JWPLC Basic se habia compilado bajo el entorno JWPLC, con `JWPLC_BASIC` definido y los remapeos de GPIO activos.

En ese contexto, `Arduino.h` remapea:

```txt
pinMode       -> jwplc_pinMode
digitalWrite  -> jwplc_digitalWrite
digitalRead   -> jwplc_digitalRead
```

Por ello `libJW_MatrixButtons.a` quedo enlazado contra simbolos exclusivos del core JWPLC. Arduino lo reutilizaba tambien para `ESP32 Board` porque el archive estaba ubicado en:

```txt
JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a
```

y ambas placas comparten `build.mcu=esp32`.

## Correccion Alpha5

Se aplica la correccion minima y conservadora:

1. retirar `libJW_MatrixButtons.a` generado bajo el entorno JWPLC;
2. retirar `precompiled=full` de `JW_MatrixButtons/library.properties`;
3. excluir `JW_MatrixButtons` de la lista P1 de `Build-JWPLCPrecompiledLibraries.ps1`.

No se modifica la API publica de `JW_MatrixButtons`.
No se modifica el firmware JWPLC Laundry.
No se modifica `platform.txt`.
No se modifica `build.mcu`.
No se desactiva la precompilacion de las demas librerias P1.

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
- [x] JWPLC Basic compila correctamente.
- [x] JWPLC Basic Core compila correctamente.
- [ ] La botonera conserva comportamiento funcional en JWPLC Basic.
- [ ] Se confirma que las demas librerias P1 precompiladas no introducen el mismo acoplamiento.

## Pendiente de arquitectura

Este cambio no intenta volver a precompilar `JW_MatrixButtons` de inmediato.

Antes de reintroducirla como archive comun para `esp32`, debe demostrarse que el binario es independiente del core o definirse una estrategia de precompilados que diferencie configuraciones ABI incompatibles.

La prioridad de Alpha5 para este ajuste es compatibilidad y estabilidad, no recuperar a cualquier costo el ahorro de compilacion asociado a un unico translation unit.
