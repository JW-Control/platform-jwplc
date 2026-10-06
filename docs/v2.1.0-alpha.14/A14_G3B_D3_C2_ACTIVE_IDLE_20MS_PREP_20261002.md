# Alpha14 — G3B-D3-C2 ACTIVE idle 20 ms — PREP 2026-10-02

## Hipótesis

D3-C1 registró 115 hard idle exits en 60 s. El full-runtime observado tiene loop gaps cercanos a 9 ms y transacciones RTU de hasta ~16.6 ms, por lo que el hard idle-exit global de 5 ms es demasiado agresivo cuando D3 ya está en ACTIVE_POLL.

## Cambio experimental

Se añade un knob interno con default preservado:

```text
JWPLC_MODBUS_TCP_ACTIVE_IDLE_EXIT_US=5000
```

El gate C2 compila sólo el Master con:

```text
-DJWPLC_MODBUS_TCP_ACTIVE_IDLE_EXIT_US=20000
```

No cambia:

- IDLE_EXIT normal de WARM/COOLDOWN = 5 ms;
- FAST_GAP = 1600 us;
- FAST_STREAK = 3;
- SLOW_GAP = 1800 us;
- SLOW_STREAK = 2;
- fallback = 10 ms;
- defaults INT/D3/profile OFF.

## Gate

60 s, FC03/125 a 1000 req/s, RTU 50 Hz y todos los periféricos P5-B. Profile hooks ON únicamente para diagnóstico.

Se añaden dos contadores:

```text
D3_PROFILE_ACTIVE_IDLE_EXITS
D3_PROFILE_NONACTIVE_IDLE_EXITS
```

## Criterio

Se busca una reducción fuerte de ACTIVE_IDLE_EXITS y de TO_IDLE_INT frente a C1, manteniendo ~1000 req/s y el resto del runtime limpio.

No se usa este gate para promoción. Si C2 mejora la estabilidad, el siguiente paso será un rerun 120 s sin profile hooks con el override 20 ms antes de decidir cualquier default.
