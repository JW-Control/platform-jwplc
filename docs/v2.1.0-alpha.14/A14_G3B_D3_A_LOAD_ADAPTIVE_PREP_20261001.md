# Alpha14 — G3B-D3-A load-adaptive scheduler — PREP 2026-10-01

## Preconditions

```text
M0A=PASS_CHARACTERIZATION
M0B=PASS_EVIDENCE_REUSE
D2_DEFAULT_PROMOTION=NO
PRODUCT_DEFAULT_INT=0
PRODUCT_DEFAULT_HOT_POLL=0
```

## Hallazgo que guía D3-A

M0A acota una transición clara:

```text
500 req/s -> 2000 us entre requests:
  D2 todavía ahorra algo de polling
  pero P95/P99 empeoran ~15/18 %

750 req/s -> 1333 us:
  D2 ya converge a POLLING
  latencia prácticamente equivalente

1000 req/s -> 1000 us:
  D2 ~= POLLING
```

La frontera útil está entre 500 y 750 req/s.

## Regla de medición

La carga no debe estimarse por cada chunk TCP leído.

Debe medirse por actividad de trama Modbus completa, para que una trama
fragmentada en varios reads no parezca varias llegadas.

## Máquina de estados candidata

```text
IDLE_INT
  -> WARM
  -> ACTIVE_POLL
  -> COOLDOWN
  -> IDLE_INT
```

### IDLE_INT

- RECV IRQ habilitada;
- ISR = flag only;
- fallback preservado.

### WARM

- medir inter-arrival de tramas completas;
- no entrar a polling por un único request aislado;
- acumular evidencia de carga sostenida.

### ACTIVE_POLL

- polling cooperativo por pasada de task();
- no busy-loop;
- RTU/SD/TFT/FRAM/RTC/I-O/botones siguen recibiendo servicio;
- no depender de una IRQ RECV por cada request.

### COOLDOWN

- confirmar que la carga bajó;
- cerrar la carrera RX antes de volver a dormir por INT;
- preservar DISCON/TIMEOUT.

## Umbrales iniciales de prueba

No son defaults finales. Son el primer candidato derivado de M0A:

```text
FAST_GAP_US≈1600
FAST_STREAK≈3 frames
SLOW_GAP_US≈1800
SLOW_STREAK≈2 frames
IDLE_EXIT_US≈5000
```

Racional:

- 1600 us queda entre 2000 us (500 req/s) y 1333 us (750 req/s);
- 1800 us crea histéresis sin convertir 500 req/s en polling permanente;
- varias tramas consecutivas evitan flapping por jitter aislado;
- idle exit independiente permite volver a INT cuando el tráfico se detiene.

Estos valores deben validarse y pueden cambiar en D3-B.

## RECV IRQ masking

No introducir masking adicional de RECV en el primer parche D3-A si no es
necesario para demostrar la política. Mantener el cambio mínimo.

Si después se evalúa masking durante ACTIVE_POLL, abrir un gate de liveness
específico para la transición POLL -> INT.

## Gate siguiente

D3-B debe comparar al menos:

```text
IDLE
100 req/s
500 req/s
750 req/s
1000 req/s
```

y registrar transiciones de estado además de latencia/calls.

Sólo después abrir H3E-R D3-C.
