# Gate 7NB.3C1 — Core 1 determinista / W5500 bloqueante en Core 0

## Objetivo

Comprobar con el acceptance distribuido completo que las esperas HTTP/NTP del W5500 pueden salir del Core 1 sin mover el resto de periféricos SPI ni modificar `JWPLC_ModbusRTU`.

La base funcional sigue siendo el sketch `20_alpha7_distributed_soak_2h_gate_7NB_async_master` y se conservan:

- Display/TFT y botonera;
- RTC;
- FRAM;
- microSD en Master;
- WiFi HTTP;
- BLE de commissioning;
- RS-485 / Modbus RTU;
- Ethernet W5500, DHCP, HTTP, NTP y rotación;
- TCA / Q -> I loopback;
- diagnósticos de heap, runtime, CRC y latencias.

## División de cores

### Core 1 — determinista

- `loopTask` Arduino;
- Modbus RTU;
- TCA / ScanIO;
- scheduler sincronizado de salidas;
- RTC;
- `JWPLC_Ethernet.service()` cooperativo;
- FRAM / SD;
- TFT.

### Core 0 — potencialmente bloqueante

- worker WiFi HTTP ya validado;
- worker W5500 `jw_w5500` para:
  - Ethernet HTTP;
  - Ethernet NTP.

`JWPLC_Ethernet.service()` **no se mueve a Core 0**. Se mantiene en Core 1 porque ya es una máquina cooperativa y así su estado/cache permanece en un solo core. El tick automático del core se difiere y el sketch llama `service()` únicamente cuando el worker W5500 no está ejecutando un job.

## Comunicación Core 1 <-> Core 0

Los jobs y resultados Ethernet usan FreeRTOS queues. No se usa `volatile` como mecanismo de mensajería entre cores.

Core 1 prepara un snapshot del JSON y encola el trabajo. Core 0 ejecuta la transacción y devuelve únicamente el resultado. Core 0 nunca llama `JWPLC_ModbusRTU.task()`.

## SPI compartido

No se mueve TFT, FRAM ni microSD a Core 0.

El worker W5500 toma el mutex global `jwplcSPI` antes de una operación HTTP/NTP y lo libera al terminar. Durante esa reserva:

- FRAM se difiere al siguiente loop;
- SD se difiere al siguiente loop;
- TFT conserva su lock normal y puede posponer un refresh;
- ScanIO, RTC, Modbus y scheduler continúan porque no dependen de SPI.

Esta primera implementación usa una **reserva gruesa** del SPI durante el job W5500 completo. Es deliberado para validar primero el beneficio sobre Core 1 sin modificar el backend W5x00. Si el gate demuestra que Core 1 queda limpio pero TFT/FRAM/SD sufren demasiado tiempo de espera, el paso siguiente será granularizar el lock dentro del backend W5x00.

## I/O sincronizado REV6

Se conservan los patrones del gate direccionado:

- all-off / all-on;
- chase Q0..Q7;
- `0xAA / 0x55` cruzados;
- wave M2 -> S1 -> S2;
- mirror patterns;
- modo `CLACK` 0x00 <-> 0xFF.

Los pasos duran al menos 650 ms para que el `IO_VERIFY_DEADLINE_MS=500` del acceptance pueda validar cada bitmap antes del siguiente patrón.

El trigger residual de `START` se consume al entrar al takeover para evitar el falso `ERR MODBUS` observado en REV5.

## Audio

```cpp
static constexpr uint8_t REV6_BUZZER_VOLUME = 24; // 0..255
```

Cada JWPLC deriva su nota del bitmap completo de salidas. Máscaras diferentes pueden producir notas diferentes aun cuando tengan la misma cantidad de bits activos.

## Comandos

En M2:

```text
START   -> inicia acceptance en SHOW
SHOW    -> modo patrones distribuidos
CLACK   -> modo 0x00/0xFF sincronizado
DIAG    -> diagnóstico base
SYNC    -> diagnóstico REV6
STATUS  -> estado base
STOP    -> detener
```

## Compile-check esperado

Al boot deben aparecer, entre otros:

```text
[SYNC REV6] apply task=PASS
[ETH WORKER] created targetCore=0
GATE_RT_REV=6 7NB-core1-deterministic-w5500-core0
[CORE] W5500 worker observed core=0
```

## Primera prueba física

No usar `ETHNEXT` en la primera pasada. Mantener el cable Ethernet en M2 y ejecutar `START` durante 2–3 minutos.

Criterio inicial:

- `syncFail=0`;
- I/O mismatch = 0;
- CRC M2/S1/S2 = 0;
- MB_FAIL = 0 o, como mínimo, sin repetición del patrón de starvation de ~450 ms;
- FRAM fail = 0;
- SD fail = 0;
- WiFi HTTP activo;
- Ethernet HTTP activo;
- RTC sync PASS;
- `ETH_WORKER core=0`;
- `MAX LOOP` de Core 1 claramente desacoplado de `ethWorkerMaxMs` / `ETH_LAT`.

La evidencia buscada es poder observar simultáneamente, por ejemplo:

```text
ethWorkerMaxMs = 400..900 ms
MAX_LOOP Core1 << ethWorkerMaxMs
```

Es decir: una red lenta puede seguir siendo lenta, pero deja de congelar el ciclo de control.

## Pendientes separados

- El handoff `ETHNEXT` aún conserva escrituras Modbus síncronas de owner y se valida en un gate posterior.
- La reserva SPI del W5500 es gruesa en esta revisión; granularizarla sólo si las mediciones lo justifican.
- No se modifica `JWPLC_ModbusRTU` en este gate.
