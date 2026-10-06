# Alpha14 - R4-R6 matriz final TCP x RTU EXP-MIX - RESULT 2026-10-02

## Estado

`PASS_CHARACTERIZED`

Configuración congelada:

- C0 POLLING;
- W5500 SPI 26 MHz;
- RTU 500000 baud;
- Master FIFO 9 / Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP / Slave STRUCTURAL;
- frame gap 100 us;
- timeout benchmark 25 ms;
- partial hold STRUCTURAL 15000 us;
- CRC BITWISE;
- workload 2DI+2DO+2AI+2AO unpaced;
- full-runtime Display + SD + FRAM + RTC + I/O + buttons.

## Matriz

| TCP target | TCP real | RTU req/s | scans/s | ms/scan | RTU retention vs OFF | Estado |
|---:|---:|---:|---:|---:|---:|---|
| 0 | 0.000 | 1282.827 | 160.353 | 6.236 | 100.00% | PASS |
| 100 | 100.000 | 1149.248 | 143.656 | 6.961 | 89.59% | PASS |
| 250 | 250.000 | 1056.961 | 132.120 | 7.569 | 82.39% | PASS |
| 500 | 500.002 | 900.039 | 112.505 | 8.889 | 70.16% | PASS |
| 750 | 750.000 | 733.416 | 91.677 | 10.908 | 57.17% | PASS |
| 1000 | 974.345 | 706.593 | 88.324 | 11.322 | 55.08% | SATURATION_FAIL_CLEAN |

Todos los casos tuvieron RTU failed/timeouts/rejected/verify/CRC = 0,
Slave discarded tails/bytes = 0, DI/DO/AI/AO failures = 0,
peripheral failures = 0, SPI probe failures = 0 y no reset.

El caso TCP1000 no alcanza el target: 974.345 req/s = 97.435% del objetivo,
pero permanece limpio, por lo que se clasifica `SATURATION_FAIL_CLEAN`.

## Latencia TCP

| TCP target | AVG us | P95 us | P99 us | MAX us |
|---:|---:|---:|---:|---:|
| 100 | 996.656 | 1433.600 | 3454.730 | 23049.2 |
| 250 | 953.360 | 1428.020 | 3734.703 | 41706.6 |
| 500 | 928.936 | 1336.600 | 3874.300 | 58322.0 |
| 750 | 936.186 | 1369.500 | 4110.600 | 19610.5 |
| 1000 | 1023.041 | 1447.300 | 3775.461 | 21518.9 |

## Comparación con R3 lightweight EXP-MIX

R3 2-2-2-2 sin full-runtime: 1550.396 RTU req/s, 193.800 scans/s, 5.160 ms/scan.

R4-R6 TCP OFF full-runtime: 1282.827 RTU req/s, 160.353 scans/s, 6.236 ms/scan.

El coste del full-runtime respecto a R3 es aproximadamente -17.26% en
throughput/scan rate y +20.86% en periodo de scan.

## Comparación con frontier F6 anterior

| TCP target | F6 RTU req/s | R4-R6 RTU req/s | Delta R4-R6 |
|---:|---:|---:|---:|
| 100 | 1176.477 | 1149.248 | -2.31% |
| 250 | 1070.373 | 1056.961 | -1.25% |
| 500 | 891.515 | 900.039 | +0.96% |
| 750 | 721.697 | 733.416 | +1.62% |
| 1000 | 670.773 | 706.593 | +5.34% |

Esta comparación es orientativa: F6 y R4-R6 no usan exactamente el mismo
workload RTU. R4-R6 es la referencia final porque usa EXP-MIX realista y
full-runtime.

## Conclusión

R4-R6 caracteriza una frontera limpia de coexistencia TCP x RTU con todos
los periféricos activos. La curva comercial debe basarse en esta matriz y
usar margen conservador; no debe usar el target 1000 como 1000 garantizado,
pues el techo observado fue ~974 req/s TCP en coexistencia con 706.6 RTU req/s.