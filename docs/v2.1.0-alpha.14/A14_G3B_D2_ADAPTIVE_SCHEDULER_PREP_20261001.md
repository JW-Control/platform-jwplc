# Alpha14 — G3B-D2 scheduler INT adaptativo — PREP 2026-10-01

## Motivo

G3B-D1 mantuvo el ahorro de polling pero no recuperó la latencia.

Comparación G3A:

| Métrica | POLLING | INT D1 | Delta |
|---|---:|---:|---:|
| req/s | 999.991 | 999.990 | ~0 % |
| P95 | 1078.053 us | 1153.900 us | +7.036 % |
| P99 | 1142.503 us | 1243.701 us | +8.858 % |
| status calls | 171363 | 30000 | -82.493 % |
| available calls | 201363 | 60000 | -70.203 % |

La micro-optimización del ACK/rearmado no resolvió el tradeoff. La evidencia
apunta a latencia de wake/scheduling: POLLING detecta antes cada request porque
consulta continuamente; INT ahorra bus pero espera el evento y la siguiente
pasada cooperativa.

## D2

Se añade un modo interno adaptativo detrás de:

```text
JWPLC_MODBUS_TCP_INT_HOT_POLL_US
```

El default permanece:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
```

Para D2 se compila únicamente la variante candidata con:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=1
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=1500
```

Política:

1. socket idle: INT-guided;
2. llega RX: wake por INT;
3. después de RX útil: entra en hot-poll durante 1500 us;
4. cada RX nuevo extiende la ventana;
5. si el tráfico sostenido ronda 1 kHz, permanece temporalmente en polling;
6. cuando el tráfico se enfría, limpia RECV y vuelve automáticamente a INT.

No hay API nueva para el usuario.

## Gate

`a14_g3b_d2_scheduler_ab.py` ejecuta:

- HIGH: Modbus TCP FC03/125 @1000 req/s, 2 corridas por variante;
- IDLE: conexión Modbus TCP aceptada, 15 s sin payload, 1 corrida por variante.

Criterios HIGH:

- funcionalidad completa;
- >=999 req/s mediana;
- P95 adaptativo <= +5 % vs POLLING;
- P99 adaptativo <= +8 % vs POLLING.

Criterios IDLE:

- cero payload/errores;
- reducción >=80 % de status calls;
- reducción >=80 % de available calls.

Si ambos pasan:

```text
D2_READY_FOR_H3ER=YES
NEXT=G3B_D2_H3ER
```

El default productivo no cambia hasta superar H3E-R.
