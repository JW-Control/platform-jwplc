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


## Gate fisico RTU-F1

El gate versionado fuerza build fuente ocultando temporalmente el archive
precompilado y restaura su SHA al terminar.

Casos:

```txt
TCP500 + RTU unpaced, 60 s
TCP OFF + RTU unpaced, 60 s
```

Criterios minimos:

- TCP500 >=99% del target;
- RTU con TCP500 >=180 tx/s;
- RTU con TCP OFF >=195 tx/s;
- cero CRC/timeouts/fallos;
- SD/perifericos limpios;
- ambos TFT estables.


## Correccion de tooling posterior al primer intento

El primer RTU-F1 fisico compilo, enlazo, subio ambos firmwares y paso el
preflight fisico, pero el gate se detuvo al buscar literalmente
`JWPLC_ModbusRTU.cpp` dentro de un log de Arduino CLI no verbose.

Esto es un falso negativo de tooling, no un fallo de producto. La verificacion
se corrige para buscar el objeto de compilacion `JWPLC_ModbusRTU.cpp.o`
dentro del build path temporal.

Como el firmware F1 ya fue cargado correctamente durante ese intento, se anade
un gate `measure_loaded_firmware` que no compila ni sube: solo ejecuta los dos
casos fisicos y verifica mediante snapshots que Master y Slave reporten
`RTU_FRAME_GAP_US=2000`.

## Resultado fisico RTU-F1

```txt
TCP500: TCP=499.920 req/s, RTU=170.432 Hz
TCP OFF: RTU=190.275 Hz
MASTER_GAP_US=2000
SLAVE_GAP_US=2000
TCP_CLEAN=YES
RTU_CLEAN=YES
RUNTIME_CLEAN=YES
```

Contra el baseline inmediato con GND:

| Caso | Baseline millis + 2 ms | RTU-F1 micros + 2000 us exactos | Delta |
|---|---:|---:|---:|
| TCP500 | 198.274 Hz | 170.432 Hz | -14.04 % |
| TCP OFF | 214.288 Hz | 190.275 Hz | -11.21 % |

RTU-F1 queda como base de precision temporal, con regresion de rendimiento
caracterizada. El archive precompilado no se adopta aun.

