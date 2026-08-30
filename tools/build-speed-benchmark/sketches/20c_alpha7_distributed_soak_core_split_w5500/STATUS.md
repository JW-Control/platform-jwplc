# Estado — Gate 7NB.3C1 / REV6

Estado actual: **PENDIENTE DE COMPILE-CHECK**.

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
- `JWPLC_ModbusRTU` sin cambios.

## Evidencia requerida siguiente

1. `git pull --ff-only`.
2. Compilar `20c_alpha7_distributed_soak_core_split_w5500.ino` en Arduino IDE.
3. No subir a placas hasta que compile limpio.
4. Si compila, cargar M2/S1/S2 y confirmar worker W5500 en Core 0.
5. Ejecutar 2–3 min con cable Ethernet fijo en M2 y sin `ETHNEXT`.
6. Capturar `DIAG`, `SYNC` y `STATUS`.

## Criterios iniciales

- I/O mismatch 0.
- CRC M2/S1/S2 0.
- `SYNC fail=0`.
- FRAM/SD fail 0.
- RTC sync PASS.
- WiFi y Ethernet HTTP activos.
- `ETH_WORKER core=0`.
- `MAX_LOOP` Core 1 deja de seguir a la latencia HTTP/NTP del W5500.

## Pendientes posteriores

- handoff/semántica de `ETHNEXT`;
- decidir si granularizar el lock SPI interno del backend W5x00;
- endurance largo sólo después de pasar este mini-gate.
