# Alpha14 — H4A0.4-P7 — commit TCP RX inmediato frente a coalescido

Fecha: `2026-09-29`

## Resultado

```text
A14_H4A04P7_TCP_RX_COMMIT_AB=PASS_DATA_ONLY
H4A04P7_INTERPRETATION=PAYLOAD_REPEATABILITY_INSUFFICIENT
CANDIDATE_DECISION=REJECT_FOR_PROMOTION_RX_PATH_REGRESSION
PHYSICAL_STABILITY=PENDING_USER
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
```

El A/B mantuvo lectura directa y `FIFO_REUSE` en ambas variantes. La única
variable fue cuándo se escribe `Sn_RX_RD` y se emite `Sock_RECV`:

```text
BASELINE=IMMEDIATE_COMMIT
CANDIDATE=BATCH_COMMIT
ONLY_VARIABLE=TCP_RX_HARDWARE_COMMIT_FREQUENCY
READ_PATTERN=READ_DIRECT_BOTH_VARIANTS
FIFO_REUSE=ON_BOTH_VARIANTS
SPI_HZ=26000000
RUNS_PER_VARIANT=3
```

La extensión TCP es aditiva. El camino Arduino legado conserva su conducta y
el profiler usa el modo diferido únicamente cuando
`JWPLC_H4A04P7_DEFER_TCP_COMMIT=1`; el default es `0`.

## Integridad y recuperación

```text
VERIFY_RX_BYTES=1315292
FNV_ACTUAL=3990079337
FNV_EXPECTED=3990079337
PAYLOAD_INTEGRITY=PASS
RECONNECT_CASE_1=PASS
RECONNECT_CASE_2=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
```

Las dos reconexiones se ejecutaron sin reflashear entre intentos. El primer
socket liberó la barrera de medición, drenó bytes pendientes y volvió a
`MODE=IDLE` antes del segundo armado.

## A/B

| Métrica mediana | `IMMEDIATE_COMMIT` | `BATCH_COMMIT` | Delta candidata |
|---|---:|---:|---:|
| payload efectivo | 16.797465 Mbps | 16.883698 Mbps | +0.513% |
| TCP end-to-end | 13.866881 Mbps | 9.727778 Mbps | −29.849% |
| path RX | 0.534275 us/B | 0.594464 us/B | +11.266% |
| hold SPI | 0.557463 us/B | 0.729491 us/B | +30.859% |
| commits por MB | 988.550685 | 652.916871 | −33.952% |
| tiempo commit | 0.019154 us/B | 0.013948 us/B | −27.183% |
| payload spread | 0.194522% | 0.546878% | candidata FAIL >0.5% |

Coalescer redujo el costo directo de commit, pero el lote no liberó RX hasta
agotar los datos visibles del W5500. Eso introdujo sondeos adicionales de
`Sn_RX_RSR`, aumentó el tiempo total del camino RX y prolongó el ownership SPI.
La pequeña subida de payload queda fuera de aceptación por dispersión y no
compensa las regresiones internas.

## Fallo de harness corregido

El primer intento completó integridad y las seis corridas, pero el segundo
caso de reconexión encontró bytes residuales durante el armado en cero:

```text
H4A04P1_ZERO_ARM_FAILED={RX_BYTES:30964,RX_OPERATIONS:40,
TRANSPORT_ERRORS:0,TCP_SPI_LOCK_ERRORS:0}
CLASSIFICATION=HARNESS_FAILURE
PRODUCT_TRAFFIC_CORRUPTION=NO_EVIDENCE
```

La barrera `F` había dejado congelado el socket anterior. El harness se
corrigió para cerrar el socket, liberar la barrera y esperar `MODE=IDLE` antes
de abrir la segunda conexión. Se validó primero con dos casos cortos y luego
se repitió P7 completo desde un árbol limpio.

## Evidencia

```text
HEAD=3a0433475b52c0a0a4d91cc918005c6a7487a1fd
RESULT_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p7_tcp_commit_yxz9nmq9
SUMMARY_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p7_tcp_commit_yxz9nmq9\SUMMARY.log
```

## Decisión

No promover el commit TCP RX coalescido. La API experimental queda sin uso en
el camino legado y el flag del profiler permanece OFF por defecto. El
siguiente gate revisará lecturas redundantes de `socketStatus()/connected()`.

```text
FIFO_REUSE_DEFAULT=OFF
P7_DEFER_TCP_COMMIT_DEFAULT=OFF
PHYSICAL_STABILITY=PENDING_USER
NEXT=H4A0.4-P8_SOCKET_STATUS_REDUNDANCY
```
