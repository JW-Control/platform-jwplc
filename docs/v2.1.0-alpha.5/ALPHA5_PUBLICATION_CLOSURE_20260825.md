# JWPLC v2.1.0-alpha.5 — Cierre de publicación

Fecha: 2026-08-25

## Propósito

Registrar el estado final de publicación de `v2.1.0-alpha.5` después del
cierre técnico, merge, generación automática del package, publicación del
PreRelease e instalación real desde Boards Manager.

Este documento complementa y actualiza el estado de publicación que figuraba
como pendiente en `ALPHA5_CLOSURE_CHECKLIST.md` al momento de preparar el PR.

---

## 1. Integración

- [x] PR técnico Alpha5 integrado a `release/v2.1.x`.
- [x] CI del package aprobado.
- [x] README actualizado a Alpha5.
- [x] Automatización de publicación desde el marcador del README implementada.
- [x] Workflow de release corregido para aislar la rama de índices mediante
  worktree temporal.
- [x] Reintento automático de una publicación pendiente implementado.

Resultado:

`ALPHA5_RELEASE_INTEGRATION=PASS`

---

## 2. GitHub PreRelease y artefacto

GitHub PreRelease publicado:

`v2.1.0-alpha.5 - JWPLC Arduino package`

Estado:

- draft: no;
- prerelease: sí;
- tag: `v2.1.0-alpha.5`;
- commit objetivo publicado: `5e11ad65ff1de2a1d38621ac1dc5903d15e00527`.

Asset:

`jwplc-esp32-2.1.0-alpha.5.zip`

Tamaño:

`24552607 bytes`

SHA-256:

`e24505d49852c075378f786700c54308ce20ed77cac959a52e9d8072ccde16ae`

Resultado:

`ALPHA5_GITHUB_PRERELEASE=PASS`

`ALPHA5_RELEASE_ASSET=PASS`

---

## 3. Índice de Boards Manager

El workflow de release generó la rama automática de índice y el PR
correspondiente hacia `main`.

PR de índice:

`#67 ci(release): update JWPLC package indexes for v2.1.0-alpha.5`

Resultado:

- [x] `package_jwplc_index_dev.json` actualizado a Alpha5.
- [x] `README.md` de `main` actualizado a Alpha5.
- [x] El índice estable `package_jwplc_index.json` no fue modificado.
- [x] PR #67 integrado a `main`.

Resultado:

`ALPHA5_DEV_INDEX=PASS`

`ALPHA5_STABLE_INDEX_UNCHANGED=PASS`

---

## 4. Instalación pública desde Boards Manager

Se instaló desde Arduino IDE el package público:

`jwplc:esp32@2.1.0-alpha.5`

Arduino IDE reemplazó correctamente:

`jwplc:esp32@2.1.0-alpha.4`

por:

`jwplc:esp32@2.1.0-alpha.5`

La salida del IDE mostró el namespace público `jwplc`, no `jwplc_local`.

Resultado:

`ALPHA5_PUBLIC_BOARDS_MANAGER_INSTALL=PASS`

---

## 5. Compilación desde instalación publicada

Se validó desde Arduino IDE 2.3.10 con board `JWPLC Basic`.

### Sketch vacío

- primera compilación observada: 76.79 s;
- recompilación sin cambios: 11.18 s;
- APP: 394317 bytes;
- RAM global: 27596 bytes.

### Gate integral

Sketch:

`tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate`

Tiempo observado:

`56.68 s`

El gate integral compiló correctamente desde el package público Alpha5.

Resultado:

`ALPHA5_PUBLIC_ARDUINO_IDE_COMPILE=PASS`

`ALPHA5_PUBLISHED_PACKAGE_INTEGRAL_COMPILE=PASS`

---

## 6. Experiencia incremental de desarrollo

Se realizaron dos series manuales adicionales en Arduino IDE para medir el
flujo real de edición y recompilación.

### Cambios pequeños/medianos

Después de la primera compilación, seis incrementos sucesivos del mismo sketch
promediaron:

`13.52 s`

### Reemplazos completos por sketches mayores

Se reemplazó completamente el mismo `.ino` por sketches de 88, 240, 630 y
1014 líneas.

Resultados:

| Sketch | Tiempo |
|---:|---:|
| 88 líneas | 67.52 s |
| 240 líneas | 19.00 s |
| 630 líneas | 19.71 s |
| 1014 líneas | 20.35 s |
| 1014 líneas sin cambios | 8.50 s |

Promedio de los tres cambios grandes posteriores a la primera compilación:

`19.69 s`

Documento detallado:

`docs/v2.1.0-alpha.5/ARDUINO_IDE_DEVELOPER_ITERATION_VALIDATION_20260825.md`

Resultado:

`ALPHA5_DEVELOPER_ITERATION_EXPERIENCE=PASS`

---

## 7. Gates físicos no ejecutados en esta sesión

No había hardware JWPLC Basic conectado durante la validación final del
package publicado.

Por ello permanecen explícitamente sin ejecutar en esta sesión:

- upload desde instalación publicada;
- repetición física integral del gate local.

Estado:

`ALPHA5_PUBLISHED_UPLOAD=NOT_EXECUTED_NO_HARDWARE`

`ALPHA5_FINAL_PHYSICAL_RERUN=NOT_EXECUTED_NO_HARDWARE`

Esto no se clasifica como FAIL porque Alpha5 ya cuenta con gates físicos
dirigidos sobre las áreas funcionalmente modificadas y la publicación final
fue instalada y compilada correctamente desde Boards Manager.

La repetición física integral queda recomendada cuando exista hardware
disponible, sin reabrir el alcance técnico de Alpha5 salvo que aparezca una
regresión real.

---

## 8. Estado final

Se considera cerrada `v2.1.0-alpha.5` para su alcance definido.

Se cuenta con:

- precompilación compatible auditada;
- core normalizado;
- todos los periféricos del autoload preservados;
- benchmark formal final;
- validación en segundo equipo;
- CI automático del package;
- release automático funcional;
- GitHub PreRelease publicado;
- ZIP y SHA-256 registrados;
- índice dev integrado a `main`;
- instalación real por Boards Manager;
- compilación real desde package publicado;
- gate integral compilado desde package publicado;
- experiencia incremental de Arduino IDE validada.

Marcadores finales:

`ALPHA5_TECHNICAL_CLOSURE=PASS`

`ALPHA5_PUBLICATION=PASS`

`ALPHA5_BOARDS_MANAGER_VALIDATION=PASS`

`ALPHA5_DEVELOPER_ITERATION_EXPERIENCE=PASS`

`ALPHA5_STATUS=CLOSED`

Los commits documentales posteriores al tag sólo registran evidencia de
validación posterior a la publicación y no modifican el contenido del package
`v2.1.0-alpha.5` ya publicado.