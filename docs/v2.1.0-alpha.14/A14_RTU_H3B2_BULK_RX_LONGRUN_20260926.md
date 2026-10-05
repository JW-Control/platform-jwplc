# Alpha14 — RTU-H3B.2 — Bulk RX + TCP500 long-run

Fecha: 2026-09-26

## Contexto

H3B.1 confirmó repetibilidad corta de Bulk RX:

```txt
BULK_RUNS=5
BULK_CLEAN_RUNS=5
BULK_RTU_HZ_MIN=678.260
BULK_RTU_HZ_AVG=685.590
BULK_RTU_HZ_MAX=692.591
BULK_FAILED_TOTAL=0
BULK_TIMEOUTS_TOTAL=0
REQUEST_PATH_GAP_TOTAL=0
RESPONSE_PATH_GAP_TOTAL=0
```

El timeout aislado de H3B no se reprodujo en 205771 transacciones Bulk.

H3B.2 mantiene el mismo perfil durante 600 s continuos para comprobar estabilidad sostenida antes de decidir la adopción de Bulk RX.

## Perfil congelado

```txt
BAUD=500000
CONFIG=8N1
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
MOTOR=ASYNC
TX_MODE=QUEUED
RTU_MODE=UNPACED
RTU_TIMEOUT_MS=25
TCP=500 req/s
TCP_FC03_QUANTITY=125
RTU_FC03_QUANTITY=2
W5500_SPI_HZ=26000000
FULL_RUNTIME=ACTIVE
DURATION=600 s
BUCKET=60 s
```

No se cambia timeout, baud, frame gap, FIFO, W5500 ni scheduling para hacer pasar el gate.

## Control temporal

El tráfico TCP se divide en diez buckets de 60 s. Cada bucket registra:

- req/s;
- latencia media;
- P95;
- P99.

También se comparan primera y segunda mitad para observar drift de throughput y latencia.

RTU conserva la telemetría de H3B.1:

```txt
MASTER_TX
MASTER_RX
SLAVE_RX
SLAVE_TX
SLAVE_OK
REQUEST_PATH_GAP
RESPONSE_PATH_GAP
RTU_TRANSACTION_MAX_US
RTU_TRANSACTIONS_OVER_5MS
RTU_TRANSACTIONS_OVER_10MS
RTU_TRANSACTIONS_OVER_20MS
RTU_LAST_FAILURE_DURATION_US
RTU_MAX_FAILURE_DURATION_US
RTU_LAST_FAILURE_RESULT
RTU_SERVICE_GAP_MAX_US
LOOP_GAP_MAX_US
```

## Piso de rendimiento

H3B.1 obtuvo un mínimo limpio de 678.260 tx/s. Para detectar una degradación sostenida durante el long-run se fija:

```txt
RTU_LONGRUN_FLOOR_HZ=650
```

Es aproximadamente 5 % por debajo del mínimo observado en H3B.1. No es una meta de optimización; es un guardrail de estabilidad.

## Criterio de cierre

```txt
PROFILE_PASS=YES
RTU_HZ >= 650
RTU_FAILED=0
RTU_TIMEOUTS=0
MASTER_CRC=0
SLAVE_CRC=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
TCP_TARGET_PCT >= 99
cada bucket TCP >= 495 req/s
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
TFT Master/Slave estable
```

Si todo se cumple:

```txt
A14_RTU_H3B2=PASS_BULK_LONGRUN_600S
```

## Siguiente decisión

Si H3B.2 pasa, Bulk RX queda con evidencia suficiente para plantear su adopción como ruta interna del motor ASYNC en el perfil rápido controlado. Después se retoma optimización de throughput hacia la meta exploratoria:

```txt
TCP500 + RTU800
8 módulos x 100 actualizaciones/s
10 ms por actualización/módulo
```

El siguiente cuello se elegirá a partir de la telemetría medida; no se combinarán cambios antes de cerrar este long-run.


## Resultado físico H3B.2

El perfil permaneció correcto durante 600 s:

```txt
BAUD=500000
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
CLOCK=APB_FORCED
TCP=500
```

TCP se mantuvo estable:

```txt
TCP_REQ_S=499.994
TCP_TARGET_PCT=99.999
TCP_BUCKET_MIN_REQ_S=499.967
TCP_BUCKET_MAX_REQ_S=500.033
TCP_HALF_RATE_DRIFT_PCT=-0.001
TCP_AVG_US=1115.4
TCP_P95_US=1544.3
TCP_P99_US=9090.1
```

RTU mantuvo el rendimiento esperado pero registró dos timeouts:

```txt
RTU_HZ=679.481
RTU_STARTED=407868
RTU_COMPLETED=407868
RTU_SUCCESS=407866
RTU_FAILED=2
RTU_TIMEOUTS=2
MASTER_TX=407868
MASTER_RX=407866
SLAVE_RX=407866
SLAVE_TX=407866
SLAVE_OK=407866
REQUEST_PATH_GAP=2
RESPONSE_PATH_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
```

Los dos fallos terminaron como timeout (error 5) cerca del límite configurado:

```txt
LAST_FAILURE_DURATION_US=24603
MAX_FAILURE_DURATION_US=24658
RTU_TIMEOUT_MS=25
```

El runtime restante quedó limpio:

```txt
RTU_FLOOR_PASS=YES
TCP_CLEAN=YES
TCP_TARGET_PASS=YES
BUCKET_TARGET_PASS=YES
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
```

### Decisión

H3B.2 no pasa estabilidad porque 2 de 407868 transacciones terminaron en timeout.
La tasa observada es aproximadamente 4.9 ppm, demasiado baja para aparecer de
forma confiable en pruebas cortas pero real en long-run.

La evidencia apunta al lado request:

```txt
MASTER_TX - SLAVE_RX = 2
SLAVE_TX - MASTER_RX = 0
```

Esto no prueba todavía pérdida física. En el Slave, `rxFrames` sólo incrementa
cuando el parser entrega una trama local a `processServerFrame()`. Una request
puede haber llegado parcialmente o completa al UART y ser descartada como tail
ambiguo sin incrementar `rxFrames` ni `crcErrors`.

Siguiente gate:

```txt
H3B.3 = RX-path accounting
```

Se instrumentarán bytes RX/TX observados por el motor RTU y tails/bytes
descartados por `pollServer()`, sin cambiar timing ni comportamiento.
