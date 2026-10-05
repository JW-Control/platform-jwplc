# Alpha14 — preparación H3E-R post-P4.2 long-run 600 s — 2026-10-01

## Propósito y alcance

```text
PURPOSE=LONG_TERM_FULL_RUNTIME_STABILITY
BASELINE_SHORT=H3ER_POST_P4_2_120S
DURATION=600
BUCKETS=10X60S
PRODUCT_MUTATION=NO
H3ER_120S_GATE_MODIFIED=NO
PHYSICAL_RUN=NOT_RUN
```

Este gate no busca un ceiling nuevo. Repite la composición funcional del
H3E-R post-P4.2 ya cerrado en PASS y cambia únicamente la duración metodológica
de 120 a 600 segundos para observar estabilidad temporal, tail latency, RTU,
DataLog, periféricos, resets y TFT física.

El regression guard corto permanece en:

```text
tools/modbus-tcp-benchmark/gates/a14_h3er_current_package_full_runtime.ps1
```

El long-run nuevo está aislado en:

```text
tools/modbus-tcp-benchmark/gates/a14_h3er_post_p4_2_long_600s.ps1
```

## Reutilización de la cadena existente

El wrapper nuevo llama sin modificaciones a
`a14_p5b_physical_master_slave_combined.ps1`, que ya propaga `DurationS` al
runner full-runtime. El productor PC-side usa buckets de 60 s cuando la
duración es al menos 120 s y calcula su cantidad como
`ceil(duration / 60)`. Para 600 s produce exactamente diez buckets.

Se reutilizan sin duplicación:

- firmware Master `a14_p5_full_runtime_master`;
- firmware Slave `a14_p5_rtu_slave`;
- resolver físico y runner P5B;
- qualification full-runtime FC03/125;
- productor `FORMAL_RESULT` / `LONG_BUCKET`.

## Composición obligatoria

| Frente | Contrato LR600 |
|---|---|
| TCP | FC03, 125 holding registers, 1000 req/s, 600 s |
| RTU | Master/Slave, 115200 8N1, Slave ID 2, ASYNC, queued TX, 50 Hz, periodo 20 ms, timeout 25 ms |
| Ethernet/SPI | W5500 26 MHz; FIFO_REUSE, DLEN_REUSE y COPY_OUT_64 por default; DIRECT_RX OFF; RX commit inmediato |
| Display | `HMI_ON_DEMAND_DIRTY`, `USER_REFRESH_ON_DEMAND`, source/private TFT backend |
| DataLog | `JWPLCDataLog`, `BUFFERED_DATALOG`, autoservice core/system, sin servicio manual |
| Periféricos | FRAM, RTC, TCA/I-O, botonera, SPI probe y TFT Master/Slave |
| Modbus RTU | source compiled/linked en Master y Slave; archive linked = NO |

No se permiten overrides de build para:

```text
JWPLC_W5500_RX_FIFO_REUSE
JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES
JWPLC_SPI_FIFO_REUSE_DLEN_CACHE
JWPLC_SPI_FIFO_REUSE_COPY_OUT_64
```

En particular:

```text
COPY_OUT_64_SOURCE=PACKAGE_DEFAULT
COPY_OUT_64_DEFAULT=1
COPY_OUT_64_BUILD_OVERRIDE=NO
MODBUS_RTU_POLICY=SOURCE_FIRST
```

## TCP async: alcance explícito

```text
connectAsync=PRESENT_BUT_NOT_EXERCISED_INBOUND_SERVER
writeAsync=PRESENT_BUT_NOT_EXERCISED_NOT_INTEGRATED_IN_MODBUS_TCP
flushAsync=PRESENT_BUT_NOT_EXERCISED_BY_MODBUS_TCP
stop=PRESENT_LEGACY_WRAPPER_USES_ASYNC_ENGINE
MODBUS_TCP_TX=LEGACY_BLOCKING_WRITE
```

El gate no declara validación TCP async end-to-end.

## Serial y observer effect

Los diez buckets se calculan PC-side a partir de las transacciones TCP ya
observadas. La revisión estática exige que el loop formal no contenga
`collect_snapshot()` ni comandos Serial de snapshot. El snapshot completo del
DUT ocurre después de terminar la ventana y después de la quiescencia RTU.

```text
SERIAL_POLICY=COMPACT_QUIET
NO_PERIODIC_SERIAL_FORMAL_WINDOW=PASS
FINAL_SNAPSHOT_POST_WINDOW=PASS
```

La salida física futura queda limitada a preflight, estados compactos de
compile/upload, diez líneas de bucket, resumen y prompts TFT finales. Los
detalles completos permanecen en archivos.

## Buckets y estadística temporal

Los límites exigidos son:

```text
0-60
60-120
120-180
180-240
240-300
300-360
360-420
420-480
480-540
540-600
```

Cada línea de bucket contiene req/s, OK, AVG, P95, P99, MAX y los guards P95 y
P99. El resumen calcula mínimo/mediana/máximo, spread de req/s, primer y último
minuto, drift y medianas de los tres primeros frente a los tres últimos
buckets.

`REQ_S_SPREAD_PCT` se define como `(max - min) / median * 100`.

Para señalar una tendencia sostenida clara sin inventar un umbral porcentual,
el wrapper usa un criterio conservador de separación: sólo marca
`SUSTAINED_TAIL_WORSENING=YES` cuando el mínimo de los últimos tres buckets es
mayor que el máximo de los primeros tres, simultáneamente para P95 y P99. Esa
señal produce REVIEW, no PRODUCT_FAIL.

## Guards preservados

```text
P95_GUARD_US=1397.9
P99_GUARD_US=4215.9
MAX_POLICY=DIAGNOSTIC_ONLY
LOOP_MAX_POLICY=DIAGNOSTIC_ONLY
```

Los guards se evalúan en el agregado y en cada uno de los diez buckets. Un
cruce permanece visible como `TEMPORAL_DEGRADATION=REVIEW`; no se oculta en el
promedio.

## Referencia corta post-P4.2

```text
REQ_S=1000.0
AVG_US=880.3
P95_US=1253.2
P99_US=2273.1
MAX_US=8831.5
LOOP_AVG_US=633
LOOP_MAX_US=12012
```

El LR600 calcula deltas contra esa ejecución para AVG, P95, P99, MAX, loop max
y req/s. Son evidencia comparativa cross-session; diferencias pequeñas no se
convierten en fallo de producto.

## Criterios funcionales

PASS exige:

- TCP achieved >= 99.9 %, todas las requests enviadas respondidas y cero
  timeouts/transport/protocol/bus-lock errors;
- RTU 49.5..50.5 Hz, cero rejected/failed/verify/CRC/timeouts/skipped, conteos
  Master consistentes y cross-count Slave exacto;
- DataLog activo, committed bytes > 0, cero failed commits, append fails y
  verify fails;
- periféricos sin fallos, cero unexpected resets y TFT Master/Slave aprobado.

`SD_DATALOG_PENDING_BYTES` se reporta pero no debe ser cero obligatoriamente.
El Slave no expone contador directo de resets; se conserva el guard de
cross-count más snapshot final quiesced.

La clasificación queda en:

```text
H3ER_LR600_FUNCTIONAL=PASS|FAIL
H3ER_LR600_TEMPORAL_DEGRADATION=NOT_PRESENT|REVIEW
A14_H3ER_LR600=PASS|REVIEW|FAIL
```

Sólo un fallo funcional real produce FAIL. MAX TCP y loop max aislados siguen
siendo diagnósticos.

## Resultados y logs

La ejecución futura crea:

```text
%TEMP%\jwplc_a14_h3er_lr600_<timestamp>\
```

Y copia como mínimo:

```text
qualification.log
master_final.txt
slave_final.txt
tcp_result.csv
compile_master.log
compile_slave.log
SUMMARY.log
```

`SUMMARY.log` contiene las diez líneas completas de bucket y el resumen
consolidado.

## Validación offline de preparación

```text
POWERSHELL_SYNTAX=PASS
H3ER_120S_GATE_UNCHANGED=PASS
DURATION=600
BUCKET_COUNT=10
BUCKET_DURATION=60
SAME_FULL_RUNTIME_WORKLOAD=PASS
SAME_TCP_RATE=PASS
SAME_FC03_QUANTITY=PASS
SAME_RTU_CONFIGURATION=PASS
SAME_DATALOG_POLICY=PASS
SAME_DISPLAY_POLICY=PASS
SAME_PACKAGE_DEFAULTS=PASS
NO_PERIODIC_SERIAL_FORMAL_WINDOW=PASS
FINAL_SNAPSHOT_POST_WINDOW=PASS
P95_GUARD_PRESERVED=1397.9
P99_GUARD_PRESERVED=4215.9
COPY_OUT_64_SOURCE=PACKAGE_DEFAULT
COPY_OUT_64_DEFAULT=1
COPY_OUT_64_BUILD_OVERRIDE=NO
MODBUS_RTU_POLICY=SOURCE_FIRST
PRODUCT_FILES_MODIFIED=NO
```

## Estado

```text
H3ER_LR600_PREP=PASS
H3ER_LR600_COMPOSITION_AUDIT=PASS
H3ER_LR600_DURATION_S=600
H3ER_LR600_BUCKET_POLICY=10X60S
H3ER_LR600_SERIAL_POLICY=COMPACT_QUIET
H3ER_LR600_BASELINE=H3ER_POST_P4_2_120S
H3ER_LR600_PRODUCT_MUTATION=NO
H3ER_LR600_PHYSICAL_RUN=NOT_RUN
```
