# Alpha14 — decisión de promoción SINGLE_STATUS

Fecha: 2026-09-30

## Contexto

P8 demostró una mejora al eliminar una segunda consulta `connected()` dentro de
la misma pasada del scheduler RAW:

```text
STATUS_CALLS=-70.521%
STATUS_TIME=-64.431%
SCHEDULER_US_PER_BYTE=-4.556%
SPI_HOLD_US_PER_BYTE=-4.619%
PAYLOAD_EFFECT=-0.305%
```

La optimización validada no corresponde a un cache interno persistente del
`EthernetClient`. Fue una redundancia del scheduler del benchmark:

```text
acceptTcpClient()
  -> connected()

serviceTcpUnlocked()
  -> connected() otra vez
```

## Auditoría de producto

La pila `JWPLC_Ethernet` no contiene un flag productivo equivalente que pueda
promoverse de OFF a ON sin cambiar semántica de API.

`JWPLC_ModbusTCP::serviceServer()` realiza consultas distintas y necesarias:

- disponibilidad RX;
- estado de conexión para decidir cierre cuando no hay bytes.

No se considera correcto introducir un cache persistente de status ni eliminar
consultas distintas únicamente para replicar el resultado del benchmark.

## Decisión

```text
P8_SINGLE_STATUS_DATA=PASS
PACKAGE_FLAG_TO_PROMOTE=NO
PERSISTENT_SOCKET_STATUS_CACHE=FORBIDDEN
SCHEDULER_SAME_PASS_REUSE_POLICY=ADOPT
PUBLIC_API_CHANGE=NO
```

Todo scheduler nuevo que ya haya resuelto la usabilidad/conexión de un cliente
en una pasada debe reutilizar ese resultado dentro de esa misma pasada y evitar
una segunda consulta equivalente.

El resultado no debe sobrevivir a una pasada posterior.

## Aplicación

- S2/S2D ya siguen una política de una sola consulta de conexión cuando no hay
  datos RX disponibles.
- H3E-R deberá seguir la misma regla.
- P4 no debe alterar esta política.
- Un cambio futuro dentro de `JWPLC_ModbusTCP` sólo se aceptará con un A/B
  específico si elimina una consulta demostrablemente redundante, no por
  analogía con P8.

## Estado

```text
SINGLE_STATUS_PRODUCT_DECISION=CLOSED
NEXT_PACKAGE_PROMOTION=FIFO_REUSE_DEFAULT_GATE
```
