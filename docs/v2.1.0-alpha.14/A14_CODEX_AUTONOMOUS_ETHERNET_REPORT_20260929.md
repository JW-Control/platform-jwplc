# Alpha14 — Reporte autónomo Ethernet Codex

Fecha: `2026-09-29`

Rama única de trabajo:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

## CURRENT_STATE

```text
LAST_COMPLETED_GATE=H4A0.4-P9
LAST_RESULT=PASS_DATA_ONLY_LARGER_BATCH_REJECTED_FAIRNESS
CURRENT_GATE=A2
CURRENT_ACTION=DESIGN_TCP_ASYNC_TX_WITHOUT_RX_CHANGES
FIFO_REUSE_DEFAULT=OFF
FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES
PHYSICAL_STABILITY=PENDING_USER
ALPHA14_CLOSED=NO
```

## Resultados

| Gate | Baseline | Candidate | Payload Mbps | TCP Mbps | Delta | SPI errors | FNV | Resets | Physical | Decisión |
|---|---|---|---:|---:|---:|---:|---|---:|---|---|
| H4A0.4-P3 | DIRECT_RX 15.970 / 12.887425 | FIFO_REUSE | 16.804 | 13.736151 | +5.226% payload | 0 | PASS | 0 observados | PENDING_USER | FIFO_REUSE_GAIN_CONFIRMED; abrir P3R |
| H4A0.4-P3R | DIRECT_RX 15.965 / 12.830344 | FIFO_REUSE | 16.785 | 12.976148 | +5.141% payload | 0 | PASS | 0 | PENDING_USER | FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES; pasar a P4 |
| H4A0.4-P4 | FIFO_REUSE sin profiler | FIFO_REUSE + microperfil | — | — | locator only | 0 | PASS | 0 | PENDING_USER | START_WAIT_EXCESS dominante; evaluar DMA segura |
| H4A0.4-P5 | SPIClass/VSPI compartido | DMA ESP-IDF | — | — | no ejecutado | — | — | — | PENDING_USER | Bloqueado: DMA exige segundo ownership/rearquitectura |
| H4A0.4-P6 | available()+read() | read() directo | 16.853677 | 13.424499 | +0.348% payload; −0.887% path RX | 0 | PASS | 0 | PENDING_USER | Efecto pequeño/inconcluso; pasar a commit RX |
| H4A0.4-P7 | commit inmediato | commit coalescido | 16.883698 | 9.727778 | +0.513% payload; +11.266% path RX | 0 | PASS | 0 | PENDING_USER | Rechazar: regresión path RX/hold SPI; pasar a socketStatus |
| H4A0.4-P8 | doble connected() | resultado reutilizado en la misma pasada | 16.770394 | 14.129575 | −0.305% payload; −4.556% scheduler | 0 | PASS | 0 | PENDING_USER | Ganancia confirmada; pasar a batch raw |
| H4A0.4-P9 | batch 8 | batch 16/32 | 16.834180 máx. | 14.913620 máx. | +0.176% payload; +84.099% hold máx. (16) | 0 | PASS | 0 | PENDING_USER | Rechazar 16/32 por fairness; cerrar RX |

## HEAD y commits de la sesión

HEAD inicial sincronizado:

```text
9569be3f2d4bd0f9daeb6d02f9989c15b28b498f
```

Commits creados hasta P7:

```text
c9666d0d docs(alpha14): registrar resultado P3 FIFO reuse
bd38f7c5 test(alpha14): añadir confirmación P3R FIFO reuse
47958b90 docs(alpha14): confirmar P3R FIFO reuse
115e8b07 test(alpha14): perfilar overhead por chunk SPI W5500
1b6abff1 docs(alpha14): registrar microperfil P4 W5500
6b3e9a3d docs(alpha14): cerrar factibilidad DMA P5
d13df201 test(alpha14): medir fusión TCP available read
0d81daa6 docs(alpha14): registrar resultado P6 available read
59fb264a test(alpha14): medir commit TCP RX coalescido
3a043347 test(alpha14): aislar reconexiones P7
de5cf907 docs(alpha14): registrar resultado P7 commit RX
526690c2 test(alpha14): medir redundancia socketStatus TCP
e968afdf docs(alpha14): registrar resultado P8 socketStatus
abdb5ab0 test(alpha14): medir batch raw TCP RX
```

## Gates ejecutados

### Contrato package/source-first

```text
A14_PACKAGE_PROMOTION_CONTRACT=PASS
PACKAGE_DEVELOPMENT_MODE=SOURCE_FIRST
SPI_ETHERNET_SETTINGS=26000000
```

### H4A0.4-P3

```text
ONLY_VARIABLE=DUMMY_FIFO_REFILL_PER_64B_RX_CHUNK
PAYLOAD_INTEGRITY=PASS
DIRECT_RX_PAYLOAD=15.970 Mbps
FIFO_REUSE_PAYLOAD=16.804 Mbps
PAYLOAD_DELTA=+5.226%
DIRECT_RX_US_PER_BYTE=0.500949
FIFO_REUSE_US_PER_BYTE=0.476072
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
PHYSICAL_STABILITY=PENDING_USER
RESULT=PASS_DATA_ONLY
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3_fifo_reuse_wkj1h336
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3_fifo_reuse_wkj1h336\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P3_W5500_FIFO_REUSE_20260929.md
```

### H4A0.4-P3R

```text
ONLY_VARIABLE=DUMMY_FIFO_REFILL_PER_64B_RX_CHUNK
RUNS_PER_VARIANT=5
PAYLOAD_INTEGRITY=PASS
DIRECT_RX_PAYLOAD=15.965 Mbps
FIFO_REUSE_PAYLOAD=16.785 Mbps
PAYLOAD_DELTA=+5.141%
DIRECT_RX_US_PER_BYTE=0.501105
FIFO_REUSE_US_PER_BYTE=0.476604
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES
PHYSICAL_STABILITY=PENDING_USER
RESULT=PASS_DATA_ONLY
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3r_fifo_reuse_xp83g134
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3r_fifo_reuse_xp83g134\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P3R_W5500_FIFO_REUSE_CONFIRMATION_20260929.md
```

### H4A0.4-P4

```text
ONLY_VARIABLE=CHUNK_TIMING_INSTRUMENTATION
CHUNK_COUNT=87227
BYTES=4504177
AVG_CHUNK_BYTES=51.637
SETUP_TOTAL_US=72830
WIRE_WAIT_TOTAL_US=2051640
COPY_OUT_TOTAL_US=157270
OTHER_TOTAL_US=52038
IDEAL_WIRE_US=1385900.615
EXCESS_OVER_WIRE_US=947877.385
WIRE_WAIT_EXCESS_US=665739.385
DOMINANT_TOTAL_BLOCK=WIRE_WAIT
DOMINANT_ACTIONABLE_BLOCK=START_WAIT_EXCESS
PAYLOAD_INTEGRITY=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
PHYSICAL_STABILITY=PENDING_USER
RESULT=PASS_DATA_ONLY
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p4_chunk_profile_o0jaj78t
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p4_chunk_profile_o0jaj78t\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P4_W5500_CHUNK_PROFILE_20260929.md
```

### H4A0.4-P5

```text
P5_DMA_CANDIDATE_IMPLEMENTED=NO
ARDUINO_HAL_DMA_REUSE_API=ABSENT
IDF_DMA_REQUIRES_SPI_BUS_INITIALIZE=YES
IDF_DMA_REQUIRES_SPI_DEVICE_HANDLE=YES
SECOND_SPI_OWNER_ALLOWED=NO
SPI_BUS_REINITIALIZATION_ALLOWED=NO
P5_DMA_BLOCK_REASON=NO_SHARED_OWNERSHIP_BRIDGE_BETWEEN_ARDUINO_HAL_AND_IDF_DMA
PRODUCT_FAILURE=NO
PHYSICAL_STABILITY=PENDING_USER
NEXT=PHASE4_AVAILABLE_READ
```

Evidencia:

```text
docs/v2.1.0-alpha.14/A14_H4A04P5_DMA_SHARED_BUS_FEASIBILITY_20260929.md
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\hardware\esp32\2.1.0-dev\cores\jwcontrol\esp32-hal-spi.c
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\hardware\esp32\2.1.0-dev\cores\jwcontrol\esp32-hal-spi.h
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp32-libs\3.3.8\include\esp_driver_spi\include\driver\spi_common.h
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp32-libs\3.3.8\include\esp_driver_spi\include\driver\spi_master.h
```

### H4A0.4-P6

```text
ONLY_VARIABLE=PRE_READ_AVAILABLE_PROBE
AVAILABLE_READ_PAYLOAD=16.795211 Mbps
READ_DIRECT_PAYLOAD=16.853677 Mbps
PAYLOAD_DELTA=+0.348%
AVAILABLE_READ_RX_PATH=0.544076 us/B
READ_DIRECT_RX_PATH=0.539249 us/B
RX_PATH_DELTA=-0.887%
AVAILABLE_READ_HOLD=0.568248 us/B
READ_DIRECT_HOLD=0.569987 us/B
HOLD_DELTA=+0.306%
AVAILABLE_CALLS=33930 -> 0
PAYLOAD_INTEGRITY=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
INTERPRETATION=AVAILABLE_READ_SMALL_OR_INCONCLUSIVE_EFFECT
PHYSICAL_STABILITY=PENDING_USER
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p6_available_read_p7e0qvmt
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p6_available_read_p7e0qvmt\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P6_TCP_AVAILABLE_READ_20260929.md
```

### H4A0.4-P7

```text
ONLY_VARIABLE=TCP_RX_HARDWARE_COMMIT_FREQUENCY
IMMEDIATE_COMMIT_PAYLOAD=16.797465 Mbps
BATCH_COMMIT_PAYLOAD=16.883698 Mbps
PAYLOAD_DELTA=+0.513%
IMMEDIATE_COMMIT_RX_PATH=0.534275 us/B
BATCH_COMMIT_RX_PATH=0.594464 us/B
RX_PATH_DELTA=+11.266%
HOLD_DELTA=+30.859%
COMMIT_CALLS_DELTA=-33.952%
PAYLOAD_REPEATABILITY_OK=False
PAYLOAD_INTEGRITY=PASS
RECONNECT_CASE_1=PASS
RECONNECT_CASE_2=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
CANDIDATE_DECISION=REJECT_FOR_PROMOTION_RX_PATH_REGRESSION
PHYSICAL_STABILITY=PENDING_USER
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p7_tcp_commit_yxz9nmq9
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p7_tcp_commit_yxz9nmq9\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P7_TCP_RX_COMMIT_COALESCING_20260929.md
```

### H4A0.4-P8

```text
ONLY_VARIABLE=SECOND_CONNECTED_PROBE_SAME_PASS
STATUS_CACHE_SCOPE=CURRENT_SCHEDULER_PASS_ONLY
DOUBLE_STATUS_PAYLOAD=16.821631 Mbps
SINGLE_STATUS_PAYLOAD=16.770394 Mbps
PAYLOAD_DELTA=-0.305%
DOUBLE_STATUS_SCHEDULER=0.569412 us/B
SINGLE_STATUS_SCHEDULER=0.543472 us/B
SCHEDULER_DELTA=-4.556%
HOLD_DELTA=-4.619%
STATUS_CALLS_DELTA=-70.521%
STATUS_TIME_DELTA=-64.431%
PAYLOAD_REPEATABILITY_OK=True
PAYLOAD_INTEGRITY=PASS
RECONNECT_CASE_1=PASS
RECONNECT_CASE_2=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
INTERPRETATION=SINGLE_STATUS_GAIN_CONFIRMED
PHYSICAL_STABILITY=PENDING_USER
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p8_socket_status_sib9cl9k
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p8_socket_status_sib9cl9k\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P8_SOCKET_STATUS_REDUNDANCY_20260929.md
```

### H4A0.4-P9

```text
ONLY_VARIABLE=TCP_RX_MAX_CHUNKS_PER_SPI_OWNERSHIP
BATCH8_TCP=13.109531 Mbps
BATCH16_TCP=14.913620 Mbps
BATCH32_TCP=14.699033 Mbps
BATCH8_PAYLOAD=16.804584 Mbps
BATCH16_PAYLOAD=16.834180 Mbps
BATCH32_PAYLOAD=16.824562 Mbps
BATCH8_HOLD_MAX=5138 us
BATCH16_HOLD_MAX=9459 us
BATCH32_HOLD_MAX=18134 us
BATCH16_VS_8_HOLD_MAX=+84.099%
BATCH32_VS_8_HOLD_MAX=+252.939%
PAYLOAD_REPEATABILITY_OK=False
PAYLOAD_INTEGRITY=PASS
RECONNECT_CASE_1=PASS
RECONNECT_CASE_2=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
AUTO_PROMOTION=NO
LARGER_BATCH_DECISION=REJECT_FOR_PROMOTION_FAIRNESS
PHYSICAL_STABILITY=PENDING_USER
```

Evidencia:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p9_rx_batch_nwemy_7g
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p9_rx_batch_nwemy_7g\SUMMARY.log
docs/v2.1.0-alpha.14/A14_H4A04P9_RX_BATCH_RAW_20260929.md
docs/v2.1.0-alpha.14/A14_ETHERNET_BENCHMARK_COMPARISON_20260929.md
```

## Candidatos

- Rechazado: commit TCP RX coalescido P7 por `+11.266%` en path RX,
  `+30.859%` en hold SPI y dispersión de payload fuera del límite.
- No implementado por seguridad: DMA P5 sobre el bus Arduino compartido.
- Validado por datos, pendiente de revisión física:
  `JWPLC_W5500_RX_FIFO_REUSE=1` por P3/P3R.
- Validado en el scheduler del profiler, pendiente de revisión física:
  reutilizar el resultado de `connected()` solo dentro de la misma pasada P8.
- Rechazados: batch RX 16/32 por aumentar hold máximo `+84.099%` y
  `+252.939%`, respectivamente.
- Aún OFF: `JWPLC_W5500_RX_FIFO_REUSE`,
  `JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES`.

## Fallos de harness/entorno

```text
STALE_PYTHON_311_PATH=YES
SANDBOX_ARDUINO_CONFIG_ACCESS_DENIED=YES
MISSING_PYSERIAL_IN_WORKSPACE_RUNTIME=YES
P7_RECONNECT_RESIDUAL_ZERO_ARM=YES_CORRECTED
PRODUCT_TRAFFIC_REACHED_BY_FAILED_ATTEMPTS=NO
```

El runner válido usó el Python del workspace y `pyserial==3.5` aislado en
`%TEMP%`; no se cambió producto para resolver estos fallos.

## Ceiling medido

El mayor payload efectivo confirmado en esta sesión es:

```text
MEASURED_REPEATABLE_PAYLOAD_CEILING=16.804 Mbps
OBSERVED_EXPERIMENTAL_PAYLOAD_CEILING=16.834180 Mbps
OBSERVED_EXPERIMENTAL_TCP_CEILING=14.913620 Mbps
DEFENSIBLE_P8_TCP=14.129575 Mbps
P3R_CONFIRMED_PAYLOAD=16.785 Mbps
P3R_TCP_MEDIAN=12.976148 Mbps
```

No se extrapola un ceiling teórico nuevo.

## Optimización restante

1. diseñar TX async aditivo sin modificar RX;
2. probar pending real, cancel, timeout, peer close y reconnect;
3. cerrar `FLUSH_PENDING_PATH` con TX realmente pendiente.

## Verificación física pendiente del usuario

Tras finalizar las pruebas autónomas, verificar visualmente:

1. TFT operativa, sin parpadeo ni diagnóstico SPI;
2. SD, FRAM, RTC, botonera y TCA/I-O operativos;
3. RS-485 y Modbus RTU operativos;
4. link Ethernet estable tras tráfico y reconexión;
5. ausencia de reset, cuelgue o degradación visible del JWPLC.

Hasta esa revisión:

```text
PHYSICAL_STABILITY=PENDING_USER
```

## Comando exacto para continuar

Para repetir el último gate cerrado mientras se prepara A2:

```powershell
$env:PYTHONPATH='C:\Users\jeykc\AppData\Local\Temp\jwplc-codex-pydeps'
& 'C:\Users\jeykc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -B `
  tools\modbus-tcp-benchmark\gates\a14_h4a04p9_rx_batch_raw.py `
  --defer-physical-review
```
