## ESTADO VIGENTE — F137 integrado; PreRelease aún NO autorizada (21/10/2026)

**Cambio de estado:** el bloqueo `BLOCKED_UNTIL_PR125_APPROVED_AND_PROMOTED` quedó superado. El propietario autorizó y se integró [PR #125](https://github.com/JW-Control/platform-jwplc/pull/125) por squash en la **base Alpha13**, commit `23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`. El package real de `JWPLC/2.1.0` mantiene tree SHA-1 `cc17716f173509b1e0f3c0df5eb99549014c1d61` idéntico al source de N3; política weak/strong Basic/Core, archivos `.a` canónicos y `precompiled=full` están realmente en base. El conocimiento F137 [PR privado #27](https://github.com/JW-Control/jwplc-dev-mcp/pull/27) quedó integrado en `knowledge/main`.

**NO ejecutar todavía el workflow Release.** Aunque F137-H y F137-C ya son PASS, la auditoría global Alpha13, el gate de fuente release-like, la confirmación separada de despacho y el post-publicación del ZIP/Boards Manager siguen pendientes. Último preflight de tag/Release `v2.1.0-alpha.13`: ambos ausentes (404). Si el árbol del package de base cambia, revisar impacto y no reutilizar la evidencia N3/H como PASS de otro producto.

**Origen reservado cuando se autorice:** seleccionar deliberadamente `v2.1.0-alpha.13/feature/cleanup-robustness` y `version=2.1.0-alpha.13`; NO fusionar a ciegas `release/v2.1.x`. En el despacho registrar `GITHUB_SHA`, raíz ZIP `2.1.0/`, checksum, índice dev, README y PRE_RELEASE exactos. La autorización del merge de PR125/27 **no** incluye autorización de publicar.

```text
F137_IN_ALPHA13_BASE=YES
F137_PACKAGE_TREE=cc17716f173509b1e0f3c0df5eb99549014c1d61
F137_PROMOTION_COMMIT=23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11
BASE_SYNC=OK
KNOWLEDGE_SYNC=OK
RELEASE_SOURCE_SHA=TO_CONFIRM_AT_DISPATCH
RELEASE_DISPATCH=NOT_AUTHORIZED_PENDING_ALPHA13_GLOBAL_AUDIT
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
