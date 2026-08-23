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

El archive generado previamente para JWPLC Basic se habia compilado bajo:

```txt
Core: jwcontrol
JWPLC_BASIC definido
```

En ese contexto, `Arduino.h` remapea:

```txt
pinMode       -> jwplc_pinMode
digitalWrite  -> jwplc_digitalWrite
digitalRead   -> jwplc_digitalRead
```

Por ello `libJW_MatrixButtons.a` quedo enlazado contra simbolos exclusivos de `jwcontrol`. Arduino lo reutilizaba tambien para `ESP32 Board` porque el archive estaba ubicado en:

```txt
JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a
```

y ambas placas comparten `build.mcu=esp32`.

## Correccion Alpha5

Se aplica la correccion minima y conservadora:

1. retirar `libJW_MatrixButtons.a` generado bajo `jwcontrol`;
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
```

Compilar al menos un sketch que utilice la botonera o el display integrado.

Resultado esperado:

```txt
COMPILACION OK
```

Confirmar que la botonera sigue operativa en hardware si se realiza subida.

### C. JWPLC Basic Core

Seleccionar:

```txt
JWPLC Basic Core
```

Compilar un sketch representativo compatible con esta variante.

Resultado esperado:

```txt
COMPILACION OK
```

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

- [ ] JWPLC Laundry compila con `ESP32 Board` sin referencias `jwplc_*` no resueltas.
- [ ] JWPLC Basic compila correctamente.
- [ ] JWPLC Basic Core compila correctamente.
- [ ] La botonera conserva comportamiento funcional en JWPLC Basic.
- [ ] Se confirma que las demas librerias P1 precompiladas no introducen el mismo acoplamiento.

## Pendiente de arquitectura

Este cambio no intenta volver a precompilar `JW_MatrixButtons` de inmediato.

Antes de reintroducirla como archive comun para `esp32`, debe demostrarse que el binario es independiente del core o definirse una estrategia de precompilados que diferencie configuraciones ABI incompatibles.

La prioridad de Alpha5 para este ajuste es compatibilidad y estabilidad, no recuperar a cualquier costo el ahorro de compilacion asociado a un unico translation unit.
