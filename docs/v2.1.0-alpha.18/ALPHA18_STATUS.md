# v2.1.0-alpha.18 — JWPLC Sim: estado vivo

Actualizado: `2026-11-17` (apertura: alpha.17 publicada; editor con la integración del Sim y Modbus TCP)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea | `v2.1.0-alpha.18/JWPLC-Sim/main` | `29632e4` (= `release/v2.1.x`) |
| openplc-editor | Base de la línea | `v2.1.0-alpha.18/JWPLC-Sim/main` | `6ef47ef` (#19, #20); versión 4.3.3-jwplc.2 |
| openplc-editor | Designer / VPP empaquetados | `PLATFORM_DESIGNER_SHA` / `PLATFORM_VPP_SHA` | platform `3f29398` / `0647a84` (sin cambios desde jwplc.2) |
| jwplc-sim | Simulador | `main` | `c807af1`; las 6 `feature/alpha18-*` ya están dentro |

Pendiente del propietario (cierre de alpha.17): platform #244 y #243 hacia `main` (`main` sigue en `d007b44`) y
jwplc-sim #36 (`package.lock` → tag `v2.1.0-alpha.17`). Ver `docs/v2.1.0-alpha.17/ALPHA17_STATUS.md`.

## 2. Qué entró en `…/main`

| Repo | PR | Rama | Merge | Contenido |
|---|---|---|---|---|
| openplc-editor | #19 | `…/feature/integracion-openplc` | `f344a79` | botón Simular, `--open-project`, `--jwsim-close`, preload de respaldo; HMI: TREND ch2/ch3 y pisos de Baliza, Guardar sin reescribir |
| openplc-editor | #20 | `…/feature/modbus-tcp-enlace-tardio` | `6ef47ef` | firmware OpenPLC (`MBETH_JWPLC`): listener 502 con enlace tardío y ADU reensamblado (venía de `ccr-4e8cc1e7-lygr9t` @ `8669a86`) |

Validación de #19 + #20 sobre alpha.17 `[ejecutado]`: `tsc --noEmit`, Prettier, ESLint sin errores, Jest de
los módulos tocados 643/643 y `npm run build`. En Windows fallan `board-info-resolver`, `vendor-document/serve` y
`debug-map-path` por rutas/symlinks (no los toca la línea).

## 3. Gates

```text
A18_H0_APERTURA=IN_PROGRESS
A18_H1_EDITOR_SIM=IN_PROGRESS        # Simular → Sim instalado PASS (17/11); resto [no verificado]
A18_H2_HMI_ENLACES=IN_PROGRESS       # unit tests PASS; Designer en vivo [no verificado]
A18_H3_MODBUS_TCP=PENDING            # requiere JWPLC Basic físico
A18_H4_CIERRE=PENDING
```

## 4. Riesgos y deuda

- #19 y #20 se fusionaron con la prueba manual incompleta (decisión del 17/11): H1–H3 siguen abiertos.
- `--jwsim-close` cierra el editor aunque se haya abierto a mano: falta decisión de producto.
- Upstream 4.3.4 (30 commits) y 4.3.5 (237) sin integrar: alpha.19.
- La rama suelta `ccr-4e8cc1e7-lygr9t` del editor sigue en GitHub (su commit ya está en #20).

## 5. Para retomar

1. Fusionar este PR de apertura (OK del propietario).
2. Correr los checklists H1 y H2 de [ALPHA18_PLAN.md](ALPHA18_PLAN.md) con el editor de `…/main`; H3 cuando haya placa.
3. Cada arreglo que salga: feature `v2.1.0-alpha.18/JWPLC-Sim/feature/<tema>` con PR hacia `…/main`.
4. Cierre: editor 4.3.3-jwplc.3 + nota en `docs/jwplc/releases/`, `README` de platform y PR de cierre.

`NEXT_GATE=A18_H1_EDITOR_SIM`
