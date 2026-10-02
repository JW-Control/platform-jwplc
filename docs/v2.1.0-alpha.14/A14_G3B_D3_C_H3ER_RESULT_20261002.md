# Alpha14 — G3B-D3-C H3E-R resultado — 2026-10-02

## Clasificación

```text
HEAD=147a386d97fa7aefd5868e3eb5ac7e40db18c6ef
D3C_BUILD_SELECTION=PASS
D3C_PACKAGE_COMPOSITION=PASS
D3C_TCP=SATURATION_FAIL_CLEAN
D3C_RTU=PASS
D3C_PERIPHERALS=PASS
D3C_DATALOG=PASS
D3C_CROSS_COUNT=PASS
D3C_FUNCTIONAL_FAILURE=NO_EVIDENCE
D3C_PERFORMANCE_RESULT=FAIL
DEFAULT_PROMOTION=BLOCKED
NEXT=G3B_D3_C1_FULL_RUNTIME_PROFILE
```

## TCP

```text
TARGET=120000 requests / 120 s / 1000 req/s
OK=112989
ACHIEVED=941.574 req/s
ACHIEVED_PCT=94.157 %
AVG=1050.5 us
P95=1754.7 us
P99=4746.5 us
MAX=40902.4 us
LOOP_AVG=373 us
LOOP_MAX=8772 us
TCP_ERRORS=0
CROSS_COUNT=PASS
```

El calificador lo clasificó como `SATURATION_FAIL_CLEAN`: no existen timeout, errores de transporte/protocolo ni bus-lock. La limitación observada es headroom/rendimiento.

## Evolución temporal

```text
0..60 s: REQ_S=894.23 AVG=1100.5 P95=1898.3 P99=6068.9 MAX=40902.4
60..120 s: REQ_S=988.92 AVG=1005.2 P95=1680.7 P99=4105.1 MAX=17287.8
```

La mejora material del segundo bucket descarta tratar la corrida como una saturación estacionaria simple.

## RTU y periféricos

```text
RTU=6001/6001
RTU_HZ=50.004
RTU_FAILED=0
RTU_SKIPPED=0
RTU_CRC=0
RTU_TIMEOUTS=0
MASTER_RUNTIME=PASS
SLAVE_RUNTIME=PASS
CROSS_COUNT=PASS
DATALOG_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
PERIPHERAL_ACTIVITY=PASS
PERIPHERAL_FRESHNESS=PASS
```

## Comparación full-runtime

| Gate | OK | req/s | AVG us | P95 us | P99 us | MAX us | loop avg us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| POLLING C0 | 120000 | 1000.00 | 884.1 | 1262.6 | 2413.6 | 8972.1 | 591 |
| INT puro R1 | 119744 | 997.86 | 977.5 | 1373.6 | 2659.8 | 8155.3 | 266 |
| D2 fixed 1500 | 120000 | 1000.00 | 937.8 | 1360.8 | 2953.7 | 17182.9 | 879 |
| D3-C | 112989 | 941.57 | 1050.5 | 1754.7 | 4746.5 | 40902.4 | 373 |

D3-C no puede promocionarse.

## Hipótesis siguiente

D3-B corto mostró una sola transición a ACTIVE_POLL a 1000 req/s y se mantuvo estable. D3-C full-runtime combina peor throughput con `loop avg=373 us`, valor intermedio entre INT puro y POLLING C0.

Esto es consistente con —pero no demuestra todavía— que las pausas/jitter del runtime completo hagan que el detector de gaps abandone ACTIVE_POLL repetidas veces. El umbral de salida actual usa dos gaps consecutivos >=1800 us.

No se modifica el threshold todavía. D3-C1 habilitará exclusivamente los hooks de perfil existentes para contar transiciones durante full-runtime.
