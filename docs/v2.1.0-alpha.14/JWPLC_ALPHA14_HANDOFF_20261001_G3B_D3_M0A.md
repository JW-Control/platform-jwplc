# JWPLC Alpha14 Handoff — 2026-10-01 — G3B/D3 M0A

## CURRENT STATE

```text
REPO=JW-Control/platform-jwplc
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
REMOTE_HEAD=8826e4009d22aa14cedf5f9a4db90e0ff6b1158d
PR99=OPEN_DRAFT
ALPHA14_CLOSED=NO
CURRENT_GATE=G3B_D3_M0A_LOAD_CHARACTERIZATION
CURRENT_GATE_STATE=RUNNING_PHYSICAL
PRODUCT_DEFAULT_CHANGED_BY_M0A=NO
```

M0A fue lanzado físicamente antes de migrar. El nuevo chat debe recibir e
interpretar ese output antes de implementar D3-A.

## Defaults/product decisions

```text
W5500=26 MHz
8x2KB
FIFO_REUSE=ON
DLEN_REUSE=ON
COPY_OUT_64=ON
DIRECT_RX=OFF
TCP_RX_BATCH=8
TCP_RX_COMMIT=IMMEDIATE
SAME_PASS_STATUS_REUSE=ADOPT
PERSISTENT_SOCKET_STATUS_CACHE=FORBIDDEN
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
```

## UDP

H4A0.2 matched:

```text
LEGACY=11.025638 Mbps
FAST=12.669195 Mbps
GAIN=+14.91 %
```

Retener:
- BATCH2 como policy de consumidor;
- soporte W5500 INT regs;
- FUSED + COMMIT2 como APIs JWPLC aditivas;
- R1 liveness validado;
- P3I single-CS rechazado;
- API Arduino UDP legacy preservada.

El histórico ~13.87 Mbps tuvo tail-accounting no perfectamente alineado; no
usarlo como ceiling sostenido.

## TCP RAW

P3R FIFO_REUSE:

```text
payload 15.965 -> 16.785 Mbps
+5.141 %
us/B 0.501105 -> 0.476604
```

P4.1 DLEN_REUSE:

```text
payload +0.702 %
us/B -0.698 %
setup/chunk -13.175 %
```

P4.2 COPY_OUT_64:

```text
copy cost -22.376 %
payload +1.737 %
us/B -1.707 %
```

RAW LR600 post-P4.2:

```text
DUT_RX=14.493741 Mbps
PC_OFFERED=14.549637 Mbps
INTERNAL_PAYLOAD=17.094032 Mbps
SPI_OCCUPANCY=98.112 %
ERRORS=0
```

Descartes:
- DMA shared-bus no implementado por ownership inseguro;
- P6 direct-read pequeño/inconcluso;
- P7 coalesced RX commit rechazado;
- P9 batch16/32 rechazados por fairness;
- DIRECT_RX OFF por repeatability insuficiente.

## H3E-R post-P4.2

120 s:

```text
120000/120000
1000 req/s
AVG=880.3 us
P95=1253.2 us
P99=2273.1 us
MAX=8831.5 us
RTU=50.004 Hz
```

600 s:

```text
600000/600000
P95=1256.7 us
P99=2486.1 us
MAX=12221.8 us
RTU=50.000 Hz
all peripherals clean
```

## TCP INT sequence

G2-R1:
- bus efficiency gain confirmed;
- TCP speed gain not confirmed.

G3A real Modbus:
- 1000 req/s maintained;
- status -82.496 %;
- available -70.207 %;
- P95/P99 modestly worse.

G3B INT pure:

```text
Attempt1=119979/120000 @999.79 req/s
Repeat=119744/120000 @997.86 req/s
```

Matched POLLING:

```text
120000/120000
1000 req/s
AVG=884.8
P95=1269.2
P99=2272.5
```

Conclusion: INT pure no promotable.

D1 fast rearm:
- no recuperó latencia.

D2 1500 us:
- short latency ~= polling;
- idle status/available -99.285 %;
- H3E-R 120000/120000 @1000 req/s;
- full-runtime P95=1360.8, P99=2953.7, MAX=17182.9.

C0 current-head POLLING:

```text
120000/120000
1000 req/s
AVG=884.1
P95=1262.6
P99=2413.6
MAX=8972.1
LOOP_AVG=591
```

Conclusion:

```text
D2_TAIL_COST_CONFIRMED
D2_PROMOTION=NO
```

## D3 architecture

Target:

```text
IDLE_INT -> WARM -> ACTIVE_POLL -> COOLDOWN -> IDLE_INT
```

Rules:
- INT idle/low load;
- cooperative polling when busy;
- hysteresis;
- ISR flag only;
- no blocking busy-loop;
- preserve RTU/SD/TFT/FRAM/RTC/I-O/buttons;
- close POLL->INT race;
- thresholds from evidence, not intuition.

## M0A current

```text
IDLE
100 req/s
500 req/s
750 req/s
1000 req/s
POLLING vs ADAPTIVE_D2
15 s active
10 s idle
```

After M0A:

```text
review load curve
-> M0B RAW TCP
-> derive thresholds/hysteresis
-> D3-A
-> D3-B
-> D3-C H3E-R
-> D3-D promotion if clean
-> post-promotion regression
-> formal RAW TCP ~14.49 reconciliation
-> DIRECT_RX only after reconciliation
```

## Do not do

- no promote D2 fixed;
- no return to INT pure;
- no invent thresholds;
- no reopen 30 MHz;
- no persistent socket cache;
- no batch16/32;
- no DIRECT_RX promotion yet;
- no claim ModbusTCP async end-to-end;
- no jump to Alpha30.
