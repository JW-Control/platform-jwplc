# Alpha14 — R7/R8 cierre de frontier TCP x RTU EXP-MIX — PREP 2026-10-02

## Objetivo

Completar los datos que quedaron abiertos después de R4-R6 usando exactamente
el workload final realista:

```text
2 x DI
2 x DO
2 x AI
2 x AO
```

y el full-runtime normal del JWPLC Basic.

El gate responde dos preguntas:

1. ¿qué ocurre en los puntos TCP 900 y TCP 950 cuando RTU trabaja UNPACED?;
2. ¿cuál es la mayor tasa RTU fija que puede coexistir con un target TCP de
   1000 req/s sin perder integridad ni el target de servicio?

No cambia ningún default productivo.

## Configuración congelada

```text
TCP policy = C0 POLLING
INT default = OFF
D2 default = OFF
D3 default = OFF
E1 default = OFF

W5500 = 26 MHz
FIFO_REUSE = ON
DLEN_REUSE = ON
COPY_OUT_64 = ON

RTU baud = 500000
Master FIFO = 9
Slave FIFO = 8
RX = BULK/BULK
TX = QUEUED/QUEUED
Master framing = GAP
Slave framing = STRUCTURAL
frame gap = 100 us
partial hold = 15000 us
CRC = BITWISE
benchmark timeout = 25 ms

full runtime:
Display + Ethernet + SD/DataLog + FRAM + RTC + I/O/TCA + buttons
```

## Cambios de tooling

El firmware EXP-MIX de benchmark añade únicamente comandos Serial de prueba
para tasas fijas adicionales:

| Comando | RTU req/s |
|---|---:|
| `;` | 350 |
| `,` | 400 |
| `.` | 450 |
| `/` | 500 |
| `=` | 550 |
| `_` | 600 |
| `$` | 650 |
| `%` | 700 |

Esto no modifica `JWPLC_ModbusRTU`, `JWPLC_ModbusTCP`, el core ni APIs
públicas.

## Fase R7 — completar frontera UNPACED

Cada caso: 600 s.

```text
TCP OFF + RTU UNPACED
TCP 900 + RTU UNPACED
TCP 950 + RTU UNPACED
```

El primer caso vuelve a servir como referencia RTU-only del mismo firmware y
misma sesión de qualification.

Después se ejecuta:

```text
TCP UNPACED + RTU OFF
duration = 600 s
```

para obtener el ceiling transaccional Modbus TCP full-runtime del mismo HEAD.

## Fase R8 — RTU compatible con TCP1000

Se fija:

```text
TCP target = 1000 req/s
```

y se barre RTU EXP-MIX con pacing explícito:

```text
0
50
100
150
200
250
300
350
400
450
500
550
600
650
700 req/s
```

Cada punto dura 300 s.

Cada caso exige:

- cero timeout TCP/RTU;
- cero error de transporte/protocolo;
- cero CRC;
- cero rejected/verify fail;
- cero tails descartados;
- mapas DO/AO exactos entre Master y Slave;
- `RTU_PERIODS_SKIPPED=0`;
- tasa RTU real >=99 % del target;
- cero fallo de periférico;
- cero fallo SPI probe;
- readiness completo;
- sin reset de Master ni Slave;
- perfil RTU sin drift.

## Criterios TCP

El runner registra tres niveles:

```text
TCP >= 99.0 % del target
TCP >= 99.5 % del target
TCP >= 99.9 % del target
```

Para afirmar de forma estricta:

```text
"RTU compatible con TCP1000"
```

se usa el criterio:

```text
TCP_TARGET_PCT >= 99.9 %
```

es decir, aproximadamente >=999 req/s sobre target 1000.

Si ningún punto RTU positivo alcanza 99.9 %, el gate no inventa una cifra:
queda `REVIEW_NO_POSITIVE_RTU_AT_TCP_99_9` y conserva los resultados 99.5/99.0
como caracterización.

## Confirmación automática

El mayor RTU que cumpla 99.9 % TCP se repite durante 600 s.

Si no existe un punto 99.9 %, el runner puede seleccionar 99.5 % o 99.0 % para
tener evidencia adicional, pero el estado final seguirá en REVIEW y no se
presentará como cierre estricto de TCP1000.

## Duración

Ventanas puras por default:

```text
R7 RTU OFF/900/950:        3 x 600 s = 30 min
TCP-only unpaced:          1 x 600 s = 10 min
R8 fixed-rate sweep:      15 x 300 s = 75 min
confirmation:              1 x 600 s = 10 min
------------------------------------------------
TOTAL MEDIDO                         = 125 min
```

Más compile/upload, boot, DHCP, snapshots y persistencia de logs.

El gate está diseñado para ejecutarse sin intervención durante varias horas.

## Evidencia

Raíz:

```text
tools/modbus-tcp-benchmark/results/
  a14_tcp_rtu_frontier_closure_YYYYMMDD_HHMMSS/
```

Archivos principales:

```text
SESSION.log
MANIFEST.txt
compile_master.log
compile_slave.log
upload_master.log
upload_slave.log
runner.log
R7_UNPACED_FRONTIER.csv
R8_TCP1000_RTU_SWEEP.csv
R8_SELECTION.json
FINAL_SUMMARY.json
FINAL_STATUS.txt
GATE_STATUS.txt
```

Cada caso conserva además snapshots pre/post y `result.json`.

## Interpretación

Estados de un caso:

```text
PASS_TCP_99_9
PASS_TCP_99_5
PASS_TCP_99_0
TCP_SATURATION_FAIL_CLEAN
RTU_TARGET_FAIL_CLEAN
PRODUCT_FAILURE
```

Sólo `PRODUCT_FAILURE` aborta inmediatamente la campaña.

Un clean saturation o target miss se registra y permite continuar el barrido,
porque justamente forma parte de la búsqueda de frontera.

## Qué NO cierra este gate

R7/R8 no sustituye:

1. la medición final RAW Ethernet TCP RX/TX;
2. la medición final UDP RX/TX y UDP FAST;
3. la coexistencia final TCP + UDP + RTU si se decide publicar una cifra conjunta;
4. la regression/package final de release.

Estos frentes deben ejecutarse después con el frontier R7/R8 ya congelado.
