# Alpha13 G8 — Checklist final de cierre

**Fecha:** 2026-10-11. **Estado:** `G8=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`.

| Punto de aceptación | Evidencia | Estado |
|---|---|---|
| P0 baseline read-only | `g8_p0_20261011_185223_e0c4e0cc` | PASS |
| P1/P2 fixtures C++ nativos | 20 casos; post-commit [CI 38191980973](https://github.com/JW-Control/platform-jwplc/actions/runs/38191980973) | PASS |
| Compilación fuente RTU y archive precompilado | Paridad objeto-miembro, SHA archive y linker exacto | PASS |
| Adopción local P3 | `g8_adopt_20261011_193839_57f9bb49`, 3 regresiones | PASS |
| Git producto P3 | [`fb71d65e`](https://github.com/JW-Control/platform-jwplc/commit/fb71d65eb2c842b3f46e6949760e693f3b73d95e), solo 3 archivos (2 fuentes y .a) | PASS |
| CI de gate P4 Windows/Linux | [38194119661](https://github.com/JW-Control/platform-jwplc/actions/runs/38194119661) | PASS |
| P4 preflight y builds productivos | COM4/COM14 CH340, dos compilaciones y archive exacto | PASS |
| Consentimiento de upload | Frase explícita aceptada en el BAT | PASS |
| P4 físico Slave COM14 | 12 solicitudes FC03, RX 96, CRC 0, READY 1 | PASS |
| P4 físico Master COM4 | 12/12 lecturas correctas, RX 156, CRC 0, timeout 0, READY 1 | PASS |
| P4 auditoría final | `FINAL_AUDIT_PASS` y Git/producto sin modificaciones | PASS |
| Incidentes previos F122–F124 | Corregidos, contrastados en Windows y CI; [FAILURES](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md) | PASS |
| Registro de límites físicos | Solo FC03 a 115200 8N1, ruta no-bulk | DOCUMENTADO |
| Estado firmware de placas | Sketches G8 cargados en ambas; restauración previa NO automática | PENDIENTE OPERATIVO |
| F120 freeze postupload | Causa no demostrada en este gate | PENDIENTE SEPARADO |
| Publicación Alpha13 | No autorizado por G8 | NO EJECUTADO |

## Decisión

```text
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
A13-010=CLOSED_PASS
A13-011=CLOSED_PASS_WITH_NATIVE_NEGATIVE_UART
ALPHA13=IN_PROGRESS
NO_EXTRA_FW_CHANGES=YES
NO_RELEASE_MERGE_PR=YES
```

**Evidencia física canónica:**
[A13_G8_P4_PHYSICAL_CLOSURE_20261011.md](A13_G8_P4_PHYSICAL_CLOSURE_20261011.md).

La señal positiva de UART corresponde a drivers efectivamente
iniciados bajo la prueba. La negativa (UART fallida) fue probada
en fixtures, no provocada en hardware real. No extender el PASS
a variantes no ensayadas, a F120, al resto de Alpha13 o al firmware
que había instalado antes de las dos cargas de diagnóstico.
