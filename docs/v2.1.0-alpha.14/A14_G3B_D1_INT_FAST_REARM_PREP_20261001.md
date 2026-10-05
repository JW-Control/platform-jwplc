# Alpha14 — G3B-D1 INT fast rearm — PREP 2026-10-01

## Motivo

El control matched POLLING de G3B-D0 volvió a:

```text
120000/120000
1000.00 req/s
AVG=884.8 us
P95=1269.2 us
P99=2272.5 us
```

mientras G3B-R1 con INT produjo:

```text
119744/120000
997.86 req/s
AVG=977.5 us
P95=1373.6 us
P99=2659.8 us
```

La regresión queda aislada en la implementación INT actual, no en el package
general ni en el harness.

## Cambio G3B-D1

Se conserva:

- `JWPLC_MODBUS_TCP_INT_GUIDED_RX=0` por default;
- GPIO15;
- máscara `RECV | DISCON | TIMEOUT`;
- ISR sin SPI;
- fallback 10 ms;
- APIs públicas intactas.

Se modifica sólo el rearmado interno del candidato.

### Antes

Al final de cada wake RX:

1. `readSnIR()`;
2. clear de flags;
3. `readSnRX_RSRStable()`;
4. lectura del pin INT.

Ese camino se ejecutaba por cada request útil y consumía margen de latencia.

### Ahora

Al inicio de la pasada RX:

1. se limpia únicamente `SnIR::RECV`;
2. se drena el RX normal;
3. se mantiene localmente el número de bytes conocidos aún pendientes;
4. al terminar, pending se rearma si:
   - ya había bytes adicionales conocidos; o
   - INTn sigue LOW.

`DISCON` y `TIMEOUT` no se limpian en el ACK rápido, por lo que siguen
despertando el lifecycle normal.

El fallback de 10 ms permanece como red de seguridad.

## Seguridad para requests pipelined

La cantidad `availableBytes` se decrementa localmente sólo cuando el build INT
está activo. Si una trama completa termina pero ya había bytes adicionales en
el RX del W5500, el scheduler deja pending activo para la siguiente pasada.

Cuando INT está OFF, el flujo POLLING conserva el comportamiento anterior.

## Gate de confirmación

Antes de volver al H3E-R completo se repite G3A con el candidato D1:

```text
FC03
quantity=125
1000 req/s
30 s
POLLING, INT_GUIDED, INT_GUIDED, POLLING
```

Objetivo principal:

- recuperar la latencia de POLLING sin perder la reducción de status/available;
- 30000/30000 en cada corrida;
- cero errores;
- P95/P99 dentro de guards;
- mecanismo INT sigue confirmado.

Si pasa, se repite G3B full-runtime con el mismo override INT.

## Estado

```text
PRODUCT_DEFAULT_CHANGED=NO
G3C=BLOCKED
NEXT=G3B_D1_G3A_CONFIRMATION
```
