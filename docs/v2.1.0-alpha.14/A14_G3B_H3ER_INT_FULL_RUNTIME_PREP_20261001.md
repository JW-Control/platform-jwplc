# Alpha14 — G3B H3E-R con candidato INT — PREP 2026-10-01

## Objetivo

Validar el candidato interno `JWPLC_MODBUS_TCP_INT_GUIDED_RX` dentro del
runtime completo del JWPLC antes de cambiar el default del package.

G3A ya confirmó con Modbus TCP real FC03/125 @1000 req/s:

```text
REQUEST_RATE=999.99 req/s
STATUS_CALL_REDUCTION=82.496 %
AVAILABLE_CALL_REDUCTION=70.207 %
P95_DELTA=+4.433 %
P99_DELTA=+8.363 %
RATE_REGRESSION=NO
```

G3B no busca una nueva mejora de throughput. Busca demostrar que el ahorro de
polling no rompe RTU, DataLog, HMI/TFT, FRAM, RTC, TCA/I-O, botonera ni la
fairness del shared SPI.

## Package y build

El source del package conserva:

```text
#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0
```

El default normal sigue OFF.

Sólo el firmware Master de G3B se compila con:

```text
-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1
```

El Slave no necesita ese override.

La infraestructura P5B/H3E-R recibió un parámetro aditivo
`MasterExtraCppFlags`; vacío conserva exactamente el comportamiento histórico.

## Workload

Se reutiliza H3E-R POST-P4.2:

```text
TCP=Modbus TCP FC03
TCP_QUANTITY=125
TCP_TARGET=1000 req/s
WINDOW=120 s
RTU=50 Hz
RTU_PERIOD=20 ms
RTU_TIMEOUT=25 ms
DATALOG=JWPLCDataLog buffered/autoservice
DISPLAY=HMI_ON_DEMAND_DIRTY
TFT=product wrapper/private backend
FRAM=exercised
RTC=exercised
TCA_I/O=exercised
BUTTONS=exercised
SPI_PROBE=exercised
```

No hay snapshots seriales periódicos dentro de la ventana formal.

## Referencia full-runtime

Referencia POST-P4.2 de 120 s:

| Métrica | Baseline |
|---|---:|
| req/s | 1000 |
| requests | 120000/120000 |
| total TCP payload | 2.168 Mbps |
| useful register data | 2.000 Mbps |
| AVG | 880.3 us |
| P95 | 1253.2 us |
| P99 | 2273.1 us |
| MAX | 8831.5 us |
| loop avg | 633 us |
| loop max | 12012 us |
| RTU | 50.004 Hz |
| RTU errors/skips | 0 |
| peripheral failures | 0 |

La referencia LR600 posterior mantuvo 1000 req/s y RTU 50 Hz durante 600 s,
con P95 1256.7 us y P99 2486.1 us.

## Criterios G3B

Obligatorios:

- 120000/120000 TCP;
- cero timeout/transporte/protocolo/bus-lock;
- RTU dentro de 49.5–50.5 Hz;
- cero RTU failed/skips/CRC/timeouts;
- DataLog sin failed commits;
- FRAM/RTC/I-O/buttons/SPI probe sin fallos;
- Master/Slave TFT físico PASS;
- no reset inesperado;
- package default INT permanece 0;
- compile log Master demuestra `JWPLC_MODBUS_TCP_INT_GUIDED_RX=1`.

Además de los guards H3E-R ya existentes, la revisión para promoción debe
vigilar expresamente:

- P95 contra el baseline POST-P4.2;
- P99 contra el baseline POST-P4.2/LR600;
- MAX como diagnóstico de outliers, porque G3A observó un pico aislado de
  12.267 ms en una corrida INT pero no en la segunda;
- loop avg/max;
- SPI probe y tiempos de FRAM/SD para detectar efectos de fairness.

## Decisión

G3B no cambia automáticamente el default.

Si el full-runtime pasa sin regresión material:

```text
G3B_INT_FULL_RUNTIME=PASS
NEXT=G3C_PROMOTE_INT_DEFAULT_AND_REGRESSION
```

La promoción a default ON sólo se realizará después de revisar el resultado
físico de G3B.
