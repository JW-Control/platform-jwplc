# v2.1.0-alpha.16 — JWPLC Sim: plan

Línea `JWPLC-Sim`: lo que el simulador JWPLC Sim (`JW-Control/jwplc-sim`) necesita
del package, del VPP y del editor OpenPLC. Abierta el 2026-10-28, después de
publicar 2.1.0-alpha.15. Número reservado en `RAMAS_Y_TAGS.md`.

```text
LINEA=JWPLC-Sim
RAMA=v2.1.0-alpha.16/JWPLC-Sim/main (desde release/v2.1.x @ a6c239b1 = tag v2.1.0-alpha.15 + índices)
PUBLICA_COMO=2.1.0-alpha.16 (si se mantiene el orden de publicación)
EDITOR=misma rama en JW-Control/openplc-editor, desde v2.1.0-alpha.15/OpenPLC-HMI/main (4a61c0b),
       al empezar H3 (el primer hito que toca el editor)
```

## Qué ya está en `release/v2.1.x` (no es trabajo de esta línea)

- Gate `SIM_REGRESSION` en `release-jwplc-package.yml` (la release lo necesita) y en el CI de PR,
  con `jwplc-sim` fijado a `19a1a40d`. Entró con la republicación de 2.1.0-alpha.15 y pasó en la
  publicación final ([run 40599208706](https://github.com/JW-Control/platform-jwplc/actions/runs/40599208706)).

## Hitos

Gates del simulador: `docs/GATES.md` de `jwplc-sim` (G6).

| Hito | Resultado | Criterio de cierre | Repos |
|---|---|---|---|
| H1 OpenPLC en el simulador | Un proyecto OpenPLC vinculado compila con la cadena real (matiec → C + HAL `jwplcbasic.cpp` del VPP) contra el backend simulado y corre con la misma placa | `JWSIM_G6_LADDER_CHAIN=PASS` con un proyecto de ejemplo | jwplc-sim; aquí solo si el VPP necesita un punto de extensión |
| H2 Recarga desde el editor | Guardar en el editor recarga la simulación | `JWSIM_G6_RELOAD_FROM_EDITOR=PASS` (E2E) | jwplc-sim |
| H3 Botón «Simular» | El editor abre el proyecto en JWPLC Sim | `JWSIM_G6_SIMULATE_BUTTON=PASS`; PR en `openplc-editor` | openplc-editor (+ SHA del VPP si cambia) |
| H4 Cierre | `jwplc-sim` fijado al commit con G6 en `sim-regression` | Release con `SIM_REGRESSION` en verde | platform-jwplc |

Reglas del package que valen aquí: el VPP no se parchea desde el simulador; un
cambio que el simulador necesite en el VPP o en el package entra por una feature
de esta línea, con su PR hacia `…/main`.
