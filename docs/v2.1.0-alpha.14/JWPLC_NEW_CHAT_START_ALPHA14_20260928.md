# Inicio de chat — JWPLC Alpha14 H4 post-H3E

Continuar Alpha14 desde el estado actual de H4 post-H3E.

Leer primero, en este orden:

1. `docs/v2.1.0-alpha.14/JWPLC_ALPHA14_HANDOFF_20260928_H4A03.md`
2. `docs/v2.1.0-alpha.14/ALPHA14_STATUS.md`
3. `docs/v2.1.0-alpha.14/A14_POST_H3E_THRESHOLD_REQUALIFICATION_20260928.md`
4. `JWPLC_ASSISTANT_FAILURES_AND_PREVENTION_UPDATED_20260928_v51.md` si está disponible en el Proyecto.

Estado operativo:

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
H3E=CLOSED_PASS
R0=PASS_RETAINED
R1=PASS_RETAINED
PR99_MERGE_READINESS=PAUSED
POST_H3E_THRESHOLD_REQUALIFICATION=OPEN

H4A02_FAST_PATH_GAIN=+14.91 %
H4A02_FAST_MEDIAN=12.669195 Mbps
HISTORICAL_P3K_FAST=13.866349 Mbps
H4A02_CEILING_FINAL=NO
```

Siguiente gate:

```text
H4A0.3A = FAST minimal-instrumentation duration sweep
5 s x 3
15 s x 3
30 s x 3
```

Objetivo:

- medir throughput con la mínima intrusión posible;
- recuperar o explicar el ~13.87 Mbps histórico;
- NO hacer todavía component ablation;
- NO avanzar a H4A1 Modbus TCP-only;
- NO productizar el fast path todavía.

Después de H4A0.3A, si corresponde:

```text
H4A0.3B Component Ablation
A LEGACY
B +BATCH2
C +INT
D +FUSED
E +COMMIT2
F +R1
```

Trabajar un gate por vez.

Flujo obligatorio:

```text
PATCH
-> RE-READ
-> SYNTAX/PARSE
-> SOURCE CONTRACT
-> COMMIT
-> comando único al usuario
-> interpretar resultado
```

Separar siempre:

```text
HARNESS_FAILURE
PRODUCT_FAILURE
HARDWARE_FAILURE
```

No asumir que un resultado histórico sigue representando el runtime final.
