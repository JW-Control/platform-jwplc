# Estado · alpha OpenPLC + HMI (trabajo ALPHA14 · publica 2.1.0-alpha.15)

Estado vivo de la línea OpenPLC ↔ HMI Designer. No confundir con `ALPHA14_STATUS.md` (alpha.14 de robustez
Ethernet, cerrada en 2026-09). Notas de la Release: `docs/v2.1.0-alpha.15/PRE_RELEASE.md`. Contrato del puente:
`A14_F3_CONTRATO_HMI_OPENPLC_20261017.md` §7–§10. Hoja de ruta original: `A14_HOJA_DE_RUTA_ALPHA15_OPENPLC_HMI_20261024.md`.

```text
RAMA_PRINCIPAL=v2.1.0-alpha.15/OpenPLC-HMI/main (platform-jwplc y openplc-editor)
BASE_PLATFORM=release/v2.1.x @ f3f47ae (2.1.0-alpha.14 publicada)
BASE_EDITOR=develop/alpha14-openplc-hmi-v1 @ c0bf9ac07 + 4.2.8-jwplc.3
F0_BOOL=PASS_PHYSICAL
F0b_VALORES=PASS_PHYSICAL
F0c_ESCRITURA=PASS_PHYSICAL
F0d_COSTO_HMI=PASS_PHYSICAL (alpha.32)
F0d_JITTER_TFT=PENDING (package: redibujo de página en jwplcSystemTask) -> siguiente alpha
F3_PUENTE_1_1=PASS [ejecutado]
F3_VARIABLES_SIN_DIRECCION_1_2=PASS [ejecutado]
F3_VENTANA_Y_VISTA_DE_ENLACES_1_4=PASS [ejecutado] (Electron real 14/14 + tests del Designer)
VPP_2_1_0_ALPHA_15=PASS (firma jwcontrol-2026 3b46176, verificada con el verificador del editor)
INSTALADOR_EDITOR=4.2.8-jwplc.8 (= jwplc.7 con nombres de descarga cortos; tag jwplc-editor-v4.2.8-jwplc.8 @ 4a61c0b)
F3_CADENA_COMPLETA_EN_APP_Y_PLACA=PASS_PHYSICAL (propietario, 27/10/2026)
CIERRE=IN_PROGRESS (feature/cierre-alpha15 -> …/main -> PR a release/v2.1.x con autorización)
```

## Matriz de versiones (lo que se publica)

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` | 2.1.0-alpha.15 (librerías iguales a 2.1.0-alpha.14; + curso y plantilla como ejemplos) | platform-jwplc `…/main` |
| HMI Designer | 2.1.0-alpha.15 | platform-jwplc `fbb61e6` (fijado en `PLATFORM_DESIGNER_SHA` del editor) |
| VPP JWPLC Basic | 2.1.0-alpha.15 | platform-jwplc `a8458dc` (fijado en `PLATFORM_VPP_SHA`) |
| OpenPLC Editor – JWPLC Edition | 4.2.8-jwplc.8 | openplc-editor `4a61c0b` |

Los dos SHAs fijados son alcanzables desde `…/main` (entraron con merge commits).

## Registro de merges en `…/main`

| Fecha | Repo | Qué entró | Merge en `…/main` |
|---|---|---|---|
| 24/10 | platform-jwplc | historia de `v2.1.0-alpha.14/HmiDesigner/main` (árbol = release) | `83abf3c` |
| 24/10 | platform-jwplc | `v2.1.0-alpha.14-OpenPLC-HMI` @ `20ef597` (Kenyi) | `7476cd6` |
| 24/10 | openplc-editor | `claude/awesome-goldberg-7sj2vx` @ `df6b74d6c` (4.2.8-jwplc.3) | `adde17d74` |
| 27/10 | platform-jwplc | PR #175 `feature/apertura-rama-principal` @ `aca0080` | `cab2d47` |
| 27/10 | platform-jwplc | PR #176 `feature/designer-alpha14-en-openplc` @ `fbb61e6` | `ceef692` |
| 27/10 | platform-jwplc | PR #177 `feature/vpp-version-alpha15` @ `a8458dc` | `2adce24` |
| 27/10 | platform-jwplc | PR #179 `feature/readme-descargas` @ `e96106a` | `cb5eebc` |
| 27/10 | platform-jwplc | PR #180 `feature/flujo-de-trabajo` @ `bb5a17f` (conflicto en `CLAUDE.md`: queda la versión sin estado) | `919f346` |
| 27/10 | platform-jwplc | PR #181 `feature/cierre-alpha15` @ `858bb32` | `7fc9771` |
| 27/10 | platform-jwplc | PR #183 `feature/curso-jwplc-basic` @ `64d0937` (curso, unidades 1 y 2) | `9f7a055` |
| 27/10 | platform-jwplc | PR #184 `feature/plantilla-proyecto` @ `94659ed` (plantilla de proyecto) | `e7bb3f9` |
| 27/10 | openplc-editor | PR #7 `feature/designer-alpha14-en-openplc` @ `9a88922` | `da89ca9d3` |
| 27/10 | openplc-editor | PR #8 `feature/flujo-de-trabajo` @ `41ca36daa` | `8fb704a66` |
| 27/10 | openplc-editor | PR #9 `feature/nombres-cortos` (4.2.8-jwplc.8) | `4a61c0b66` |

PR #178 (`sync/jw-libraries-v2.1`, snapshot de JW-Libraries con JW_DWIN) queda fuera de esta alpha.

## Ramas y pre-releases a retirar (último SHA)

Todas están contenidas en `…/main` de su repo. Se borran con OK del propietario (27/10/2026).

| Repo | Rama | SHA |
|---|---|---|
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/apertura-rama-principal` | `aca0080` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc` | `fbb61e6` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/vpp-version-alpha15` | `a8458dc` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/readme-descargas` | `e96106a` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/flujo-de-trabajo` | `bb5a17f` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/cierre-alpha15` | `858bb32` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/curso-jwplc-basic` | `64d0937` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/plantilla-proyecto` | `94659ed` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/cierre-curso-plantilla` | `2e180b4` |
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/feature/nombres-cortos` | (esta feature) |
| openplc-editor | `v2.1.0-alpha.15/OpenPLC-HMI/feature/nombres-cortos` | `4a61c0b66` (merge) |
| platform-jwplc | `v2.1.0-alpha.14-OpenPLC-HMI` (Kenyi) | `20ef597` |
| platform-jwplc | `v2.1.0-alpha.14/HmiDesigner/main` | `7e7a141` |
| openplc-editor | `v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc` | `9a88922b8` |
| openplc-editor | `v2.1.0-alpha.15/OpenPLC-HMI/feature/flujo-de-trabajo` | `41ca36daa` |
| openplc-editor | `claude/awesome-goldberg-7sj2vx` | `df6b74d6c` |

Pre-releases de prueba del editor que se borran: `jwplc-editor-v4.2.8-jwplc.4`, `.5` y `.6` (quedan la `.7` y la
`hmi-designer-v2.1.0-alpha.15` de platform-jwplc). `claude/jwplc-basic-simulator-8dgcen` es de otra línea
(simulador): no se toca.

## Decisiones de la línea

- El editor no tiene `release/v2.1.x`: su `v2.1.0-alpha.15/OpenPLC-HMI/main` queda como línea JWPLC Edition y los
  instaladores salen de ella por tag `jwplc-editor-v<versión>` (workflow `jwplc-edition-prerelease.yml`).
- El Designer va empaquetado en el instalador del editor, no en el VPP.
- En OpenPLC: Designer completo en ventana + vista de enlaces en la pestaña (1.4), tras probar la pestaña completa
  (1.3) y descartarla.

## Para retomar

1. Cierre: fusionar `feature/cierre-alpha15` en `…/main` y abrir el PR `…/main` → `release/v2.1.x`; sólo se fusiona
   con autorización explícita (publica `v2.1.0-alpha.15` con los instaladores del Designer adjuntos).
2. Después de publicar: sincronizar `main`, borrar las ramas de la tabla de arriba y `…/main`.
3. Siguiente alpha: modo claro del Designer sincronizado con OpenPLC, traer el OpenPLC original v4.3.x
   (STruC++ 0.7, reescritura de Baremetal: 65 archivos en conflicto en la fusión de prueba), JW-Libraries (#178),
   `F0d_JITTER_TFT`, TEXT enlazable.
