# Alpha14 — G3B-D3-C3 SLOW_STREAK=4 — PREP 2026-10-02

## Hipótesis

C2 redujo `TO_IDLE_INT` de 115 a 1, pero registró 45 transiciones `ACTIVE_POLL→COOLDOWN` en 60 s. La salida ACTIVE actual requiere sólo dos frame gaps consecutivos >=1800 us.

El full-runtime normal introduce jitter por RTU, TFT, SD, FRAM y servicios globales. Dos gaps lentos consecutivos pueden ser demasiado sensibles para distinguir una caída real de carga de una pausa breve del sistema.

## Cambio experimental

Se añade un knob interno con default preservado:

```text
JWPLC_MODBUS_TCP_ACTIVE_SLOW_STREAK=2
```

El gate C3 compila sólo el Master con:

```text
-DJWPLC_MODBUS_TCP_ACTIVE_IDLE_EXIT_US=20000
-DJWPLC_MODBUS_TCP_ACTIVE_SLOW_STREAK=4
```

No cambia:

- FAST_GAP = 1600 us;
- FAST_STREAK = 3;
- SLOW_GAP = 1800 us;
- ACTIVE idle-exit candidate = 20 ms;
- WARM/COOLDOWN idle-exit = 5 ms;
- fallback = 10 ms;
- defaults productivos INT/D3/profile OFF;
- default ACTIVE idle = 5 ms;
- default ACTIVE slow streak = 2.

## Gate

60 s, FC03/125 a 1000 req/s, RTU 50 Hz y full-runtime completo. Profile hooks permanecen ON sólo para diagnóstico.

## Criterio

Comparación primaria contra C2:

```text
TO_COOLDOWN: 45 -> reducción fuerte
TO_ACTIVE_POLL: 46 -> reducción correspondiente
TO_IDLE_INT: mantener cercano a 0
TCP: mantener ~1000 req/s
RTU/periféricos: PASS
```

Si C3 reduce fuertemente COOLDOWN sin degradar el resto, se evaluará luego el impacto de respuesta al bajar de 1000→500 req/s antes de cualquier promoción. Si no lo reduce, no se seguirá elevando el streak sin medir primero la distribución de gaps.
