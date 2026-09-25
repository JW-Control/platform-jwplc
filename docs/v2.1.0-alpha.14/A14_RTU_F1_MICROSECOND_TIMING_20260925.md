# Alpha14 — RTU-F1 — Motor temporal en microsegundos

Fecha: 2026-09-25

## Objetivo

Migrar exclusivamente la delimitacion temporal de tramas de
`JWPLC_ModbusRTU` desde `millis()` a `micros()`, sin romper APIs existentes
ni cambiar todavia baudrate, politica automatica de gap, TX bloqueante,
autoservice o scheduler multislave.

## Cambio de package

Se mantienen `setFrameGapMs()` y `frameGapMs()`. Se anaden
`setFrameGapUs()` y `frameGapUs()`.

El default continua siendo 5 ms. La deteccion de silencio usa resta unsigned
sobre `micros()`.

## Compatibilidad

- `setFrameGapMs(2)` equivale internamente a 2000 us.
- El Master de benchmark conserva la API historica.
- El Slave de benchmark ejercita la API nueva.
- `frameGapMs()` redondea hacia arriba cuando el gap fue fijado en us.
- No cambia `begin()`, baudrate default ni timeout Master.

## Precompilacion

La libreria usa `precompiled=full`; el archive versionado aun es el previo a
RTU-F1. El gate fisico debera forzar compilacion desde fuente, restaurar el
archive previo y verificar que ambos firmwares se construyeron con
`JWPLC_ModbusRTU.cpp`.

Si RTU-F1 pasa, se regenerara y adoptara el nuevo archive antes de considerar
productizado el cambio.

## Baseline con GND anterior

```txt
TCP500 + RTU unpaced: TCP=500.000 req/s, RTU=198.274 Hz
TCP OFF + RTU unpaced: RTU=214.288 Hz
```
