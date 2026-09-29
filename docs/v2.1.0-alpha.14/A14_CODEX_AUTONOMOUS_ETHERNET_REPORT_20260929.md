# Alpha14 — Reporte autónomo Ethernet Codex

Fecha: `2026-09-29`

Rama única de trabajo:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

## CURRENT_STATE

```text
LAST_COMPLETED_GATE=H4A0.4-P5
LAST_RESULT=BLOCKED_SAFELY_NO_PRODUCT_CANDIDATE
CURRENT_GATE=H4A0.4-P6
CURRENT_ACTION=DESIGN_ADDITIVE_AVAILABLE_READ_FUSION
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

## HEAD y commits de la sesión

HEAD inicial sincronizado:

```text
9569be3f2d4bd0f9daeb6d02f9989c15b28b498f
```

Commits creados hasta P3R:

```text
c9666d0d docs(alpha14): registrar resultado P3 FIFO reuse
bd38f7c5 test(alpha14): añadir confirmación P3R FIFO reuse
47958b90 docs(alpha14): confirmar P3R FIFO reuse
115e8b07 test(alpha14): perfilar overhead por chunk SPI W5500
1b6abff1 docs(alpha14): registrar microperfil P4 W5500
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

## Candidatos

- Rechazados en esta sesión: ninguno.
- No implementado por seguridad: DMA P5 sobre el bus Arduino compartido.
- Validado por datos, pendiente de revisión física:
  `JWPLC_W5500_RX_FIFO_REUSE=1` por P3/P3R.
- Aún OFF: `JWPLC_W5500_RX_FIFO_REUSE`,
  `JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES`.

## Fallos de harness/entorno

```text
STALE_PYTHON_311_PATH=YES
SANDBOX_ARDUINO_CONFIG_ACCESS_DENIED=YES
MISSING_PYSERIAL_IN_WORKSPACE_RUNTIME=YES
PRODUCT_TRAFFIC_REACHED_BY_FAILED_ATTEMPTS=NO
```

El runner válido usó el Python del workspace y `pyserial==3.5` aislado en
`%TEMP%`; no se cambió producto para resolver estos fallos.

## Ceiling medido

El mayor payload efectivo confirmado en esta sesión es:

```text
MEASURED_PAYLOAD_CEILING=16.804 Mbps
P3R_CONFIRMED_PAYLOAD=16.785 Mbps
P3R_TCP_MEDIAN=12.976148 Mbps
```

No se extrapola un ceiling teórico nuevo.

## Optimización restante

1. diseñar y medir fusión aditiva `available()/read`;
2. revisar commit RX diferido/coalescido;
3. revisar lecturas redundantes de `socketStatus()`;
4. abordar TX async solo después de cerrar el frente RX.

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

El último gate físico repetible continúa siendo P4 mientras se prepara P6:

```powershell
$env:PYTHONPATH='C:\Users\jeykc\AppData\Local\Temp\jwplc-codex-pydeps'
& 'C:\Users\jeykc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -B `
  tools\modbus-tcp-benchmark\gates\a14_h4a04p4_w5500_chunk_profile.py `
  --defer-physical-review
```
