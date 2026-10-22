# Alpha13 G9 — Checklist final cerrado

**Fecha:** 2026-10-12. **Estado:** `G9=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`.
Alpha13 permanece `IN_PROGRESS`.

| Puerta | Evidencia | Estado |
|---|---|---|
| G9-P0 baseline | Run `g9_p0_20261011_234245_6727f0b0` (Git, NUL, hashes) | PASS |
| G9-P1 documental / A13-017 | README y comentarios, APIs intactas | PASS |
| G9-P2 candidato SD | NUL reemplazado por escape, fuente→objeto→archive | PASS |
| G9-P3 GlobalPeripherals | Flag RTC invariable removido; throttle 25 ms | PASS |
| P2/P3 candidato compilado | `g9_p2p3_20261012_000449_23842b4f`, 4/4 | PASS |
| Adopción productiva con respaldo | `g9_adopt_20261012_101458_1e395a6e`, 4/4 | PASS |
| Git controlado | `g9_git_20261012_102731_5f38ca7a` | PASS |
| Commit producto exacto | [`63e0291a`](https://github.com/JW-Control/platform-jwplc/commit/63e0291a7b1480af846e955559ab175aa78e7300), tres archivos | PASS |
| CI contrato P2/P3 | [38205406022](https://github.com/JW-Control/platform-jwplc/actions/runs/38205406022), Win/Linux | PASS |
| CI adopción/Git | [38209971080](https://github.com/JW-Control/platform-jwplc/actions/runs/38209971080), [38270082029](https://github.com/JW-Control/platform-jwplc/actions/runs/38270082029) | PASS |
| CI del smoke P5 | [38274903580](https://github.com/JW-Control/platform-jwplc/actions/runs/38274903580), Win/Linux | PASS |
| G9-P5 prueba real COM4 | `g9_p5_20261012_112751_200b0593`, microSD raíz, RTC y UP | PASS |
| Post-smoke | Tres heartbeats y auditoría final | PASS |
| COM14 | No tocado | PASS |
| Reposición programa de aplicación COM4 | No restaurado automáticamente | PENDIENTE OPERATIVO |
| F120 freeze post-upload | Causa no determinada | PENDIENTE SEPARADO |
| Alpha13 G10 | Auditoría, CI, fingerprint de archives y métricas | PENDIENTE |
| PR/merge/release | No autorizado | NO EJECUTADO |

## Límites

El PASS físico **no cubre** escritura de archivos SD, lectura
de contenido, hotplug, estrés, validación de cada tecla,
ajuste RTC ni horas de actividad. Solo se observó apertura
del directorio raíz `/` con `FILE_READ`, snapshot RTC válido,
un evento PRESS/RELEASE de UP y tres heartbeats.

Las pruebas del package no retiran periféricos de autoload
ni cambian APIs, OTA, configuración de flash o bootloader.
COM4 y COM14 todavía contienen firmwares de diagnóstico
según última evidencia. Antes de usarse con maquinaria se
deben reprogramar las aplicaciones pertinentes.

```text
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
A13-012=CLOSED_PASS
A13-014=CLOSED_PASS
A13-017=CLOSED_PASS
ALPHA13=IN_PROGRESS
NEXT_GATE=G10-P0
G10_A13-015=OPEN
G10_A13-016=OPEN
G10_A13-018=OPEN
G10_G5_G6_CANONICAL_RECONCILIATION=OPEN
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_PR_MERGE=NO
```

**Trazabilidad:** [cierre físico](A13_G9_P5_PHYSICAL_CLOSURE_20261012.md),
[cierre Git](A13_G9_GIT_PRODUCT_CLOSURE_20261012.md),
[estado vivo](ALPHA13_STATUS.md).
