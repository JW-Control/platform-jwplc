# Alpha14 — G2 INT GPIO15 TCP A/B — PREP 2026-10-01

## Objetivo

Comparar el scheduler TCP polling actual contra un candidato guiado por INTn
del W5500 en GPIO15.

G2 es un gate diagnóstico A/B. No productiza todavía INT.

## Base común

Ambas variantes usan exactamente:

```text
W5500_SPI_HZ=26000000
SOCKET_TOPOLOGY=8x2KB
FIFO_REUSE=ON
DLEN_REUSE=ON
COPY_OUT_64=ON
DIRECT_RX=OFF
RX_COMMIT=IMMEDIATE
TCP_RX_BATCH=8
PROFILE_HOOKS=ON
SINGLE_STATUS_SAME_PASS=ON
```

Activar `SINGLE_STATUS` en las dos variantes evita que el candidato INT gane
por la redundancia ya identificada en P8.

## Variantes

### POLLING

Scheduler RAW actual sin gating por INT.

### INT_GUIDED

GPIO15 recibe INTn del W5500.

Contrato:

- ISR sólo marca `pending`;
- cero acceso SPI desde ISR;
- máscara de socket: `RECV | DISCON | TIMEOUT`;
- el scheduler evita adquirir el shared SPI cuando no hay pending ni INT LOW;
- todo acceso a registros W5500 ocurre fuera de ISR y dentro del ownership SPI;
- tras una pasada activa se limpia el evento y se hace un one-shot
  `Sn_RX_RSR` para no perder liveness si queda payload por drenar;
- INT LOW después del rearm también vuelve a marcar pending.

## Cargas

Se repiten las tres geometrías de G1:

| Caso | Duración | Carga |
|---|---:|---|
| IDLE | 15 s | conexión activa, 0 payload |
| CONTROLLED | 30 s | RAW TCP 12 B @ 1000 Hz |
| SATURATED | 30 s | RAW TCP 4096 B continuo |

CONTROLLED conserva el significado de G1: imita la cadencia/tamaño de ingress
de una solicitud FC03, pero no es Modbus TCP completo.

## Métricas

Por variante/carga:

- DUT Mbps / operaciones por segundo;
- SPI hold count/total/avg/max;
- SPI occupancy;
- service passes/active/empty;
- status calls;
- available calls y available=0;
- ISR count;
- INT skip/wake;
- low fallback;
- rearm por RSR/pin.

## Liveness

El caso CONTROLLED INT ejecuta además:

1. cierre de la conexión medida;
2. liberación del freeze;
3. detección de IDLE mediante DISCON;
4. verificación de que la máscara INT antigua queda deshabilitada;
5. nueva conexión;
6. reconfiguración INT sobre el socket activo;
7. RX de payload después de reconectar.

Debe terminar:

`G2_INT_RECONNECT_LIVENESS=PASS`.

## Decisión

El harness no promueve automáticamente INT. Al terminar deja:

```text
G2_INT_TCP_DECISION=REVIEW_REQUIRED
G2_PRODUCT_PROMOTION=NOT_YET
```

La decisión posterior debe separar:

- eficiencia del bus en IDLE/CONTROLLED;
- impacto de throughput en SATURATED;
- liveness/reconnect;
- eventual efecto full-runtime H3E-R.

Si el A/B es favorable, el siguiente paso será definir la política productiva
interna y ejecutar una regresión real Modbus TCP + RTU + periféricos antes de
hacerla default.
