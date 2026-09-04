# Gate 7NB.3A-C — Soak async sincronizado + patrones + audio

Variante incremental del `20_alpha7_distributed_soak_2h_gate_7NB_async_master`.

## Objetivo

Validar dentro del acceptance completo que la secuencia de E/S sea gobernada por el Master y ejecutada de forma sincronizada en M2 y los Slaves, conservando el resto de periféricos y telemetría del soak 20.

Esta variante **no modifica `JWPLC_ModbusRTU`**. Consume la API Master cooperativa ya existente.

## Qué conserva del soak 20

El `.ino` incluye el soak 20 completo como base y mantiene sus rutas de:

- Display/TFT y botonera.
- FRAM stress.
- RTC, NTP y drift.
- microSD en Master.
- WiFi + HTTP worker en Core 0.
- Ethernet + HTTP + rotación de cable.
- BLE qualification.
- RS-485 y Modbus RTU cooperativo.
- telemetría, heap, uptime, max loop y CRC diagnostics.
- loopback físico Q0.x -> I0.x.

El cambio funcional se concentra en cómo se generan y aplican los patrones Q.

## Arquitectura I/O

El soak 20 original arrancaba un temporizador local en cada nodo. Aunque `IO_NODE_STAGGER_MS=0`, cada placa conservaba su propia fase y los relés terminaban audiblemente desincronizados.

En 7NB.3A-C:

1. M2 carga el siguiente `SyncPatternStep`.
2. Cada paso define un bitmap independiente para M2, S1 y S2.
3. M2 prepara cada Slave mediante FC06 direccionado, sin broadcast.
4. Cada Slave recibe bitmap, hint de canal y retardo relativo.
5. M2 y los Slaves programan la aplicación en una tarea `jwplcSyncIO` de prioridad alta en Core 1.
6. La tarea aplica el banco completo en el instante objetivo.
7. El loopback compara el bitmap físico completo y cuenta cada transición individual `0 -> 1` de Q/I.

La tarea dedicada evita que un bloqueo ocasional del loop Arduino por Ethernet desplace por cientos de milisegundos el instante de conmutación.

## Patrones SHOW

`START` inicia el soak completo y usa por defecto el modo `SHOW`.

Incluye:

- `all-off / all-on`.
- chase Q0..Q7 en las tres placas.
- `0xAA / 0x55` cruzado entre nodos.
- ola M2 -> S1 -> S2.
- patrones espejo `0x81 / 0x42 / 0x24 / 0x18`.

Por ejemplo:

```text
alternate-a  M2/S1/S2 = AA/55/AA
wave-s1      M2/S1/S2 = 00/FF/00
mirror-1     M2/S1/S2 = 81/42/24
```

Los tres bitmaps cambian en el mismo instante aunque sean diferentes.

### Comandos extra

En M2:

```text
START  -> inicia el soak completo en SHOW
CLACK  -> inicia/cambia a 00<->FF sincronizado
SHOW   -> vuelve al juego de patrones durante un soak en curso
STOP   -> usa el STOP normal del soak completo
```

`CLACK` no es un sketch aparte: usa el mismo acceptance completo y sólo cambia el generador de patrones.

## Audio por bitmap completo

Cada nodo genera una nota corta no bloqueante usando LEDC en el mismo instante de aplicación del bitmap.

La frecuencia se deriva de la firma completa de las ocho salidas, no solamente del número de bits activos. Por eso, por ejemplo, `0xAA` y `0x55` pueden producir notas distintas aunque ambos tengan cuatro relés ON.

La paleta usa frecuencias consonantes y los patrones iguales producen la misma nota. Patrones distintos pueden formar un trío de notas entre M2/S1/S2.

El OFF total (`0x00`) usa una nota grave de referencia.

### Volumen configurable

En el `.ino`:

```cpp
static constexpr uint8_t SOAK_BUZZER_VOLUME = 24; // 0..255
```

Referencia práctica para grabación:

| Valor | Uso |
|---:|---|
| 0 | mute |
| 18 | suave |
| 24 | recomendado para voz + test |
| 48 | fuerte |

El nivel se implementa con duty PWM LEDC; `tone()` por sí solo no permite controlar este nivel.

## Compile-check

El sketch incluye como base el soak 20 mediante `#include` y renombra su `setup()/loop()` para conservar todas las rutas ya validadas.

Esperado al boot de la revisión actual:

```text
[SYNC IO] apply task=PASS
GATE_RT_REV=5 7NB-sync-patterns-audio
BUZZER_VOLUME=24/255
SYNC_COMMANDS=START/SHOW/CLACK/STOP
```

Durante `START` debe aparecer:

```text
[SYNC IO] mode=SHOW
[SYNC IO] takeover=ON mode=SHOW volume=24/255
PATTERN seq=... name=... M2/S1/S2=0x../0x../0x.. tone=.../.../...Hz
```

## Criterio inicial PASS

- compile-check limpio de REV=5;
- tarea `jwplcSyncIO = PASS` en los tres nodos;
- clicks M2/S1/S2 audiblemente alineados en cada frontera de patrón;
- notas del trío alineadas con cada patrón;
- bitmaps distintos visibles en alternancias/ola/espejos;
- `MODBUS_CRC total/run = 0/0` en los tres;
- `MB_FAIL W/R/V = 0/0/0` durante la ventana corta;
- `IO mismatch = 0` y contadores Q/I coherentes;
- FRAM/RTC/SD/WiFi/Ethernet siguen activos como en soak 20.

Primera prueba física: 60-120 s. Si pasa, extender a 5-10 min antes de volver al endurance largo. No dejar el modo SHOW durante horas todavía: genera más ciclos de relé que el walking original.

## Pendiente separado

El Gate Ethernet/Core1 sigue abierto. Esta variante desacopla la **aplicación temporal de las salidas** del loop bloqueable, pero no considera resuelto el bloqueo HTTP Ethernet observado (~200-450 ms y un pico cercano a 1 s). Ese problema se corrige en el gate Ethernet, no aquí.
