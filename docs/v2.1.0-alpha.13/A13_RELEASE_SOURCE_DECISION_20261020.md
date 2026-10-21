## AVISO VIGENTE — Bloqueado hasta promover F137 (21/10/2026)

**Este aviso reemplaza la condición histórica `READY_FOR_OPERATOR_RELEASE_DISPATCH` del 20/10/2026. NO ejecutar el workflow Release JWPLC Arduino Package aún.** H-F137 de los archives Ethernet/Modbus TCP concluyó `PASS_FUNCIONAL_ACOTADO`, y F137-C verificó el package candidato exacto en [CI #39628750096](https://github.com/JW-Control/platform-jwplc/actions/runs/39628750096) Windows/Ubuntu, pero el código sigue únicamente en [PR #125 Draft](https://github.com/JW-Control/platform-jwplc/pull/125). La base Alpha13 **carece de los dos archives**, la política de perfil weak/strong y `precompiled=full` hasta que se autorice la integración.

**Huella obligatoria de package para futura publicación:** `JWPLC/2.1.0` Git tree SHA-1 `cc17716f173509b1e0f3c0df5eb99549014c1d61`. Comprobar desde la base ya integrada con el guard F137-C (en el PR candidato): `--verify-release-source --expected-head <SHA_REAL_BASE>`. Si el árbol no coincide, `REVIEW_STOP`; no disparar release ni alegar que las pruebas previas cubren otra fuente.

Después de integración revisada por operador, se mantiene la decisión sobre el workflow: seleccionar **rama base Alpha13** con `version=2.1.0-alpha.13`, nunca mezclar ciegamente `release/v2.1.x`; comprobar source SHA, README, PRE_RELEASE, ZIP raíz, checksum, índice dev y post-install. La autorización de merge y release son decisiones separadas. No cambiar el canal estable.

```text
F137_C_CI=PASS_NONPHYSICAL
RELEASE_SOURCE_TREE_REQUIRED=cc17716f173509b1e0f3c0df5eb99549014c1d61
RELEASE_DISPATCH=BLOCKED_UNTIL_PR125_APPROVED_AND_PROMOTED
RELEASE_SOURCE_BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
ALPHA13=CANDIDATE_NOT_PUBLISHED
```

---

# Alpha13 — Decisión de origen de PreRelease sin integrar 1095 commits ajenos

**20/10/2026. Estado: READY_FOR_OPERATOR_RELEASE_DISPATCH, no publicación ejecutada.**

El package Ethernet/Modbus TCP G10 fue integrado en `v2.1.0-alpha.13/feature/cleanup-robustness` mediante PR #124, merge `3467a375ccaf7c7ee27220ed0bc348056e5e8219`. Entre el código flasheado `7c1974c659be4aa88c877a849db6efc32673da60` y el último HEAD de aceptación del candidato, `JWPLC/` y ambos sketches de banco no cambiaron. N0/N1/N2/N3 previa sobre estos inputs no debe repetirse sin deriva.

## Riesgo de mezcla entre ramas

La rama estándar indicada en `JWPLC/tools/README_RELEASE_AUTOMATION.md` es `release/v2.1.x`. La consulta Git el 20/10 dio `release/v2.1.x...` contra base Alpha13: **1095 commits de atraso, con otras rutas no productivas y 35 cambios en `JWPLC/`**. El merge directo de Alpha13 a release llevaría el historial completo del alpha, no solo el package que se desea publicar. **No hacer ese merge ciego**.

## Ruta segura compatible con el workflow actual

La propia versión de `.github/workflows/release-jwplc-package.yml` en la base Alpha13:

- Acepta `workflow_dispatch` con input **`version`**; el checkout estándar usa el SHA de la rama elegida en Actions.
- Deriva `SOURCE_FOLDER=JWPLC/2.1.0` para `2.1.0-alpha.13`.
- Exige `boards.txt`, `platform.txt` y `docs/v2.1.0-alpha.13/PRE_RELEASE.md`, presentes en la base.
- Siempre regenera ZIP con raíz `2.1.0/`, verifica estructura, checksum y tamaño; deriva canal **dev** y no modifica índice estable.
- Sembrará índices desde `origin/main`; abrirá PR automático del índice dev hacia `main`; copia el **README del SHA del workflow** a ese PR. Por eso el README de la base Alpha13 se preparó con marker `JWPLC_RELEASE_VERSION: 2.1.0-alpha.13` y texto que no anticipa publicación exitosa.
- Rechaza un tag/release preexistente y crea PreRelease y asset apuntando a **`GITHUB_SHA`** de la rama elegida.
- Consulta read-only de `releases/tags/v2.1.0-alpha.13` dio **404 el 20/10**; la verificación definitiva de tag/release la hace el workflow antes de publicar.

**Por tanto, seleccionar de forma deliberada la rama `v2.1.0-alpha.13/feature/cleanup-robustness` en el menú `Run workflow`, y `version=2.1.0-alpha.13`**. Esto publica el source real integrado sin mezclar las 1095 diferencias de `release/v2.1.x`. No es la ruta normal de esa documentación histórica: requiere que el operador confirme visualmente la rama seleccionada y el HEAD/ZIP de Actions.

## Cierre postpublicación obligatorio

Después del workflow verificar `GITHUB_SHA` / source package, tag/asset y checksum, el PR automático del índice dev hacia `main`, revisión README, e instalación/compilación desde Boards Manager **oficial** en entorno aislado; upload físico desde package publicado si está disponible sin interferir con las pruebas actuales. Guardar evidencia real en `PRE_RELEASE.md` y `ALPHA13_STATUS.md`.

```text
RELEASE_SOURCE_BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
WORKFLOW=release-jwplc-package.yml
WORKFLOW_VERSION_INPUT=2.1.0-alpha.13
RELEASE_V2_1_X_MERGE=NOT_RUN_INTENTIONALLY
PACKAGE_ROOT=JWPLC/2.1.0
README_RELEASE_MARKER=2.1.0-alpha.13
RELEASE_TAG_PRECHECK=NO_EXISTING_GITHUB_RELEASE_404
GITHUB_PRERELEASE=NOT_RUN
INDEX_DEV_PR=NOT_CREATED
POST_PUBLISHED_INSTALL=NOT_RUN
ALPHA13_STATUS=OPEN_RELEASE_PENDING
```
