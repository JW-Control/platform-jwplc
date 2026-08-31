# Estado — Gate 7NB.3C1 / REV6

Estado actual: **TCA 0xFF REVALIDADO / GUARD I/O POR GENERACIÓN PENDIENTE DE COMPILE-CHECK Y PRUEBA FÍSICA**.

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
- `JWPLC_ModbusRTU` sin cambios.

## Evidencia REV6 / split Core0-Core1

- compile-check Arduino IDE de REV6 previo: PASS.
- mismo sketch cargado en M2/S1/S2: PASS de arranque.
- worker W5500 separado del loop de control: mejora observable.
- el `MAX_LOOP` de Core 1 dejó de seguir directamente las esperas W5500 de ~450 ms vistas antes del split.
- en corridas cortas posteriores, S1/S2 se mantuvieron típicamente por debajo de ~50–57 ms de max loop y sin loops >250 ms.
- M2 conserva un pico aislado >250 ms pendiente de localizar con precisión; no se atribuye todavía a W5500 ni a una causa cerrada.

## TCA / I/O 0xFF — corregido y revalidado

Los patrones con las ocho entradas activas (`0xFF`) expusieron un bug del runtime del package:

- `TCA6424A_readBank()` retornaba `uint8_t`;
- `0xFF` se usaba simultáneamente como dato válido y como sentinel de error;
- `jwplcSystemScanIO()` interpretaba una lectura válida `0xFF` como fallo y conservaba el bitmap DI anterior.

Corrección aplicada en el package:

- `TCA6424A_readBank()` ahora retorna `bool` como estado de la transacción;
- el dato completo, incluido `0xFF`, se entrega exclusivamente por `*state`;
- `jwplcSystemScanIO()` evalúa el `bool` y acepta `0xFF` como bitmap válido;
- `precompiled/core/JWPLCBASIC/core.a` fue regenerado desde `cores/jwcontrol` y verificado con el gate normalizado del core precompilado.

Revalidación física:

- `all-on` y patrones `wave-*` pasan por `0xFF` sin conservar el bitmap anterior;
- M2 mantuvo `IO_MISMATCH=0` durante las corridas observadas;
- visualmente las entradas/salidas de TFT acompañaron los patrones;
- el bug específico `0xFF` se considera cerrado.

## Hallazgo nuevo — race del verifier I/O REV6

S1 registró falsos `ERR IO` con lecturas que coincidían exactamente con el patrón anterior:

- `Q=0x00 / I=0xAA` al salir de un patrón donde S1 estaba en `0xAA`;
- `Q=0x00 / I=0x42` al salir de un patrón donde S1 estaba en `0x42`.

S2 también llegó a latchear `ERR IO` durante la misma clase de prueba.

La evidencia apunta a una carrera entre:

- `rev6SyncApplyTask()` de prioridad alta, que aplica Q y publica la nueva fase;
- `rev6ServiceLoopback()` en `loop()`, que podía conservar una muestra I de la generación anterior y combinarla con `phaseStart/expected` de la nueva generación.

Corrección REV6 aplicada en el sketch:

- `rev6AppliedToken` pasa a actuar como marcador de generación;
- token `0` significa estado en actualización/no verificable;
- la nueva generación se publica sólo después de completar Q + metadata;
- cada muestra I queda asociada a `rev6InputSampleToken`;
- el verifier sólo compara `expected`, tiempo de fase e input si pertenecen al mismo token;
- si la generación cambia durante la captura, la muestra se descarta en vez de generar `IO FAIL`;
- nuevo contador diagnóstico `ioRaceDiscard`;
- el commit del mismatch se protege con un guard corto de estado para evitar latchear una generación que cambió durante la decisión.

## Modbus scheduler REV6 — pendiente separado

El bus Modbus normal continúa muy estable:

- CRC M2/S1/S2: 0 en las muestras revisadas;
- polling cooperativo normal llegó a miles de operaciones con `MB_POLL fail=0`;
- `MB_FAIL W/R/V=0/0/0`.

Sin embargo, el scheduler sincronizado aún mostró timeouts aislados en sus FC06:

- corrida observada: `txns=5980`, `timeouts=4`, `rejects=0`;
- orden de magnitud: ~0.067 % de timeout del scheduler;
- se observaron fallos en `HR_CMD_ARG0` (`reg=73`) tanto hacia S1 como S2;
- no se aumenta todavía `MODBUS_TIMEOUT_MS=250` para no ocultar el síntoma;
- no se modifica todavía `JWPLC_ModbusRTU`.

Este pendiente se tratará después de cerrar el guard I/O por generación.

## Evidencia requerida siguiente

1. `git pull --ff-only`.
2. Compilar nuevamente `20c_alpha7_distributed_soak_core_split_w5500.ino`.
3. Confirmar en boot `SYNC_IO_GENERATION_GUARD=ON` y `SYNC_FC06_GAP_MS=4`.
4. Cargar el mismo sketch en S1, S2 y M2.
5. Ejecutar una corrida corta de 10–15 min con Ethernet fijo en M2 y sin `ETHNEXT`.
6. Observar especialmente S1/S2 y confirmar ausencia de `IO FAIL` falso al cambiar `0xAA -> 0x00`, `0x42 -> 0x00` y otros cambios de patrón.
7. Capturar `DIAG`, `SYNC` y `STATUS` de M2; `DIAG` y `STATUS` de S1/S2 si aparece algún error.

## Criterios del mini-gate actual

- M2/S1/S2 `IO_MISMATCH=0`.
- `ioRaceDiscard` puede ser mayor que cero y se considera evidencia útil de muestras descartadas correctamente.
- `0xFF` continúa válido sin regresión.
- CRC M2/S1/S2 = 0.
- FRAM/SD fail = 0.
- RTC sync PASS.
- WiFi y Ethernet HTTP activos.
- `ETH_WORKER core=0`.
- no introducir regresiones de Core 1.

## Pendientes posteriores

- resolver los timeouts aislados del scheduler FC06 REV6 sin tocar aún la librería Modbus;
- identificar el origen del pico aislado >250 ms visto en M2;
- handoff/semántica de `ETHNEXT`;
- decidir si granularizar el lock SPI interno del backend W5x00;
- endurance largo sólo después de pasar estos mini-gates.
