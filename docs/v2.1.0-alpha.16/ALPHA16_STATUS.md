# v2.1.0-alpha.16 — JWPLC Sim: estado vivo

Actualizado: `2026-10-28` (apertura de la línea)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea (`…/main`) | `v2.1.0-alpha.16/JWPLC-Sim/main` | `a6c239b1` (= `release/v2.1.x` = `main`) |
| platform-jwplc | Feature en curso | `v2.1.0-alpha.16/JWPLC-Sim/feature/apertura-linea` | apertura: plan, estado y `CLAUDE.md` |
| openplc-editor | Base de la línea | (se crea al empezar H3) | desde `v2.1.0-alpha.15/OpenPLC-HMI/main` @ `4a61c0b` |
| jwplc-sim | Simulador | `claude/jwplc-basic-simulator-8dgcen` | `package.lock` en `v2.1.0-alpha.15` |

Rama retirada: `claude/jwplc-basic-simulator-8dgcen` @ `b8906d83` (gate `SIM_REGRESSION`; su contenido entró a
`release/v2.1.x` como `e1c56337` con la republicación de 2.1.0-alpha.15).

`BASE_SYNC=OK` · `KNOWLEDGE_SYNC=PENDIENTE` (reserva de `2.1.0-alpha.16` en `RAMAS_Y_TAGS.md`)

## 2. Matriz de versiones

| Componente | Versión | SHA / tag | Dónde se fija |
|---|---|---|---|
| Package | 2.1.0-alpha.15 | `v2.1.0-alpha.15` (`cbeced0d`) | base de la línea |
| JWPLC Sim en la release | — | `19a1a40d` | `release-jwplc-package.yml` y `ci-jwplc-package-smoke.yml` (`uses` y `sim_ref`) |
| Editor | 4.2.8-jwplc.8 | `jwplc-editor-v4.2.8-jwplc.8` | base para H3 |

## 3. Qué funciona en la máquina

Nada nuevo todavía: la línea recién abre.

## 4. Qué existe aislado o detrás de un flag

`PENDIENTE_DE_PROMOCIÓN:` (vacío)

## 5. Evidencia vigente

- `SIM_REGRESSION` en la publicación de 2.1.0-alpha.15: PASS
  ([run 40599208706](https://github.com/JW-Control/platform-jwplc/actions/runs/40599208706), `jwplc-sim` `19a1a40d`).
- `jwplc-sim` con el tag `v2.1.0-alpha.15`: paridad PASS (0 diferencias), matriz en `docs/reports/SYNC_ALPHA15.md` de `jwplc-sim`.

## 6. Plan de hitos y NEXT

H1–H4 en [ALPHA16_PLAN.md](ALPHA16_PLAN.md).
`NEXT_GATE=JWSIM_G6_LADDER_CHAIN` · `NEXT_FAILURE_ID=` · `HARDWARE=BLOQUEADO`

## 7. Para retomar

- Esta feature abre la línea; su PR va hacia `v2.1.0-alpha.16/JWPLC-Sim/main` y espera el OK del propietario.
- El trabajo de G6 está en `jwplc-sim`; aquí entra solo lo que el VPP o el package necesiten.
- No crear la rama del editor hasta H3.
