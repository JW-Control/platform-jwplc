# v2.1.0-alpha.17 — JWPLC Sim: estado vivo

Actualizado: `2026-11-02` (apertura de la línea)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea (`…/main`) | `v2.1.0-alpha.17/JWPLC-Sim/main` | `d007b44` (= `release/v2.1.x` = `main`, 2.1.0-alpha.16 + índices) |
| platform-jwplc | Apertura | `v2.1.0-alpha.17/JWPLC-Sim/feature/apertura-linea` | plan, estado, workflows |
| platform-jwplc | HMI Designer | `v2.1.0-alpha.17/JWPLC-Sim/feature/hmi-designer` | Selector tipográfico táctil unificado (Opción A), resolutores y tokens CSS |
| JW-Libraries | Sincronización de `JW_*` | `PLATFORM_BASE_BRANCH` → esta línea | ver H1 |
| jwplc-sim | Simulador | PR #15 (crash de Windows, Ctrl+T, interfaz) | fijado en `sim-regression`: `fd08f12` |

## 2. Qué entró en `…/main`

Nada todavía.

## 3. Librerías precompiladas y en fuente

Al abrir: precompiladas `JWPLC_TFT`, `JWPLC_Display`, `JWPLC_Ethernet`, `JWPLC_ModbusTCP`, `JW_FRAM`,
`JW_MatrixButtons`, `JW_SD`. Las que pasen a fuente en esta línea se anotan aquí.

## 4. Workflows

- `ci-jwplc-package-smoke.yml` corre también en las PR hacia `v2.1.0-alpha.*/*/main`.
- `SIM_REGRESSION` una sola vez por árbol: cuando pasa en el CI, el job `sim-marker` sube el artefacto
  `sim-regression-pass-<árbol git>-<jwplc-sim>`; el CI siguiente y `release-jwplc-package.yml` lo buscan
  (`tools/ci/sim_regression_marker.sh`) y, si existe, no repiten el barrido. En alpha.16 el mismo árbol
  (`53f991c`) se barrió dos veces: CI del PR de cierre (41 min) y Release (30 min).
- El commit de jwplc-sim está en tres sitios por workflow: `uses:`, `sim_ref` y `JWSIM_REF` (iguales).

## 5. Para retomar

- Plan y hitos: [ALPHA17_PLAN.md](ALPHA17_PLAN.md).
- Siguiente: H1 (JW-Libraries) y H2/H3 por librería.

`NEXT_GATE=H1_SYNC_JW_LIBRARIES`
