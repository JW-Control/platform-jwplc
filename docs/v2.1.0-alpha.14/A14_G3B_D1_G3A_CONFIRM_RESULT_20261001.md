# Alpha14 — G3B-D1 fast rearm — G3A confirmation — resultado 2026-10-01

## Cierre

```text
HEAD=cbe70a072139b3fc841673768750baa6d47a3eea
G3B_D1_G3A_RESULT=PASS_FUNCTIONAL
G3B_D1_LATENCY_RECOVERY=NO
G3B_D1_INT_MECHANISM=CONFIRMED
PRODUCT_DEFAULT_CHANGED=NO
G3C=BLOCKED
NEXT=G3B_D2_ADAPTIVE_HOT_POLL
```

D1 redujo el costo de rearmado INT eliminando del final de cada wake
`readSnIR()+readSnRX_RSRStable()`, pero la confirmación Modbus TCP real mostró
que la latencia no regresó al nivel POLLING.

## Resultados

| Métrica | POLLING | INT D1 | Delta |
|---|---:|---:|---:|
| req/s mediana | 999.991 | 999.990 | ~0 % |
| P95 | 1078.053 us | 1153.900 us | +7.036 % |
| P99 | 1142.503 us | 1243.701 us | +8.858 % |
| MAX mediana | 4083.850 us | 2068.000 us | menor |
| loop max | 3586 us | 3108 us | menor |
| status calls | 171363 | 30000 | -82.493 % |
| available calls | 201363 | 60000 | -70.203 % |
| available-zero | 141363 | 0 | eliminado |

Las cuatro corridas completaron 30000/30000 requests con
`G3A_FUNCTIONAL_PASS=YES`.

## Comparación contra G3A INT anterior

G3A inicial, antes de D1:

```text
P95_INT_MEDIAN=1106.258 us
P99_INT_MEDIAN=1221.951 us
STATUS_REDUCTION=82.496 %
AVAILABLE_REDUCTION=70.207 %
```

G3B-D1:

```text
P95_INT_MEDIAN=1153.900 us
P99_INT_MEDIAN=1243.701 us
STATUS_REDUCTION=82.493 %
AVAILABLE_REDUCTION=70.203 %
```

La reducción de polling se conserva, pero la latencia no mejora; P95 incluso
sube respecto al candidato previo.

## Interpretación

La evidencia apunta a que el costo dominante no es sólo el rearmado de
registros W5500. Existe un tradeoff más fundamental:

- POLLING consulta continuamente y detecta la llegada del request muy pronto;
- INT evita ese trabajo, pero el request debe esperar al evento + siguiente
  pasada cooperativa antes de entrar al parser;
- a 1 kHz, ese retardo adicional reduce el margen request-response.

Por tanto seguir micro-optimizando el ACK del IRQ no es suficiente.

## Próximo candidato

G3B-D2 probará una política adaptativa interna:

- INT-guided cuando el socket está idle o con tráfico esporádico;
- después de recibir tráfico, una ventana corta de polling "hot";
- si siguen llegando requests antes de vencer la ventana, el scheduler conserva
  polling temporalmente;
- cuando el flujo se enfría, vuelve automáticamente a INT.

Objetivo:

- a 1000 req/s: recuperar latencia/headroom cercano a POLLING;
- a baja carga/idle: conservar gran parte del ahorro SPI de INT;
- sin API nueva ni perfil visible al usuario.

El default productivo permanece sin cambios hasta validación.
