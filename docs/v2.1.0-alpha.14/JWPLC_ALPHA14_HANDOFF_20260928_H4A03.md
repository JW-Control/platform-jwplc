# JWPLC Alpha14 — Handoff H4 post-H3E

Fecha: 2026-09-28  
Repositorio: `JW-Control/platform-jwplc`  
Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Objetivo de este handoff

Continuar Alpha14 desde la requalification post-H3E sin mezclar:

- límites históricos pre-H3E;
- resultados P3 diagnósticos;
- replays H4 invalidados metodológicamente;
- resultados H4 válidos pero todavía instrumentados;
- próximos ceilings que aún no han sido medidos de forma mínimamente invasiva.

Este documento es la referencia principal para abrir un chat nuevo.

---

## Estado ejecutivo

```text
ALPHA14_H3E=CLOSED_PASS
R0_FINAL_READINESS=PASS_RETAINED
R1_ARDUINO_IDE_PHYSICAL=PASS_RETAINED
PR99_MERGE_READINESS=PAUSED
POST_H3E_THRESHOLD_REQUALIFICATION=OPEN

CURRENT_AREA=H4A_RAW_ETHERNET_REQUALIFICATION
CURRENT_SUBAREA=UDP_RX_FAST_PATH
NEXT=H4A0.3A_MINIMAL_INSTRUMENTATION_DURATION_SWEEP
```

No avanzar todavía a H4A1 Modbus TCP-only.

---

## Runtime/artifacts finales que NO se reabren

```text
W5500_SPI_HZ=26000000
W5500_30MHZ=PHYSICAL_FAIL_NOT_REOPEN
SOCKET_TOPOLOGY=8x2KB

JWPLC_Display.a
SHA256=52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986

libJWPLC_TFT.a
SHA256=5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738

core.a
SHA256=4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJWPLC_ModbusRTU.a
SHA256=486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE

libJW_SD.a
SHA256=E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1
```

H3E.5 final:

```text
TCP=1000.00 req/s
RTU=50.004 Hz
TCP=120000/120000
RTU=6001/6001
RTU_TIMEOUTS=0
RTU_CRC_ERRORS=0
SD_DATALOG_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
TFT_MASTER_SLAVE_PHYSICAL=PASS
LOOP_MAX_US=8212
```

---

## Por qué se reabrió H4

Antes de H3E existían tails de servicio del orden de 19–24 ms:

```text
RTU_H3C1_SERVICE_GAP_MAX_US=19307
RTU_H3C1_LOOP_GAP_MAX_US=19301
DISPLAY_HISTORICAL_WORST ~= 23.6 ms
```

H3E atribuyó el peor gap al Display/TFT histórico y lo redujo drásticamente.

Por tanto los ceilings TCP/RTU medidos antes de H3E pueden estar deprimidos por
latencia del runtime viejo y deben requalificarse.

---

# H4A0 — RAW Ethernet baseline post-H3E

Package final, autoload normal, RTU OFF, W5500 26 MHz.

Resultado:

```text
TCP_RX_MEDIAN=13.577425 Mbps
TCP_TX_MEDIAN=4.723816 Mbps
UDP_RX_EFFECTIVE_MEDIAN=11.052889 Mbps
UDP_TX_MEDIAN=5.056589 Mbps
TRANSPORT_ERRORS=0
TFT_PHYSICAL=PASS
A14_H4A0_RAW_ETHERNET_BASELINE_GATE=PASS
```

Interpretación:

- TCP RX actual está aproximadamente en el mejor rango histórico.
- UDP RX productivo sigue usando la ruta Arduino legacy.
- El mejor UDP histórico no estaba productizado.

---

# P3 UDP histórico — qué se había logrado realmente

El mejor candidato P3 fue diagnóstico, no producto:

```text
BATCH2
+ W5500 INT GPIO15
+ FUSED UDP RX
+ COALESCED COMMIT2
+ POST-COMMIT REARM R1
+ SERIAL IDLE / QUIESCENCE R2
```

Boundary:

```text
payload=1016 B
W5500 pseudo-header=8 B
record=1024 B
batch2=2048 B exactos
socket RX=2 KB
```

P3J-R2 histórico:

```text
COMMIT2_R1_MEDIAN=13.867969 Mbps
FUSED_MEDIAN=12.794340 Mbps
AGGREGATE_GAIN=+8.39 %
```

P3K histórico:

```text
UDP_MEDIAN=13.866349 Mbps
TCP_MEDIAN=13.412507 Mbps
UDP_VS_TCP=+3.38 %
```

IMPORTANTE:

- P3B/P3H/P3J/P3J-R1/P3J-R2/P3K usaban por defecto ventanas de 5 s.
- P3K acumulaba varias ventanas balanceadas, no una sola corrida larga.
- `P3_PRODUCT_SOURCE_MUTATION=NO`.
- La decisión histórica fue no reemplazar silenciosamente `parsePacket()/read()`.

---

# H4A0.1 — replay P3K post-H3E

Se recompuso y ejecutó el candidato histórico.

```text
UDP_MEDIAN=13.331017 Mbps
TCP_MEDIAN=12.047901 Mbps
UDP_VS_HISTORICAL_P3K=-3.86 %
UDP_RECOVERY_OF_HISTORICAL=96.14 %
```

UDP fue estable pero TCP mostró fuerte dispersión (~10.99 a ~13.43 Mbps).

Conclusión:

```text
PATCH_APPLICATION=CONFIRMED
UDP_FAST_PATH=PROMISING
TCP_PARITY_RESULT=INVALID_FOR_CURRENT_CEILING
P3K_REPLAY_METHOD_FOR_FINAL_DECISION=NO
```

No usar H4A0.1 para declarar ceiling actual.

---

# H4A0.2 — matched clean UDP A/B

Diseño válido:

```text
LEGACY_DIAG vs FAST_DIAG
payload=1016 B
3 x 15 s por variante
fresh upload por corrida
orden:
LEGACY1 FAST1 FAST2 LEGACY2 LEGACY3 FAST3
TCP interleaving=NO
SERIAL IDLE + QUIESCENCE en ambos
instrumentación común en ambos
```

FAST:

```text
BATCH2 + INT GPIO15 + FUSED + COMMIT2 + R1
```

Resultado final:

```text
LEGACY_MEDIAN=11.025638 Mbps
LEGACY_MIN=10.332888 Mbps
LEGACY_MAX=11.294668 Mbps

FAST_MEDIAN=12.669195 Mbps
FAST_MIN=12.664120 Mbps
FAST_MAX=12.675336 Mbps

FAST_VS_LEGACY_GAIN=+14.91 %
FAST_GAIN_GE_10PCT=True
TRANSPORT_ERRORS=0
SPI_LOCK_ERRORS=0
TFT_PHYSICAL=PASS
A14_H4A02_CLEAN_UDP_AB=PASS
```

FAST mostró excelente repetibilidad:

```text
12.664120
12.675336
12.669195 Mbps
range ~= 0.089 %
```

Runtime FAST:

```text
PACKETS_PER_ACTIVE_HOLD=2.000000
EMPTY_HOLD_COUNT=0
SPI_READS_PER_PACKET=3.000
```

Legacy:

```text
SPI_READS_PER_PACKET ~= 4.42
EMPTY_HOLDS elevados y variables
```

Interpretación válida:

```text
FAST_PATH_VALUE=CONFIRMED
FAST_PATH_STABILITY=EXCELLENT
BATCH2_RUNTIME=CONFIRMED
EMPTY_POLLING=ELIMINATED
SPI_READS_PER_PACKET=IMPROVED
PRODUCT_SOURCE_MUTATION=NO
```

Interpretación NO válida:

```text
12.669 Mbps = ceiling final
```

No se acepta esa conclusión todavía.

---

## Problemas detectados en H4A0.2

### 1. Ceiling histórico no recuperado

```text
HISTORICAL_P3K_FAST=13.866349 Mbps
CURRENT_H4A02_FAST=12.669195 Mbps
DELTA ~= -8.63 %
```

Con H3E reduciendo drásticamente latencia Display/TFT, y manteniendo iguales
W5500/SPI/topología/path, el ceiling post-H3E no debería aceptarse como inferior
sin explicar la causa.

Hipótesis principal actual:

```text
MEASUREMENT_INTRUSION
```

El H4A0.2 todavía instrumenta el hot path con:

- counters W5100;
- read-call/read-byte counters;
- hold counters;
- `micros()` por hold;
- métricas de interrupción;
- aritmética/contadores de diagnóstico.

La medición puede estar frenando el path que intenta medir.

### 2. LOOP_GAP_MAX actual no es confiable

H4A0.2 mostró aproximadamente:

```text
LEGACY_LOOP_GAP_MAX_US_MAX=115260
FAST_LOOP_GAP_MAX_US_MAX=126313
```

No interpretar esto como regresión TFT.

El lifecycle actual hace snapshots Serial alrededor de resets de métricas y puede
contaminar el primer gap posterior. El benchmark de throughput no debe usarse a
la vez como benchmark de loop latency.

### 3. Offered load del PC no es throughput físico

El PC reporta ~0.9–1.05 Gbps de `sendto()`, imposible como tráfico físico sobre
100BASE-TX.

Por tanto:

```text
PC_MBPS = offered/enqueued load
DUT_MBPS = throughput efectivo absorbido
LOSS_PERCENT actual = no es packet loss físico publicable
```

Se necesita más adelante un sender paced para encontrar máximo throughput
sostenido con pérdida controlada/cero.

### 4. INT contribution aún no aislada

FAST reporta muy pocos ISR físicos y muchos wake cycles:

```text
ETH_INT_ISR_COUNT ~= 2
UDP_RX_INT_WAKE_COUNT ~= 11.7k
```

Probable comportamiento level-guided con INT LOW + RSR/rearm.

No asumir que INT aporta throughput hasta component ablation.

---

# Falsos negativos / harness issues recientes

## H4A0.2 intento inicial

Abortó porque exigía:

```text
UDP_RX_EMPTY_HOLD_COUNT == 0
```

y observó un único hold vacío.

Además se descubrió que FAST no tenía aplicado `P3J-R2 SERIAL IDLE`, por lo que
el `STOP` UDP era consumido como payload por el path FUSED y el DUT seguía en
`MODE=UDP_RX`.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
```

Correcciones:

- SERIAL IDLE + quiescence en LEGACY y FAST;
- instrumentación matched;
- fresh upload por corrida;
- empty hold medido por tasa, no cero absoluto;
- orden balanceado.

---

# Siguiente gate — H4A0.3A

Objetivo:

Determinar si el ~8.6 % faltante frente a P3K histórico es causado por
instrumentación o por una regresión real.

## Modo PERFORMANCE minimal-instrumentation

Durante la ventana medida conservar sólo lo imprescindible:

```text
rxBytes
rxPackets
transportErrors
spiLockErrors
```

Evitar en hot path:

```text
Serial.print
micros() por hold
uint64 counters de diagnóstico
W5100 read-call counters
W5100 read-byte counters
hold timing
loop-gap profiling
instrumentación detallada INT
```

Las características del candidato se verifican principalmente mediante source
contract antes de compilar.

## Duraciones

Mismo candidato FAST:

```text
5 s x 3
15 s x 3
30 s x 3
```

Objetivo:

```text
si >= ~13.87 Mbps sostenido:
  instrumentación era la causa / histórico recuperado

si 5 s ~=13.87 pero 15/30 s ~=12.7:
  existe degradación dependiente de duración

si 5/15/30 s ~=12.7:
  existe regresión real post-P3 que debe aislarse
```

No avanzar a component ablation antes de resolver esto.

---

# Después — H4A0.3B Component Ablation

Con harness PERFORMANCE mínimamente invasivo:

```text
A = LEGACY
B = + BATCH2
C = + INT
D = + FUSED
E = + COMMIT2
F = + R1
```

Primera pasada:

```text
5 s x 3 por variante
```

Luego sólo candidatos relevantes:

```text
15 s x 3
30 s x 3
```

Candidato final:

```text
60 s
300 s
```

La métrica primaria será throughput efectivo DUT calculado con tiempo del PC.

Separar gates:

```text
PERFORMANCE:
Mbps / packets / errors

LATENCY:
loop avg/P95/P99/max
SPI hold timing
Display effects
```

No volver a mezclar ambos objetivos en un único hot-path altamente instrumentado.

---

# Después de cerrar H4A0

Orden restante:

```text
H4A1 Modbus TCP-only frontier
H4B TCP + RTU50 frontier
H4C RTU FAST 500k 100us vs 75us
H4D SFIFO/BULK post-H3E
H4E coexistencia extrema TCP+RTU para datasheet
final R0/R1 revalidation only if product changes
PR99 + CI + release closure
```

Meta deseada de coexistencia, sólo como zona de exploración:

```text
TCP 1000 req/s + RTU 800-1000 tx/s
```

No asumirla como PASS hasta medirla.

---

# Reglas de continuidad para el próximo chat

1. Un gate por vez.
2. No productizar FAST antes de cerrar H4A0.3A/3B.
3. No aceptar 12.669 Mbps como nuevo ceiling.
4. No reabrir W5500 30 MHz.
5. No quitar periféricos del autoload.
6. No romper APIs Arduino legacy.
7. Separar HARNESS_FAILURE de PRODUCT_FAILURE.
8. Source contract antes de runtime.
9. Medición PERFORMANCE mínimamente invasiva.
10. Sólo el candidato final recibe soak largo.
11. Si cambia producto/artifact, rerun R0/R1.
12. Documentación en español.

---

# Primer trabajo del nuevo chat

```text
1. Leer este handoff.
2. Leer ALPHA14_STATUS.md.
3. Leer A14_POST_H3E_THRESHOLD_REQUALIFICATION_20260928.md.
4. Inspeccionar P3 histórico sólo para reproducibilidad, no como estado actual.
5. Diseñar/implementar H4A0.3A minimal-instrumentation.
6. PATCH -> RE-READ -> SYNTAX -> CONTRACT -> COMMIT.
7. Entregar un único comando de ejecución al usuario.
```

No avanzar a H4A0.3B hasta interpretar H4A0.3A.
