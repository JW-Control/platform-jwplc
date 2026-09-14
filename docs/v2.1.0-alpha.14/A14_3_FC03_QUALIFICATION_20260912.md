# Alpha14 — A14.3 Qualification sweep FC03

Fecha: `2026-09-12`

## Objetivo

Localizar la frontera inicial de rendimiento del Server Modbus TCP antes de ejecutar ventanas formales de 30 s y soaks largos.

Escenario:

```text
PERF-S1
PC Master -> JWPLC Modbus TCP Server
FC03 Read Holding Registers
conexión TCP persistente por caso
DURATION_PER_CASE=5 s
```

Tamaños probados:

```text
1, 16, 64, 125 registros
```

Tasas probadas:

```text
10, 20, 50, 100, 200, 500, 1000 req/s
```

Total:

```text
TOTAL_CASES=28
```

## Herramienta

Commit del qualification sweep:

```text
d472588b88e664eda0fad5cdc966dd81dc81995f
test(modbus-tcp): añadir qualification sweep FC03
```

Script:

```text
tools/modbus-tcp-benchmark/pc/a14_perf_fc03_qualification_sweep.py
```

El harness espera activamente la recuperación de `SERVER_READY` antes de iniciar cada caso y no interpreta un estado transitorio de red como fallo de rendimiento.

## Resultado completo

### FC03 — 1 registro

| req/s | achieved req/s | OK | errores | p95 us | p99 us | max us | loop max us | resultado |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 10 | 10.0 | 50/50 | 0 | 1021.3 | 1267.8 | 1438.0 | 1351 | PASS |
| 20 | 20.0 | 100/100 | 0 | 876.8 | 961.4 | 991.1 | 1880 | PASS |
| 50 | 50.0 | 250/250 | 0 | 1017.2 | 1090.6 | 1230.5 | 1386 | PASS |
| 100 | 100.0 | 500/500 | 0 | 928.5 | 1115.9 | 1784.4 | 1311 | PASS |
| 200 | 200.0 | 1000/1000 | 0 | 655.8 | 871.8 | 1924.2 | 1529 | PASS |
| 500 | 500.0 | 2500/2500 | 0 | 800.5 | 908.3 | 2105.0 | 1252 | PASS |
| 1000 | 1000.0 | 5000/5000 | 0 | 703.9 | 924.8 | 2216.9 | 1951 | PASS |

### FC03 — 16 registros

| req/s | achieved req/s | OK | errores | p95 us | p99 us | max us | loop max us | resultado |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 10 | 10.0 | 50/50 | 0 | 787.7 | 800.9 | 802.5 | 1462 | PASS |
| 20 | 20.0 | 100/100 | 0 | 829.2 | 867.3 | 910.4 | 2017 | PASS |
| 50 | 50.0 | 250/250 | 0 | 818.8 | 929.1 | 1047.5 | 1293 | PASS |
| 100 | 100.0 | 500/500 | 0 | 748.2 | 859.5 | 1033.9 | 1561 | PASS |
| 200 | 200.0 | 1000/1000 | 0 | 641.4 | 797.3 | 2558.9 | 1431 | PASS |
| 500 | 500.0 | 2500/2500 | 0 | 701.3 | 906.6 | 2127.8 | 1436 | PASS |
| 1000 | 1000.0 | 5000/5000 | 0 | 799.1 | 890.2 | 1412.5 | 1157 | PASS |

### FC03 — 64 registros

| req/s | achieved req/s | OK | errores | p95 us | p99 us | max us | loop max us | resultado |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 10 | 10.0 | 50/50 | 0 | 1156.6 | 1240.5 | 1311.5 | 4264 | PASS |
| 20 | 20.0 | 100/100 | 0 | 1035.9 | 1158.4 | 1181.9 | 1359 | PASS |
| 50 | 50.0 | 250/250 | 0 | 897.9 | 1010.4 | 1024.2 | 1467 | PASS |
| 100 | 100.0 | 500/500 | 0 | 1150.7 | 1460.3 | 2401.5 | 1940 | PASS |
| 200 | 200.0 | 1000/1000 | 0 | 1079.0 | 1346.3 | 2945.3 | 1978 | PASS |
| 500 | 499.9 | 2500/2500 | 0 | 839.6 | 969.0 | 1738.1 | 2102 | PASS |
| 1000 | 1000.0 | 5000/5000 | 0 | 918.0 | 1025.9 | 2194.3 | 1705 | PASS |

### FC03 — 125 registros

| req/s | achieved req/s | OK | errores | p95 us | p99 us | max us | loop max us | resultado |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 10 | 10.0 | 50/50 | 0 | 1519.3 | 1901.8 | 1954.3 | 1526 | PASS |
| 20 | 20.0 | 100/100 | 0 | 1021.0 | 1113.8 | 1158.3 | 1852 | PASS |
| 50 | 50.0 | 250/250 | 0 | 1118.4 | 1371.6 | 1433.7 | 1556 | PASS |
| 100 | 100.0 | 500/500 | 0 | 886.9 | 947.1 | 1685.3 | 2291 | PASS |
| 200 | 200.0 | 1000/1000 | 0 | 929.1 | 1050.6 | 1222.6 | 1406 | PASS |
| 500 | 500.0 | 2500/2500 | 0 | 1043.4 | 1221.1 | 2602.6 | 2278 | PASS |
| 1000 | 999.9 | 5000/5000 | 0 | 1130.7 | 1225.1 | 2159.7 | 2521 | PASS |

## Resumen

```text
CASES_EXECUTED=28
QUALIFICATION_PASS_POINTS=28
QUALIFICATION_FAIL_POINTS=0
Q=1   MAX_SHORT_PASS_REQ_S=1000 FIRST_SHORT_FAIL_REQ_S=NONE
Q=16  MAX_SHORT_PASS_REQ_S=1000 FIRST_SHORT_FAIL_REQ_S=NONE
Q=64  MAX_SHORT_PASS_REQ_S=1000 FIRST_SHORT_FAIL_REQ_S=NONE
Q=125 MAX_SHORT_PASS_REQ_S=1000 FIRST_SHORT_FAIL_REQ_S=NONE
A14_3_FC03_QUALIFICATION_SWEEP=PASS_EXECUTED
```

No se encontró la frontera de saturación dentro de las tasas programadas.

Por tanto:

```text
FC03_SHORT_QUALIFICATION_MAX_TESTED_REQ_S=1000
FC03_SHORT_QUALIFICATION_MAX_PASS_REQ_S=1000
FC03_SATURATION_FRONTIER=NOT_FOUND
SERVER_MAX_STABLE_REQ_S=NOT_MEASURED_YET
```

Los puntos de 5 s son únicamente qualification y no se publican como tasa estable final.

## Piso de throughput observado

Para `FC03 / 125 registros / 1000 req/s`:

```text
Request Modbus TCP = 12 bytes
Response Modbus TCP = 259 bytes
Total por transacción = 271 bytes
```

Aproximación a la tasa medida:

```text
PC -> JWPLC TCP payload       ≈ 0.096 Mbps
JWPLC -> PC TCP payload       ≈ 2.072 Mbps
TCP payload agregado          ≈ 2.168 Mbps
Datos útiles de registros     ≈ 2.000 Mbps
```

Este valor es un piso de rendimiento demostrado en ventana corta, no el máximo del sistema.

## Recuperación de SERVER_READY

Antes del sweep se observó una ventana transitoria con `SERVER_READY=NO`. El diagnóstico posterior mantuvo `SERVER_READY=YES` durante 24 s, aceptó una nueva conexión TCP en puerto 502 y terminó nuevamente en READY.

```text
SERVER_READY_RECOVERED_DURING_24S=YES
TCP_502_ACCEPTED_WHILE_DIAG=YES
FINAL_SERVER_READY=YES
A14_3_SERVER_READY_RECOVERY=PASS_RECOVERED
```

El sweep se reforzó para esperar recuperación activa antes de cada caso.

## Siguiente gate

Como 1000 req/s no alcanzó la frontera, el siguiente paso es medir saturación secuencial sin espera para FC03 de 1, 16, 64 y 125 registros, reportando explícitamente:

```text
REQ_S
TX_TCP_PAYLOAD_MBPS
RX_TCP_PAYLOAD_MBPS
TOTAL_TCP_PAYLOAD_MBPS
USEFUL_REGISTER_DATA_MBPS
```

Después se seleccionarán los puntos formales de 30 s alrededor de la frontera real.