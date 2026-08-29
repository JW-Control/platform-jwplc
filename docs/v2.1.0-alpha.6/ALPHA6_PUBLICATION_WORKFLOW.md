# v2.1.0-alpha.6 — Flujo automatizado de publicación

## Objetivo

Publicar Alpha6 usando los workflows existentes del repositorio y evitar pasos manuales que ya están automatizados.

La línea publicable es la integración corregida:

```text
v2.1.0-alpha.6/integration/rebase-alpha5-final
```

basada en:

```text
release/v2.1.x @ 64068556
```

El PR de la línea Alpha6 antigua no debe mergearse sin reconciliarse con esta rama corregida.

## Flujo esperado

```text
Alpha6 corregida
    -> PR a release/v2.1.x
    -> CI
    -> merge
    -> push de README.md con JWPLC_RELEASE_VERSION=2.1.0-alpha.6
    -> Auto Release JWPLC from README
    -> Release JWPLC Arduino Package
    -> ZIP + SHA-256 + tamaño
    -> GitHub PreRelease
    -> actualización package_jwplc_index_dev.json
    -> rama ci/release-index-v2.1.0-alpha.6
    -> PR automático a main
    -> merge PR de índices
    -> validación Boards Manager
```

## Gate previo al PR/merge

La publicación sólo continúa con estos marcadores cerrados:

```text
ALPHA6_BASE_CORRECTION=COMPLETE
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
```

## Trigger automático

El workflow:

```text
.github/workflows/auto-release-jwplc-on-readme.yml
```

escucha pushes a:

```text
release/v2.1.x
```

cuando cambia `README.md` o los propios workflows.

Para Alpha6 el README debe contener:

```html
<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.6 -->
```

y debe existir:

```text
docs/v2.1.0-alpha.6/PRE_RELEASE.md
```

## Workflow de publicación

El auto workflow despacha:

```text
.github/workflows/release-jwplc-package.yml
```

Ese workflow:

- recrea el ZIP desde `JWPLC/2.1.0`;
- detecta canal alpha;
- calcula checksum y tamaño;
- crea `v2.1.0-alpha.6` como PreRelease;
- carga el ZIP;
- actualiza el índice dev;
- prepara rama de índices;
- abre PR hacia `main`;
- conserva el índice público estable salvo política explícita.

## Fallback manual

Si el auto workflow no se dispara, ejecutar manualmente `Release JWPLC Arduino Package` desde GitHub Actions usando:

```text
version = 2.1.0-alpha.6
channel = auto
source_folder = JWPLC/2.1.0
recreate_zip = true
archive_root_mode = folder
update_public_index = auto
replace_existing_index_entry = false
commit_index_changes = true
publish_release = true
fail_if_release_exists = true
overwrite_release_asset = false
release_notes_file = docs/v2.1.0-alpha.6/PRE_RELEASE.md
upload_workflow_artifact = true
open_index_pr = true
index_pr_base_branch = main
```

No ejecutar el fallback mientras exista una corrida automática válida en progreso.

## Validación posterior

Después de publicar:

- confirmar release `v2.1.0-alpha.6`;
- confirmar que es PreRelease;
- registrar ZIP, SHA-256 y tamaño;
- confirmar PR automático de índices;
- mergear ese PR cuando pase CI;
- comprobar `package_jwplc_index_dev.json` desde `main`;
- instalar Alpha6 en un entorno limpio mediante Boards Manager;
- compilar un sketch mínimo;
- subir al hardware;
- registrar el cierre de publicación.

No iniciar otro alpha antes de completar este cierre.
