# Alpha14 — P4.2 COPY_OUT 64 B default — preparación RAW LONG RUN 600 s

Fecha: 2026-10-01

## Alcance y estado de entrada

Este documento prepara un soak RAW TCP RX continuo del package promovido.
No registra una ejecución física y no sustituye H3E-R.

~~~text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_HEAD=ebf0982ad0121db7ed9febdc2ca6870f1c77bf7f
P4_2_DEFAULT_PROMOTED=PASS
P4_2_DEFAULT_VALUE=1
P4_2_PHYSICAL_AB=PASS
P4_2_H3ER_REGRESSION=NOT_RUN
P4_2_LR600_PHYSICAL_RUN=NOT_RUN
~~~

Archivo del gate:

~~~text
tools/modbus-tcp-benchmark/gates/a14_p4_2_copy_out_64_long_600s.py
~~~

No se modificó producto ni firmware para preparar este gate. El harness
reutiliza el productor oficial:

~~~text
tools/modbus-tcp-benchmark/firmware/a14_h4a04p1_tcp_rx_profiler/
tools/modbus-tcp-benchmark/pc/a14_h4a04p1_tcp_rx_case.py
~~~

## Attempt 1 y corrección del zero-arm

El primer intento terminó antes de iniciar la ventana formal. VERIFY FNV y
los builds/uploads VERIFY y PERF fueron correctos; no existe evidencia de
fallo de producto ni de hardware.

~~~text
ATTEMPT_1=HARNESS_FAIL_PRE_WINDOW
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
FORMAL_WINDOW_STARTED=NO
ROOT_CAUSE=SPI_HOLD_COUNTERS_WRONGLY_INCLUDED_IN_ZERO_ARM
FIX=ZERO_FUNCTIONAL_COUNTERS_ONLY_PLUS_FINAL_RESET_BEFORE_PERF
~~~

El snapshot zero puede causar actividad Ethernet/SPI después del reset. Por
eso `TCP_SPI_HOLD_COUNT/TOTAL/AVG/MAX` son evidencia pre-window válida pero no
forman parte del contrato zero. Sólo se exige cero para:

~~~text
RX_BYTES
RX_OPERATIONS
TRANSPORT_ERRORS
TCP_SPI_LOCK_ERRORS
~~~

Después de validar esos cuatro campos se prepara todo el estado PC, se hace
un reset final y, tras su ACK, comienza inmediatamente la ventana. No existe
snapshot, polling, acceso Serial ni espera artificial entre ese ACK y el
inicio del cronómetro. Los contadores `TCP_SPI_HOLD_*` del snapshot final
corresponden así a la ventana formal.

## Contrato productivo

El harness audita en el source del package:

~~~text
JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1
JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=1
JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=0
JWPLC_W5500_RX_FIFO_REUSE=1
JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0
W5500_SPI_HZ=26000000
RX_COMMIT=IMMEDIATE
TCP_CHUNK=4096
PAYLOAD_PATTERN=4096B_INCREMENTING_00_FF
~~~

COPY_OUT 64 B procede exclusivamente del default productivo. El build no
añade `-DJWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1` ni sobreescribe DLEN_REUSE. El
gate también relee la condición `c_len == 64U`, las 16 copias explícitas, la
ausencia de `memcpy` sobre MMIO y la preservación del tail de 1..63 B.

Se realizan dos builds source-first del mismo firmware:

- VERIFY corto: FNV activo, hooks Ethernet apagados y profiler SPI de chunks
  apagado.
- PERF 600 s: FNV apagado y profiler SPI de chunks apagado. Los hooks
  agregados Ethernet se activan únicamente porque son necesarios para que
  `PAYLOAD_MBPS` y `US_PER_BYTE` sean matemáticamente comparables con P4.2
  SHORT. No se activa el microperfil SETUP/WIRE_WAIT/COPY_OUT/OTHER.

Antes de descubrir Arduino CLI o compilar se reporta:

~~~text
PYTHON_EXECUTABLE
PYSERIAL_IMPORT
PYSERIAL_VERSION
~~~

Un fallo de `import serial` aborta antes de cualquier build o upload.

## Productores DUT reutilizados

El snapshot oficial ya produce los contadores necesarios; no se añadió
instrumentación redundante:

~~~text
RX_BYTES
RX_OPERATIONS
TRANSPORT_ERRORS
TCP_SPI_LOCK_ERRORS
TCP_SPI_HOLD_COUNT
TCP_SPI_HOLD_TOTAL_US
TCP_SPI_HOLD_AVG_US
TCP_SPI_HOLD_MAX_US
~~~

Para las dos métricas comparables de P4.2 se leen además, una sola vez al
final:

~~~text
TCP_PROF_PAYLOAD_READ_CALLS
TCP_PROF_PAYLOAD_READ_TOTAL_US
TCP_PROF_PAYLOAD_BYTES
~~~

El firmware no produce distribución de holds SPI. Por tanto el gate no
inventa P95/P99 SPI ni añade histogramas.

## Ventana formal y política serial

La secuencia física futura es:

~~~text
preflight mínimo
→ VERIFY FNV corto
→ build/upload PERF
→ ready/arm/reset/zero-check/reset
→ una conexión TCP continua y 600 s silenciosos
→ freeze
→ un snapshot DUT final
~~~

Durante los 600 s no hay snapshots, polling DUT, resets, reconnects ni
telemetría serial periódica. Tampoco hay `Serial.print` por request, packet o
chunk. Los diez buckets se construyen íntegramente en el PC:

~~~text
P4_2_LR600_SERIAL_POLICY=QUIET
P4_2_LR600_BUCKET_POLICY=10X60S_PC_SIDE
P4_2_LR600_SPI_TELEMETRY=FINAL_SNAPSHOT_ONLY
~~~

Cada bucket informa elapsed real, bytes PC enviados, operaciones PC y Mbps
PC ofrecidos. No se deriva payload DUT por minuto porque `RX_BYTES` sólo se
observa tras el freeze. `P4_2_LR600_TOTAL_DUT_RX_BYTES` y el throughput DUT
de ventana son agregados finales exactos.

## Backpressure PC-side

Se mide aproximadamente una de cada 256 llamadas `sendall()` con
`perf_counter_ns()`. Las demás llamadas no se temporizan. El gate conserva
las muestras globales y, de forma interna, las muestras por bucket.

~~~text
P4_2_LR600_PC_SEND_METRIC=HOST_SEND_BACKPRESSURE
P4_2_LR600_PC_SEND_SAMPLES
P4_2_LR600_PC_SEND_AVG_US
P4_2_LR600_PC_SEND_P50_US
P4_2_LR600_PC_SEND_P95_US
P4_2_LR600_PC_SEND_P99_US
P4_2_LR600_PC_SEND_MAX_US
~~~

`RAW_PC_SEND_P95/P99` mide stall/backpressure del `sendall()` del host. No es
latencia request-response y no equivale a `H3ER_TCP_P95/P99`.

H3E-R posterior deberá volver a medir la latencia real y compararla con el
baseline post-P4.1 aproximado:

~~~text
H3ER_TCP_P95_US≈1270.8
H3ER_TCP_P99_US≈3666.0
H3ER_TCP_MAX_US≈22676.2
~~~

## Cálculos agregados

Payload comparable con P4.2:

~~~text
P4_2_LR600_PAYLOAD_MBPS =
  TCP_PROF_PAYLOAD_BYTES * 8 / TCP_PROF_PAYLOAD_READ_TOTAL_US

P4_2_LR600_US_PER_BYTE =
  TCP_PROF_PAYLOAD_READ_TOTAL_US / TCP_PROF_PAYLOAD_BYTES
~~~

Throughput de ventana y buckets:

~~~text
P4_2_LR600_PC_MBPS = TOTAL_PC_BYTES * 8 / measured_window_s / 1e6
P4_2_LR600_DUT_WINDOW_MBPS = TOTAL_DUT_RX_BYTES * 8 / measured_window_s / 1e6

BUCKET_PC_OFFERED_MBPS = BUCKET_PC_BYTES * 8 / bucket_elapsed_s / 1e6
BUCKET_SPREAD_PCT = (bucket_max - bucket_min) / bucket_median * 100
DRIFT_PCT = (last_minute / first_minute - 1) * 100
~~~

El nombre `P4_2_LR600_PAYLOAD_MBPS` conserva la semántica del A/B P4.2 para
permitir comparación histórica. `P4_2_LR600_DUT_WINDOW_MBPS` es la tasa
end-to-end exacta de bytes recibidos durante la ventana. Ninguna de ellas se
confunde con los buckets PC offered.

SPI y eficiencia RX:

~~~text
P4_2_LR600_SPI_HOLD_OCCUPANCY_PCT =
  TCP_SPI_HOLD_TOTAL_US / measured_window_us * 100

P4_2_LR600_RX_BYTES_PER_OPERATION = RX_BYTES / RX_OPERATIONS
P4_2_LR600_SPI_HOLDS_PER_RX_OPERATION =
  TCP_SPI_HOLD_COUNT / RX_OPERATIONS
P4_2_LR600_SPI_HOLD_US_PER_RX_BYTE =
  TCP_SPI_HOLD_TOTAL_US / RX_BYTES
~~~

Los percentiles PC usan interpolación lineal sobre las muestras ordenadas.
Se reportan también las colas entre el final de envío y la solicitud/ACK de
freeze, en microsegundos.

## Métricas emitidas

El resumen estructurado cubre:

~~~text
P4_2_LR600_DURATION_S
P4_2_LR600_VALID_BUCKETS
P4_2_LR600_TOTAL_PC_BYTES
P4_2_LR600_TOTAL_DUT_RX_BYTES
P4_2_LR600_RX_OPERATIONS

P4_2_LR600_PAYLOAD_MBPS
P4_2_LR600_PC_MBPS
P4_2_LR600_BUCKET_MIN_MBPS
P4_2_LR600_BUCKET_MEDIAN_MBPS
P4_2_LR600_BUCKET_MAX_MBPS
P4_2_LR600_BUCKET_SPREAD_PCT
P4_2_LR600_FIRST_MINUTE_MBPS
P4_2_LR600_LAST_MINUTE_MBPS
P4_2_LR600_DRIFT_PCT

P4_2_LR600_US_PER_BYTE
P4_2_LR600_RX_BYTES_PER_OPERATION

P4_2_LR600_SPI_HOLD_COUNT
P4_2_LR600_SPI_HOLD_TOTAL_US
P4_2_LR600_SPI_HOLD_AVG_US
P4_2_LR600_SPI_HOLD_MAX_US
P4_2_LR600_SPI_HOLD_OCCUPANCY_PCT
P4_2_LR600_SPI_HOLDS_PER_RX_OPERATION
P4_2_LR600_SPI_HOLD_US_PER_RX_BYTE

P4_2_LR600_PC_SEND_SAMPLES
P4_2_LR600_PC_SEND_AVG_US
P4_2_LR600_PC_SEND_P50_US
P4_2_LR600_PC_SEND_P95_US
P4_2_LR600_PC_SEND_P99_US
P4_2_LR600_PC_SEND_MAX_US

P4_2_LR600_TRANSPORT_ERRORS
P4_2_LR600_TCP_SPI_LOCK_ERRORS
P4_2_LR600_UNEXPECTED_RESETS
P4_2_LR600_FREEZE_REQUEST_TAIL_US
P4_2_LR600_FREEZE_ACK_TAIL_US
~~~

Un socket continuo que alcanza el ACK de freeze y el snapshot final congelado
es la evidencia de ausencia de reset. Una pérdida de conexión, fallo de ACK o
snapshot incompleto aborta la ejecución; no se inventa un contador DUT que el
firmware no produce.

## Referencias históricas

~~~text
P4_2_SHORT_PAYLOAD_MBPS=17.113
P4_2_SHORT_US_PER_BYTE=0.467481
P4_2_SHORT_TCP_SECONDARY_MBPS=14.203710

PRE_P4_2_PAYLOAD_MBPS=16.821
PRE_P4_2_US_PER_BYTE=0.475601
~~~

El gate calcula:

~~~text
DELTA_VS_P4_2_SHORT_PAYLOAD_PCT
DELTA_VS_PRE_P4_2_PAYLOAD_PCT
DELTA_VS_P4_2_SHORT_US_PER_BYTE_PCT
DELTA_VS_PRE_P4_2_US_PER_BYTE_PCT
~~~

Una diferencia pequeña entre sesiones no se interpreta por sí sola como
fallo de producto.

Históricamente P1 identificó `PAYLOAD_SPI_READ` como etapa dominante; P4
separó `SETUP/WIRE_WAIT/COPY_OUT/OTHER`; P4.1 redujo SETUP; P4.2 redujo
COPY_OUT. `HOLD_TOTAL/HOLD_MAX` permitió observar cuánto monopolizaba
Ethernet el bus SPI. Este soak no reactiva el profiler invasivo por etapa:
mide si los winners ya promovidos sostienen un estado sano.

## Clasificación

`PASS` exige:

- FNV corto correcto;
- 10/10 buckets completos;
- cero transport errors;
- cero SPI lock errors;
- cero resets inesperados;
- spread de buckets no mayor a 0.75 %;
- sin drift absoluto mayor a 0.75 %, reutilizando el límite de estabilidad
  como señal conservadora de cambio temporal sostenido.

`REVIEW` corresponde a integridad limpia con spread superior a 0.75 % o
drift absoluto superior a 0.75 %. Los valores de `PC_SEND_P99/MAX` y
`SPI_HOLD_MAX/OCCUPANCY` se entregan para revisión frente al histórico; no se
inventan umbrales que el plan no define. Un outlier aislado de host no produce
`FAIL`.

`FAIL` corresponde a corrupción FNV, error real de transporte, error de lock
SPI, reset/pérdida de la conexión o ejecución incompleta.

## Validación offline de preparación

La preparación se valida sin build, upload, flash ni hardware:

~~~text
AST=PASS
DURATION=600S
BUCKETS=10X60S
CONNECTION=SINGLE_CONTINUOUS
SENDALL_SAMPLING=1_OF_256
ZERO_ARM_FUNCTIONAL_COUNTERS_ONLY=PASS
ZERO_ARM_HOLD_ACTIVITY_CASE=PASS
ZERO_ARM_RX_BYTES_NONZERO_FAILS=PASS
ZERO_ARM_TRANSPORT_ERROR_FAILS=PASS
ZERO_ARM_SPI_LOCK_ERROR_FAILS=PASS
FINAL_RESET_TO_PERF_HAS_NO_DUT_SNAPSHOT=PASS
NO_PERIODIC_SERIAL_DURING_600S=PASS
FINAL_SNAPSHOT_ONLY_AFTER_FREEZE=PASS
SERIAL_DURING_WINDOW=NONE
FINAL_SEQUENCE=FREEZE_THEN_ONE_SNAPSHOT
SPI_CHUNK_PROFILE=OFF
COPY_OUT_SOURCE=PACKAGE_DEFAULT
COPY_OUT_BUILD_OVERRIDE=NO
DUT_FIELDS=REAL_PRODUCER_FIELDS
CLASSIFICATION=PASS_REVIEW_FAIL
P4_2_LR600_PHYSICAL_RUN=NOT_RUN
~~~

Estado de preparación esperado:

~~~text
P4_2_LR600_HARNESS_READY=PASS
P4_2_LR600_SERIAL_POLICY=QUIET
P4_2_LR600_BUCKET_POLICY=10X60S_PC_SIDE
P4_2_LR600_SPI_TELEMETRY=FINAL_SNAPSHOT_ONLY
P4_2_LR600_LATENCY_POLICY=SAMPLED_PC_SEND_BACKPRESSURE
P4_2_LR600_COPY_OUT_SOURCE=PACKAGE_DEFAULT
P4_2_LR600_PHYSICAL_RUN=NOT_RUN
~~~

Corrección de zero-arm validada offline:

~~~text
P4_2_LR600_ZERO_ARM_FIX=PASS
P4_2_LR600_FUNCTIONAL_ZERO_CONTRACT=PASS
P4_2_LR600_HOLD_COUNTERS_ZERO_REQUIRED=NO
P4_2_LR600_FINAL_RESET_BEFORE_PERF=PASS
P4_2_LR600_SERIAL_POLICY=QUIET
P4_2_LR600_PHYSICAL_RUN=NOT_RUN
~~~
