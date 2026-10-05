# Alpha14 — G3B-D3-C1 full-runtime profile — PREP 2026-10-02

## Objetivo

Determinar si el fallo de headroom D3-C se correlaciona con salidas repetidas de `ACTIVE_POLL` durante el runtime completo.

## Gate

Duración diagnóstica: 60 s. TCP FC03/125 a 1000 req/s con RTU 50 Hz y todos los periféricos del H3E-R.

Build Master:

```text
-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1
-DJWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE=1
-DJWPLC_MODBUS_TCP_ENABLE_PROFILE_HOOKS=1
```

Los defaults productivos permanecen en 0.

## Telemetría

El snapshot expone sólo bajo PROFILE_HOOKS: estado final, frames completos, transiciones WARM/ACTIVE/COOLDOWN/IDLE, active-poll passes, fallback passes, fallback realigns y último frame gap.

## Interpretación

```text
ACTIVE_STABLE=TRUE -> buscar otra causa; no mover slow threshold.
TRANSITION_FLAPPING=TRUE -> confirmar sensibilidad del criterio de salida bajo full-runtime y diseñar D3-C2 con histéresis de salida más robusta.
```

D3-C1 es diagnóstico; su latencia no se usa para promoción porque los hooks añaden instrumentación.
