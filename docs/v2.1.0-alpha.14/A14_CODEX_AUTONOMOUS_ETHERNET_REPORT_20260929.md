# Alpha14 — Reporte autónomo Ethernet Codex

Fecha: `2026-09-29`

Rama única de trabajo:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

## CURRENT_STATE

```text
LAST_COMPLETED_GATE=H4A0.4-P3
LAST_RESULT=PASS_DATA_ONLY
CURRENT_GATE=H4A0.4-P3R
CURRENT_ACTION=PREPARE_5_RUN_CONFIRMATION
FIFO_REUSE_DEFAULT=OFF
PHYSICAL_STABILITY=PENDING_USER
ALPHA14_CLOSED=NO
```

## Resultados

| Gate | Baseline | Candidate | Payload Mbps | TCP Mbps | Delta | SPI errors | FNV | Resets | Physical | Decisión |
|---|---|---|---:|---:|---:|---:|---|---:|---|---|
| H4A0.4-P3 | DIRECT_RX 15.970 / 12.887425 | FIFO_REUSE | 16.804 | 13.736151 | +5.226% payload | 0 | PASS | 0 observados | PENDING_USER | FIFO_REUSE_GAIN_CONFIRMED; abrir P3R |

## HEAD y commits de la sesión

HEAD inicial sincronizado:

```text
9569be3f2d4bd0f9daeb6d02f9989c15b28b498f
```

Los commits creados por esta sesión se añadirán aquí después de cada gate.

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

## Candidatos

- Rechazados en esta sesión: ninguno.
- Validados para confirmación: `JWPLC_W5500_RX_FIFO_REUSE=1` por P3.
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
TCP_MEDIAN_AT_THAT_POINT=13.736151 Mbps
```

No se extrapola un ceiling teórico nuevo.

## Optimización restante

1. confirmar `FIFO_REUSE` en P3R;
2. perfilar el chunk SPI de 64 B en P4;
3. aplicar una sola mejora P5 al bloque dominante;
4. revisar `available()/read`, commit RX y `socketStatus()`;
5. abordar TX async solo después de cerrar el frente RX.

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

Mientras P3R no esté implementado/confirmado, repetir el último gate cerrado:

```powershell
$env:PYTHONPATH='C:\Users\jeykc\AppData\Local\Temp\jwplc-codex-pydeps'
& 'C:\Users\jeykc\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -B `
  tools\modbus-tcp-benchmark\gates\a14_h4a04p3_w5500_fifo_reuse_ab.py `
  --defer-physical-review
```
