# Alpha14 — Politica AUTO de timing Modbus RTU

Fecha: 2026-09-26

## Decision

El usuario normal no debe tener que seleccionar manualmente el frame gap.

La configuracion objetivo del package es:

```cpp
JWPLC_ModbusRTU.begin(id, baud, config);
```

con seleccion automatica de timing segun:

```txt
baud + configuracion serial + perfil de timing
```

El motor sigue siendo independiente:

```cpp
JWPLC_ModbusRTU.motor(ASYNC); // default
JWPLC_ModbusRTU.motor(SYNC);  // compatibilidad
```

## Prioridad de configuracion

```txt
override manual
    >
perfil automatico validado
    >
fallback conservador
```

`setFrameGapUs()` y `setFrameGapMs()` se mantienen para diagnostico,
qualification y usuarios avanzados.

## Perfiles previstos

### STANDARD

Orientado a interoperabilidad con equipos Modbus RTU de terceros.

Debe seguir una politica conservadora compatible con la especificacion Modbus
Serial Line. Para baudrates superiores a 19200 no se deducira automaticamente
que 3.5 caracteres fisicos equivalen al t3.5 normativo; se conservara la
referencia fija recomendada por la especificacion hasta documentar formalmente
la politica final.

### JWPLC_FAST

Orientado a redes controladas JWPLC Master <-> expansiones JWPLC.

Puede usar timings menores solo cuando hayan sido caracterizados fisicamente
sobre ambos extremos del stack.

## Matriz de qualification

Los valores siguientes son estado de trabajo, no defaults finales:

| Baud | STANDARD | JWPLC_FAST | Estado |
|---:|---|---|---|
| 115200 | pendiente decision final | 500 us candidato con margen | caracterizado |
| 250000 | pendiente | pendiente | baud limpio |
| 460800 | pendiente | pendiente | baud limpio |
| 500000 | pendiente | H2 en curso | baud limpio |
| 230400 | no recomendado provisional | no recomendado provisional | fallo reproducible |

## H2

H2 mantiene:

```txt
BAUD=500000
CONFIG=8N1
MOTOR=ASYNC
TX=QUEUED
TCP=OFF
```

y busca el piso tecnico del gap en una red JWPLC <-> JWPLC.

Ese piso no se convertira automaticamente en el valor de producto. Despues se
elegira un valor con margen y se confirmara bajo TCP500/long-run antes de llenar
la celda JWPLC_FAST de 500 kbaud.
