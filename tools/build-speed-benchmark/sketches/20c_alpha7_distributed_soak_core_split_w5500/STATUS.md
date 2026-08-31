# Estado — Gate 7NB.3C1 / REV6

Estado actual: **RUNTIME REV6 PASS CORTO / STOP SEGURO PENDIENTE DE COMPILE-CHECK Y REVALIDACIÓN FÍSICA**.

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
- `ioRaceDiscard=0` en la corrida de cierre, por lo que el PASS no depende de descartar muestras de forma continua.

Resultado: **PASS**.

## Modbus durante RUN — PASS corto

Snapshot final durante `STATE=RUNNING`:

- scheduler REV6: `apply=1107`, `txns=6645`, `fail=0`, `timeouts=0`, `rejects=0`;
- `maxTxnUs=95383`;
- polling cooperativo: `MB_POLL=6816/0`;
- `MB_FAIL W/R/V=0/0/0`;
- CRC M2/S1/S2: 0;
- M2 `FIRST_ERROR=NONE`, `ERROR_COUNT=0`.

Los timeouts aislados observados en una corrida anterior no reaparecieron en esta muestra de 6645 transacciones REV6 con el gap de 4 ms.

Resultado para el runtime corto actual: **PASS**. No se modifica `JWPLC_ModbusRTU`.

## Split Core0/Core1 — PASS arquitectónico inicial

Durante la corrida de cierre:

- WiFi llegó a `max=555 ms`;
- worker W5500 llegó a `maxJobMs=227 ms`;
- M2 `MAX_LOOP_US=151797`;
- `LONG_LOOP_250MS=0`;
- S1 `MAX_LOOP_US=44303`, sin loops >50/250 ms;
- S2 `MAX_LOOP_US=42042`, sin loops >50/250 ms.

Esto confirma que Core 1 ya no copia directamente las esperas largas de red. Los picos restantes de ~151 ms siguen como optimización posterior.

## Hallazgo al ejecutar STOP — pendiente separado

Después del snapshot limpio de RUN, el comando `STOP` produjo:

```text
[MODBUS FAIL] write S1 reg=77 err=Timeout
```

`reg=77` corresponde a `HR_ETH_OWNER`.

El fallo apareció después de 6645/6645 transacciones REV6 y 6816/6816 polls normales correctos, con CRC 0. Por tanto se clasifica como problema de transición entre el runtime cooperativo REV6 y las escrituras Sync usadas por el cierre, no como regresión del bus durante RUN.

El flujo base de `manualStopMaster()` hace primero `setEthernetOwner(ETH_OWNER_NONE)` y luego `stopAllSlaves()`. `setEthernetOwner()` usa `masterWriteRegister()` síncrono. Aunque `prepareMasterForSyncCommand()` drena una transacción cooperativa en curso, no deja el silencio de 4 ms que sí estabilizó el scheduler REV6.

## Corrección STOP segura — implementada, pendiente de revalidación

Se agregó en `20c` una transición explícita a quiescencia antes de invocar el `STOP` base:

1. `STOP` en Master durante RUN sólo marca `rev6StopRequested`.
2. Si existe una secuencia REV6 iniciada, se deja terminar sin iniciar un patrón nuevo.
3. Si existe un poll cooperativo iniciado, se deja terminar FC06/FC03.
4. Se drena cualquier resultado `masterBusy/masterDone` residual.
5. Una vez realmente idle, se espera `REV6_STOP_QUIET_GAP_MS=4`.
6. Recién entonces se ejecuta el `manualStopMaster()` existente, conservando `setEthernetOwner()`, `stopAllSlaves()`, FRAM y semántica base.
7. La rotación Ethernet no inicia un nuevo handoff mientras el STOP está pendiente.

Diagnóstico agregado:

- boot: `STOP_SAFE_GAP_MS=4`;
- solicitud: `[STOP REV6] requested stage=... pollPhase=... masterBusy=...`;
- quiescencia: `[STOP REV6] bus idle; quietGapMs=4`;
- cierre: `[STOP REV6] quiescent drainMs=...` y `[STOP REV6] complete`;
- `SYNC`: `STOP pending/lastMs/maxMs/count=...`.

`JWPLC_ModbusRTU` no fue modificado.

## Siguiente mini-gate

1. `git pull --ff-only`.
2. Compilar `20c_alpha7_distributed_soak_core_split_w5500.ino`.
3. Confirmar en boot:
   - `SYNC_FC06_GAP_MS=4`;
   - `SYNC_IO_GENERATION_GUARD=ON`;
   - `STOP_SAFE_GAP_MS=4`.
4. Cargar el mismo sketch en S1, S2 y M2.
5. Ejecutar RUN corto de 3–5 min con Ethernet fijo en M2 y sin `ETHNEXT`.
6. Ejecutar `STOP` desde M2 mientras los patrones están activos.
7. Confirmar ausencia de `[MODBUS FAIL] write S1 reg=77` y cualquier otro error de cierre.
8. Capturar `SYNC` y `STATUS` de M2 después del STOP; `STATUS` de S1/S2.

## Criterios del mini-gate STOP

- `STOP` termina en `SOAK_STOPPED`.
- aparece `[STOP REV6] complete`.
- no aparece ningún `[MODBUS FAIL]` durante el cierre.
- M2/S1/S2 quedan con Q=0 e I=0.
- S1/S2 quedan `STATE=STOPPED`.
- CRC M2/S1/S2 = 0.
- no se introduce `ERR IO` ni una regresión del guard por generación.

## Pendientes posteriores

- identificar y reducir los picos restantes de Core 1 (~151 ms observados);
- probar y cerrar `ETHNEXT`/handoff entre nodos con la arquitectura Core0/Core1;
- decidir si granularizar el lock SPI interno del backend W5x00;
- endurance largo sólo después de cerrar estos mini-gates;
- antes de cerrar Alpha7, restaurar/auditar la estrategia precompilada de Modbus y repetir la validación multidrop correspondiente.
