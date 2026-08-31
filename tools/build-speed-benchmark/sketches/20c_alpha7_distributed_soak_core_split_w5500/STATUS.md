# Estado — Gate 7NB.3C1 / REV6

Estado actual: **COMPILE-CHECK PASS / CORRECCIÓN TCA PENDIENTE DE REVALIDACIÓN FÍSICA**.

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

## Evidencia REV6 inicial

- compile-check Arduino IDE: PASS.
- mismo sketch cargado en M2/S1/S2: PASS de arranque.
- worker W5500 separado del loop de control: mejora observable.
- `MAX_LOOP` Core 1 bajó aproximadamente de ~460 ms a ~149 ms en la primera corrida.
- long loops >250 ms: 0 en la corrida observada.
- `MB_FAIL W/R/V=0/0/0` durante la muestra revisada.
- CRC M2/S1/S2: 0 durante la muestra revisada.
- `syncFail=0` durante los patrones distribuidos.

## Hallazgo I/O

Los patrones con las ocho entradas activas (`0xFF`) expusieron un bug del runtime del package:

- `TCA6424A_readBank()` retornaba `uint8_t`;
- `0xFF` se usaba simultáneamente como dato válido y como sentinel de error;
- `jwplcSystemScanIO()` interpretaba una lectura válida `0xFF` como fallo y conservaba el bitmap DI anterior.

Corrección aplicada en el package:

- `TCA6424A_readBank()` ahora retorna `bool` como estado de la transacción;
- el dato completo, incluido `0xFF`, se entrega exclusivamente por `*state`;
- `jwplcSystemScanIO()` evalúa el `bool` y acepta `0xFF` como bitmap válido.

## Evidencia requerida siguiente

1. `git pull --ff-only`.
2. Asegurar que el package local `jwplc_local:esp32:2.1.0-dev` use las fuentes corregidas del branch antes de recompilar.
3. Compilar nuevamente `20c_alpha7_distributed_soak_core_split_w5500.ino`.
4. Cargar M2/S1/S2.
5. Ejecutar 2–3 min con Ethernet fijo en M2 y sin `ETHNEXT`.
6. Confirmar especialmente `Q=0xFF / I=0xFF` sin `IO FAIL`.
7. Capturar `DIAG`, `SYNC` y `STATUS`.

## Criterios iniciales

- I/O mismatch 0, incluyendo `0xFF`.
- CRC M2/S1/S2 0.
- `SYNC fail=0`.
- FRAM/SD fail 0.
- RTC sync PASS.
- WiFi y Ethernet HTTP activos.
- `ETH_WORKER core=0`.
- `MAX_LOOP` Core 1 no vuelve a seguir la latencia HTTP/NTP del W5500.

## Pendientes posteriores

- identificar y reducir los picos restantes de Core 1 (~149 ms observados en esta primera corrida);
- handoff/semántica de `ETHNEXT`;
- decidir si granularizar el lock SPI interno del backend W5x00;
- endurance largo sólo después de pasar este mini-gate.
