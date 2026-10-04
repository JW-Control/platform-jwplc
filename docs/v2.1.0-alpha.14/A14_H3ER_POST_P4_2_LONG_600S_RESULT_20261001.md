# Alpha14 — resultado H3E-R post-P4.2 LR600 — 2026-10-01

## Cierre

```text
PHYSICAL_RUN=COMPLETE
HARDWARE_RERUN_REQUIRED=NO
ORIGINAL_CLASSIFICATION=FAIL_FALSE_POSITIVE_RTU
CORRECTED_CLASSIFICATION=PASS
H3ER_LR600_PHYSICAL_RESULT=PASS
```

La corrida física ya ejecutada se conserva sin alteraciones en:

```text
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h3er_lr600_20261001_113730
```

No se repitió hardware. La corrección afecta exclusivamente al clasificador
PowerShell y a la documentación.

## Evidencia física

### TCP

```text
REQUESTS=600000/600000
ACHIEVED_REQ_S=1000.00
ACHIEVED_PCT=100.000
P95_US=1256.7
P99_US=2486.1
MAX_US=12221.8
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

Los diez buckets preservaron los guards individuales. El rango observado fue:

```text
REQ_S=999.95..1000.05
P95_US=1249.1..1266.0
P99_US=2171.7..3428.8
P95_GUARD_US=1397.9
P99_GUARD_US=4215.9
```

### RTU

```text
ACHIEVED_HZ=50.000
STARTED=30002
COMPLETED=30002
SUCCESS=30002
FAILED=0
VERIFY_FAILS=0
PERIODS_SKIPPED=0
CRC_ERRORS=0
TIMEOUTS=0
SLAVE_RX=30002
SLAVE_TX=30002
SLAVE_OK=30002
RTU_MASTER_PASS=YES
RTU_SLAVE_PASS=YES
RTU_CROSS_COUNT_PASS=YES
```

La configuración solicitada y la telemetría de baud fueron:

```text
MASTER_RTU_BAUD_REQUESTED=115200
MASTER_RTU_BAUD_EFFECTIVE=115201
SLAVE_RTU_BAUD_REQUESTED=115200
SLAVE_RTU_BAUD_EFFECTIVE=115201
RTU_CLOCK_PROFILE=APB_FORCED
```

### DataLog, periféricos y TFT

```text
DATALOG_ACTIVE=YES
DATALOG_ACCEPTED_BYTES=19168
DATALOG_COMMITTED_BYTES=19136
DATALOG_PENDING_BYTES=32
DATALOG_FAILED_COMMITS=0
DATALOG_APPEND_FAILS=0
DATALOG_VERIFY_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
MASTER_UNEXPECTED_RESETS=0
MASTER_TFT_PHYSICAL=PASS
SLAVE_TFT_PHYSICAL=PASS
```

El remanente de 32 bytes es válido para el snapshot instantáneo y no forma
parte del criterio duro de fallo.

### Tail y estabilidad temporal

```text
H3ER_LR600_TAIL_GUARD=PASS
H3ER_LR600_SUSTAINED_TAIL_WORSENING=NO
H3ER_LR600_TEMPORAL_DEGRADATION=NOT_PRESENT
```

El incremento P99 entre la mediana de los tres primeros y los tres últimos
buckets fue `+11.964 %`, pero no existió la separación simultánea sostenida de
P95 y P99 definida por el gate. Todos los buckets permanecieron por debajo de
ambos guards.

## Falso positivo del clasificador

```text
HARNESS_BUG=EXACT_EFFECTIVE_BAUD_COMPARISON_AGAINST_REQUESTED_BAUD
ROOT_CAUSE=CONSUMER_REQUIRED_RTU_BAUD_EFFECTIVE_115200_WHILE_APB_PROFILE_REPORTED_115201
G0_FALSE_RTU_CLASSIFICATION_REPRODUCED=PASS
```

El consumidor LR600 añadía `RTU` a `FUNCTIONAL_FAILURES` cuando
`RTU_BAUD_EFFECTIVE` no era exactamente `115200`. Master y Slave reportaron
`115201`, valor efectivo normal del divisor APB para la solicitud nominal
`115200`. Todos los demás términos de la condición RTU antigua fueron
verdaderos.

El productor P5-B no usa esa igualdad errónea. Su contrato canónico validó:

```text
RTU_MASTER_PASS=YES
RTU_SLAVE_PASS=YES
RTU_CROSS_COUNT_PASS=YES
```

Por tanto existía una divergencia productor/consumidor: la corrida física era
válida, pero una comprobación adicional del wrapper confundía baud solicitado
con baud efectivo.

## Corrección

El clasificador LR600 ahora:

- consume los marcadores canónicos `RTU_MASTER_PASS`, `RTU_SLAVE_PASS` y
  `RTU_CROSS_COUNT_PASS`;
- exige `RTU_BAUD=115200` solicitado en Master y Slave;
- conserva `RTU_BAUD_EFFECTIVE` como telemetría diagnóstica;
- mantiene el rango `49.5..50.5 Hz`, los conteos exactos, cero rejected,
  failed, verify fails, skipped periods, CRC y timeouts;
- mantiene periodo `20000 us`, timeout `25 ms`, motor `ASYNC` y TX `QUEUED`;
- mantiene la igualdad exacta Slave RX/TX/OK contra Master success.

No se debilitó ningún control funcional.

## Tests offline

```text
CASE_A_REAL_CLEAN=PASS
CASE_B_TIMEOUT_FAILS=PASS
CASE_C_CRC_FAILS=PASS
CASE_D_PERIOD_SKIPPED_FAILS=PASS
CASE_E_CROSS_COUNT_MISMATCH_FAILS=PASS
CASE_F_HZ_OUT_OF_RANGE_FAILS=PASS
G0_REAL_RUN_RECLASSIFICATION=PASS
G0_NEGATIVE_RTU_CASES=PASS
G0_CLASSIFIER_NOT_WEAKENED=PASS
```

## Reclasificación de la evidencia existente

```text
H3ER_LR600_TCP_FUNCTIONAL=PASS
H3ER_LR600_RTU_FUNCTIONAL=PASS
H3ER_LR600_DATALOG_FUNCTIONAL=PASS
H3ER_LR600_PERIPHERALS_FUNCTIONAL=PASS
H3ER_LR600_TAIL_GUARD=PASS
H3ER_LR600_TEMPORAL_DEGRADATION=NOT_PRESENT
H3ER_LR600_FUNCTIONAL_FAILURE_COUNT=0
H3ER_LR600_FUNCTIONAL_FAILURES=NONE
H3ER_LR600_FUNCTIONAL=PASS
A14_H3ER_LR600=PASS
```

El `FAIL` original permanece documentado como falso positivo histórico; no se
reescribió ni eliminó el `SUMMARY.log` de la corrida física.

## Scope

```text
PRODUCT_FILES_MODIFIED=NO
H3ER_120S_GATE_MODIFIED=NO
HARDWARE_RERUN_REQUIRED=NO
NEXT_GATE=G1_TCP_SPI_WASTE_BASELINE
```
