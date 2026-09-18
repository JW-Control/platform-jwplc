# Alpha14 — NB1: cierre cooperativo del lifecycle TCP

## Objetivo

Eliminar esperas internas prolongadas del lifecycle TCP bajo ownership SPI compartido sin reducir el timeout productivo ni romper la API Arduino legacy.

NB1 cubre dos operaciones:

- cierre de conexión TCP (`stop`);
- espera de drenaje TCP (`flush`).

Las APIs legacy se conservan. Las extensiones cooperativas se añaden de forma incremental para que el runtime JWPLC pueda avanzar las operaciones en pasos cortos.

## Diseño

Se añadieron las siguientes extensiones a `EthernetClient`:

```text
beginStopAsync()
pollStopAsync()
stopAsyncInProgress()
cancelStopAsync()

beginFlushAsync()
pollFlushAsync()
flushAsyncInProgress()
cancelFlushAsync()
```

Las rutas legacy `stop()` y `flush()` conservan su interfaz y se implementan sobre el mismo motor cooperativo.

El raw benchmark usa `stopAsync` dentro de ventanas cortas del mutex SPI global. No se adoptó una reducción de `setConnectionTimeout()` como solución.

## Evidencia de stopAsync

La causa original del hold extremo fue reproducida antes del cambio:

| Timeout diagnóstico | Hold extremo TCP observado |
|---:|---:|
| 1000 ms | ~1,000,460–1,000,754 us |
| 200 ms | 199,547 us |

Una variante diagnóstica de 1 ms eliminó la firma larga y, con observación física válida, no presentó diagnóstico visual SPI.

Tras introducir `stopAsync()`, NB1-C2 se ejecutó dos veces con timeout productivo de 1000 ms.

Resultado acumulado:

- 40 pares TCP_RX → TCP_TX;
- firmas de ~1 s: 0;
- holds >10 ms: 0;
- eventos visuales SPI: 0;
- TCP_RX hold máximo por corrida: 5845 / 5707 us;
- TCP_TX hold máximo por corrida: 2549 / 2488 us;
- throughput TCP_RX combinado aproximado: 13.579 Mbps;
- throughput TCP_TX combinado aproximado: 4.566 Mbps.

Conclusión:

```text
NB1_STOP_ASYNC=PASS_REPRODUCED
```

## Semántica física del TX del W5500

NB1-D2R capturó directamente los registros TX para un envío de 1024 B:

```text
BEFORE:
TX_WR=26223
TX_RD=26223
TX_FSR=2048

AFTER_BUFFER:
TX_WR=26223
TX_RD=26223
TX_FSR=2048

AFTER_SEND:
TX_WR=27247
TX_RD=26223
TX_FSR=1024

SEND_OK:
TX_WR=27247
TX_RD=27247
TX_FSR=1024
SEND_OK_ELAPSED_US=136
```

Por tanto:

- mover datos y `TX_WR` sin `SEND` no produjo el estado de FSR pendiente esperado;
- `SEND_OK` y `TX_RD == TX_WR` no equivalen a `TX_FSR == SSIZE`;
- el contrato de `flush` no debe degradarse a esperar únicamente `SEND_OK`.

## Evidencia de flushAsync

NB1-D2S ejecutó 20 ciclos reales de 1024 B usando la semántica `TX_FSR == W5100.SSIZE` como condición terminal.

Corrida formal completa:

```text
CYCLES_COMPLETED=20
FLUSH_BEGIN_PENDING_COUNT=20
FLUSH_BEGIN_IMMEDIATE_COUNT=0
FLUSH_POLL_COUNT_TOTAL=11060
FLUSH_POLL_PENDING_TOTAL=11020
FLUSH_TIMEOUT_COUNT=0

FLUSH_DURATION_MIN_US=45643
FLUSH_DURATION_AVG_US=46418
FLUSH_DURATION_MAX_US=46740

FLUSH_POLL_HOLD_MAX_US=219
SERVICE_SPI_HOLD_MAX_US=1877
LOOP_GAP_MAX_US=2282
SPI_LOCK_ERRORS=0

FIRST_AFTER_SEND_TX_FSR=1024
FIRST_FLUSH_DONE_TX_FSR=2048
CLIENT_RX_BYTES=20480
VISUAL_SPI_EVENTS=0
```

La corrida anterior también completó 20 ciclos sin timeout y presentó un máximo de servicio de 1860 us, pero quedó interrumpida en el prompt de observación física y no se usa como PASS formal completo.

Conclusión:

```text
NB1_FLUSH_ASYNC_COOPERATIVE=PASS
NB1_FLUSH_SEMANTICS=TX_FSR_FULL
```

## Contrato de timeout

El cierre NB1 exige además prueba estática de que:

- los dos constructores de `EthernetClient` siguen inicializando `_timeout(1000)`;
- el raw benchmark no contiene `setConnectionTimeout(...)`;
- no quedan llamadas `tcpClient.stop()` bloqueantes en el raw benchmark candidato;
- los hashes de header, implementación y firmware candidato coinciden con los calificados físicamente.

Esto se verifica en `A14 NB1-E`.

## Cierre NB1-E

NB1-E verificó directamente sobre el working tree calificado:

```text
DEFAULT_TIMEOUT_1000_CONSTRUCTOR_COUNT=2
RAW_SET_CONNECTION_TIMEOUT_COUNT=0

RAW_BLOCKING_STOP_COUNT=0
RAW_BEGIN_STOP_ASYNC_COUNT=2
RAW_POLL_STOP_ASYNC_COUNT=1
RAW_STOP_ASYNC_IN_PROGRESS_COUNT=1

FLUSH_USES_SOCKET_SEND_AVAILABLE=True
FLUSH_USES_W5100_SSIZE=True

TRACKED_DIRTY_COUNT_FINAL=4
STAGED_COUNT_FINAL=0
```

Además se mantuvieron intactos los hashes protegidos de `core.a` y `libJW_SD.a`, y los hashes de header, implementación y firmware raw coincidieron con los candidatos previamente calificados.

Marcadores finales del gate:

```text
NB1_DEFAULT_CONNECTION_TIMEOUT_MS=1000_SOURCE_PROVEN
NB1_RAW_TIMEOUT_OVERRIDE=ABSENT
NB1_RAW_BLOCKING_STOP=ABSENT
NB1_LEGACY_API_COMPATIBILITY=PRESERVED
NB1_STOP_ASYNC_PHYSICAL=PASS_REPRODUCED_40_PAIRS
NB1_FLUSH_ASYNC_PHYSICAL=PASS_20_CYCLES
NB1_FLUSH_SEMANTICS=TX_FSR_FULL
A14_NB1_TCP_LIFECYCLE_CLOSURE=PASS
```

## Estado final

```text
NB1_STOP=PASS_REPRODUCED
NB1_FLUSH=PASS
NB1_TIMEOUT_CONTRACT=PASS
NB1_LEGACY_COMPATIBILITY=PASS
NB1_OVERALL=CLOSED_PASS
```

El siguiente paso operativo es restaurar el DUT desde el firmware especializado de diagnóstico D2S al raw candidate normal de Alpha14 (26 MHz / 8 chunks / async stop) y ejecutar una verificación mínima de un par TCP_RX → TCP_TX.

## Siguiente bloque

NB2 auditará DNS y resolución de hostname. El objetivo es eliminar esperas internas largas del runtime sin romper `connect(const char*, port)` legacy.

No se mezcla NB2 con optimización de throughput. La caracterización de duty-cycle SPI, buffers de socket y SEND/polling pertenece al bloque de performance posterior a la limpieza de rutas bloqueantes.
