# Alpha14 — H3E-R post-P4.1 — auditoría de composición

Fecha: 2026-09-30

## Objetivo

Demostrar qué rutas de producto estarán realmente activas en el gate H3E-R que
revalida el full runtime después de promover P4.1 DLEN_REUSE.

La auditoría distingue:

- ACTIVE: la ruta forma parte del workload y se valida durante el gate;
- PRESENT_NOT_EXERCISED: la API existe en el package pero este workload no la usa;
- EXPERIMENTAL_NOT_PROMOTED: candidato existente que deliberadamente no forma
  parte del baseline productivo;
- LINKAGE_POLICY: source o archive cualificado usado por el build.

## Ethernet / W5500

| Elemento | Estado próximo gate | Evidencia/contrato |
|---|---|---|
| W5500 SPI 26 MHz | ACTIVE | package + gate exigen 26000000 |
| RX FIFO_REUSE | ACTIVE / DEFAULT | JWPLC_W5500_RX_FIFO_REUSE=1 |
| P4.1 DLEN_REUSE | ACTIVE / DEFAULT | JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=1 |
| DIRECT_TRANSFER_BYTES | OFF | candidato no promovido |
| RX commit | IMMEDIATE legacy path | ModbusTCP usa EthernetClient::read() |
| SINGLE_STATUS | SAME_PASS policy | sin cache persistente |
| socket size | 2 KB baseline | W5100 SSIZE=2048 |

## TCP async

EthernetClient contiene y conserva:

```text
beginConnectAsync / pollConnectAsync
beginWriteAsync   / pollWriteAsync
beginFlushAsync   / pollFlushAsync
beginStopAsync    / pollStopAsync
```

Pero H3E-R usa Modbus TCP Server:

| Proceso | Estado |
|---|---|
| async connect | PRESENT_NOT_EXERCISED — conexión entrante |
| async write | PRESENT_NOT_INTEGRATED_IN_MODBUS_TCP |
| async flush | PRESENT_NOT_USED_BY_MODBUS_TCP |
| async stop | motor presente; wrapper legacy stop() espera hasta finalizar |
| ModbusTCP response TX | LEGACY_BLOCKING_WRITE |

Esto es deliberado para el gate causal post-P4.1. Integrar async TX en
JWPLC_ModbusTCP sería otra variable de producto y requiere un gate separado.

## Display / TFT

El próximo H3E-R exige:

```text
JWPLC_Display=SOURCE
JWPLC_TFT=SOURCE
SPI=SOURCE
DISPLAY_ARCHIVE_LINK=FORBIDDEN
TFT_ARCHIVE_LINK=FORBIDDEN
SPI_ARCHIVE_LINK=FORBIDDEN
```

La HMI usa:

```text
USER_REFRESH_ON_DEMAND
setFields()
setValue()/setText()/setBool()
dirty cache
refreshNeeded()
drawDirty()
```

Las regiones estáticas no se redibujan en cada actualización.

JWPLC_TFT es la API propia del package. Internamente conserva TFT_eSPI como
backend privado, con configuración JWPLC controlada y lock del bus compartido:

```text
JWPLC_TFT -> TFT_eSPI backend privado
TFT SPI=80 MHz
SPI MODE0
ST7789 170x320
jwplcSPI_acquire/release
```

El sketch no depende directamente de TFT_eSPI.

## Modbus RTU

El gate conserva el baseline seguro/productivo:

```text
BAUD=115200
CONFIG=8N1
TARGET=50 Hz
MASTER motor=ASYNC
SLAVE motor=ASYNC
TX=QUEUED activo
RS485 AutoDirection
Master request API cooperativa
task()/masterBusy()/masterDone()
RX_MODE=BYTE
CRC_MODE=BITWISE
SERVER_FRAMING=GAP
```

Los siguientes candidatos NO se introducen silenciosamente:

```text
500000 baud + 100 us = qualified candidate / FAST
BULK RX = experimental, long-run presentó tails antes de H3C
FIFO9/FIFO8 = no universal todavía
STRUCTURAL framing = candidate
CRC LOOKUP = +1.555% candidato, no default
```

ModbusRTU se mantiene enlazado mediante el archive precompiled cualificado por
H3E; desde el commit de adopción f1648654 no hubo cambios en ModbusRTU/RS485 ni
en el core protegido que invaliden esa paridad.

## microSD

Ruta runtime obligatoria:

```text
JWPLCDataLog
BUFFER=4096 B
COMMIT_THRESHOLD=512 B
COMMIT_TIMEOUT=5000 ms
MANUAL_SERVICE=NO
```

Autoservicio:

```text
jwplcSystemTask
 -> jwplcDataLogTickCallback()
 -> JWPLC_SD.serviceDataLogs()
 -> JWPLCDataLog.service()
```

El uso directo de JWPLC_SD en setup para exists/remove del archivo de prueba es
housekeeping; el workload de logging durante la ventana es DataLog.

## Runtime automático del core

El gate comprueba que el core conserve:

```text
ModbusTCP service antes de loop()
loop()
ModbusTCP service después de loop()
Ethernet tick
DataLog tick
Display tick
I/O scan
RTC tick
```

## Otros periféricos

Activos en FULL_RUNTIME_REALISTIC:

| Periférico | Periodo/workload |
|---|---|
| TCA/I/O | 20 ms |
| botones | 20 ms |
| Display telemetry | 100 ms |
| FRAM | 250 ms |
| RTC | 250 ms |
| DataLog append | 1000 ms |
| DataLog verify | 5000 ms |
| SPI probe | 100 ms |

## Guard post-P4.1

Baseline inmediato pre-P4.1:

```text
P95=1275.5 us
P99=3781.0 us
MAX=28179.8 us
```

El siguiente gate exige:

```text
TCP achieved >= 99.9%
RTU = 49.5..50.5 Hz
RTU periods skipped=0
DataLog failed commits=0
Peripheral failures=0
P95 <= baseline +10%
P99 <= baseline +15%
MAX=diagnostic only
```

MAX no veta por sí solo debido a la variación histórica observada incluso en
TCP-only.

## Decisión

```text
P4_1_DLEN_REUSE=PROMOTED
NEXT=H3E_R_P4_1_REGRESSION
P4_2_BLOCKED_UNTIL_REGRESSION_PASS=YES
MODBUS_TCP_ASYNC_TX_INTEGRATION=SEPARATE_FUTURE_GATE
```
