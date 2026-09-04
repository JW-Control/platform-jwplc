# Estado — Gate 7NB.3C1 / REV6

Estado actual: **RUNTIME REV6 PASS CORTO / STOP SEGURO PASS / ETHNEXT PENDIENTE**.

## Implementado

- Core 1 conserva control determinista: Modbus, TCA/ScanIO, RTC y scheduler de Q.
- HTTP y NTP del W5500 pasan a worker Core 0.
- `JWPLC_Ethernet.service()` permanece cooperativo en Core 1 y se difiere mientras Core 0 usa W5500.
- comunicación Core1/Core0 mediante FreeRTOS queues.
- mutex SPI compartido conservado.
- FRAM y SD se difieren mientras el job W5500 reserva SPI, sin contarlo como fallo.
- TFT permanece en Core 1 con su mutex existente.
- patrones distribuidos SHOW/CLACK conservados.
- audio por bitmap con volumen configurable.
- trigger residual de START consumido al entrar al scheduler.
- arbitraje corregido para no abandonar a medias un poll Modbus cooperativo.
- separación no bloqueante de 4 ms entre FC06 del scheduler REV6.
- diagnóstico por transacción REV6: Slave, registro, valor, duración, timeout y rejects.
- `syncFail` ya no duplica el conteo de una misma transacción fallida.
- verifier I/O protegido por generación/token para no mezclar una muestra I anterior con un Q nuevo.
- transición segura de `STOP` a quiescencia Modbus con gap de 4 ms antes del cierre síncrono base.
- `JWPLC_ModbusRTU` sin cambios.

## TCA / I/O 0xFF — PASS

Los patrones `0xFF` expusieron el conflicto histórico entre dato válido y sentinel de error de `TCA6424A_readBank()`.

Corrección ya aplicada y revalidada:

- `TCA6424A_readBank()` retorna `bool` como estado de transacción;
- el bitmap completo se entrega por `*state`, incluido `0xFF`;
- `jwplcSystemScanIO()` acepta correctamente `0xFF`;
- `precompiled/core/JWPLCBASIC/core.a` fue regenerado desde `cores/jwcontrol` y verificado para el target Basic normal;
- patrones `all-on`, `wave-*` y demás secuencias pasan por `0xFF` sin conservar el bitmap anterior.

Resultado: **PASS**.

## Guard I/O por generación — PASS

Antes del guard, S1 registró falsos `ERR IO` al combinar el patrón nuevo con una muestra I que correspondía exactamente al patrón anterior, por ejemplo:

- `Q=0x00 / I=0xAA`;
- `Q=0x00 / I=0x42`.

La corrección REV6 usa `rev6AppliedToken` como generación, asocia cada muestra a `rev6InputSampleToken` y descarta cualquier captura que cambie de generación durante la verificación.

Revalidación física posterior:

- M2 `IO_MISMATCH=0`;
- S1 `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`;
- S2 `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`;
- se recorrieron repetidamente `all-on`, `alternate-*`, `wave-*`, `mirror-*` y `all-off` sin volver a generar los falsos mismatches;
- `ioRaceDiscard=0` en las corridas de cierre, por lo que el PASS no depende de descartar muestras de forma continua.

Resultado: **PASS**.

## Modbus durante RUN — PASS corto

Corrida de cierre previa:

- scheduler REV6: `apply=1107`, `txns=6645`, `fail=0`, `timeouts=0`, `rejects=0`;
- `maxTxnUs=95383`;
- polling cooperativo: `MB_POLL=6816/0`;
- `MB_FAIL W/R/V=0/0/0`;
- CRC M2/S1/S2: 0;
- M2 `FIRST_ERROR=NONE`, `ERROR_COUNT=0`.

Nueva corrida específica del gate STOP, antes de detener:

- `SYNC mode=SHOW apply=577 fail=0 txns=3470 timeouts=0 rejects=0 maxTxnUs=51398 gapMs=4`;
- polling cooperativo llegó a `MB_POLL=3761/0`;
- `MB_FAIL W/R/V=0/0/0`;
- CRC M2/S1/S2: 0;
- M2 seguía `STATE=RUNNING`, `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`.

Los timeouts aislados observados en corridas anteriores no reaparecieron en estas muestras con el gap de 4 ms.

Resultado para el runtime corto actual: **PASS**. No se modifica `JWPLC_ModbusRTU`.

## Split Core0/Core1 — PASS arquitectónico inicial

Durante RUN de la corrida específica de STOP:

- M2 `MAX_LOOP_US=151157`;
- `LONG_LOOP_250MS=0`;
- worker W5500 llegó a `maxJobMs=263 ms`;
- WiFi llegó a `max=417 ms`;
- S1 `MAX_LOOP_US=29872`, sin loops >50/250 ms;
- S2 `MAX_LOOP_US=30573`, sin loops >50/250 ms.

Esto confirma que Core 1 ya no copia directamente las esperas largas de red. Los picos restantes de ~151 ms siguen como optimización posterior.

## STOP seguro — PASS

El fallo previo era:

```text
[MODBUS FAIL] write S1 reg=77 err=Timeout
```

`reg=77` corresponde a `HR_ETH_OWNER`. El problema aparecía al entrar desde el runtime cooperativo REV6 a las escrituras Sync del cierre.

La corrección implementada en `20c`:

1. `STOP` durante RUN marca una solicitud pendiente.
2. Si existe una secuencia REV6 iniciada, se deja terminar sin iniciar un patrón nuevo.
3. Si existe un poll cooperativo iniciado, se deja terminar FC06/FC03.
4. Se drena cualquier resultado `masterBusy/masterDone` residual.
5. Una vez idle, se espera `REV6_STOP_QUIET_GAP_MS=4`.
6. Recién entonces se ejecuta `manualStopMaster()` conservando la semántica base.
7. La rotación Ethernet no inicia un nuevo handoff mientras el STOP está pendiente.

### Revalidación física

El `STOP` se forzó en una condición no trivial:

```text
[STOP REV6] requested stage=0 pollPhase=2 masterBusy=1
[STOP REV6] bus idle; quietGapMs=4
[STOP REV6] quiescent drainMs=21
[ETH] owner=NONE
SOAK_STOPPED
[SYNC REV6] takeover=OFF
[STOP REV6] complete
```

No apareció ningún `[MODBUS FAIL]` durante el cierre.

Estado posterior:

- M2: `STATE=READY`, `Q=0`, `I=0`, `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`, CRC 0;
- S1: `STATE=STOPPED`, `Q=0`, `I=0`, `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`, CRC 0;
- S2: `STATE=STOPPED`, `Q=0`, `I=0`, `IO_MISMATCH=0`, `FIRST_ERROR=NONE`, `ERROR_COUNT=0`, CRC 0.

Resultado funcional del gate: **PASS**.

### Latencia del comando STOP — REVIEW, no regresión de RUN

Después del `STOP`, M2 registró:

- `MAX_LOOP_US=460752`;
- `LONG_LOOP_250MS=1`.

Antes del `STOP`, la misma corrida mantenía `MAX_LOOP_US=151157` y `LONG_LOOP_250MS=0`.

El pico se clasifica por ahora como latencia del camino explícito/síncrono de cierre (`setEthernetOwner()` + `stopAllSlaves()` + restauración/log), no como bloqueo del runtime durante RUN. El gate funcional de STOP queda cerrado, pero la latencia del comando queda registrada como **REVIEW** y no se oculta.

## Siguiente mini-gate — ETHNEXT / handoff Ethernet

El siguiente pendiente es validar la rotación de propietario Ethernet con la arquitectura Core0/Core1 y la misma disciplina de quiescencia usada para STOP.

Objetivo:

- ejecutar `ETHNEXT` durante RUN;
- cambiar propietario M2 -> S1 -> S2 -> M2 sin introducir timeouts de `HR_ETH_OWNER`;
- mantener CRC 0, `MB_POLL fail=0`, `syncFail=0`, `syncTimeout=0` e `IO_MISMATCH=0`;
- comprobar que el worker W5500 sólo actúa en el nodo propietario y que los nodos no propietarios permanecen estables.

No iniciar endurance largo hasta cerrar este handoff.

## Pendientes posteriores

- identificar y reducir los picos restantes de Core 1 (~151 ms observados durante RUN);
- revisar si conviene reducir también la latencia síncrona del comando STOP (~461 ms observado fuera de RUN);
- decidir si granularizar el lock SPI interno del backend W5x00;
- endurance largo sólo después de cerrar estos mini-gates;
- antes de cerrar Alpha7, restaurar/auditar la estrategia precompilada de Modbus y repetir la validación multidrop correspondiente.
