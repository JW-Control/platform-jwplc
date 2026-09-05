# Automatización de release del package JWPLC

Esta carpeta contiene las herramientas usadas para generar y publicar el package Arduino JWPLC, calcular `SHA-256` y `size`, actualizar los índices del Boards Manager y crear el GitHub Release / PreRelease.

## Política de operación

El flujo normal de publicación queda deliberadamente simplificado: en GitHub Actions el único dato editable es la versión.

```text
version = 2.1.0-alpha.10
```

El resto de parámetros forman parte de la política fija del package y no deben depender de una selección manual en cada release.

```text
channel=auto
source_folder=JWPLC/<base-version>
recreate_zip=true
archive_root_mode=folder
update_public_index=auto
replace_existing_index_entry=true
commit_index_changes=true
publish_release=true
fail_if_release_exists=true
overwrite_release_asset=false
release_notes_file=docs/v<version>/PRE_RELEASE.md
upload_workflow_artifact=true
open_index_pr=true
index_pr_base_branch=main
```

Para `2.1.0-alpha.10`, por ejemplo:

```text
source_folder=JWPLC/2.1.0
release_notes_file=docs/v2.1.0-alpha.10/PRE_RELEASE.md
```

## Por qué `archive_root_mode=folder` es obligatorio

Arduino Boards Manager necesita una única carpeta raíz dentro del ZIP de plataforma.

Estructura válida:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

Una estructura como esta es inválida para nuestro package:

```text
boards.txt
platform.txt
cores/
libraries/
variants/
```

Durante el cierre de Alpha10 se reprodujo el error de Arduino CLI:

```text
searching package root dir: no unique root dir in archive
```

La causa fue publicar con `archive_root_mode=contents`. Desde Alpha10 la UI normal ya no expone esa opción y el workflow fija `folder` internamente.

Además, antes de crear el GitHub Release el workflow valida que:

```text
JWPLC_PACKAGE_UNIQUE_ROOT=PASS
JWPLC_PACKAGE_REQUIRED_FILES=PASS
```

Se exige una única raíz con el nombre de la versión base y la presencia de `boards.txt` y `platform.txt` dentro de ella.

## Flujo normal desde GitHub

Ir a:

```text
Actions
  -> Release JWPLC Arduino Package
  -> Run workflow
```

Seleccionar el branch de release correspondiente, actualmente:

```text
release/v2.1.x
```

Ingresar únicamente:

```text
Version without leading v:
2.1.0-alpha.10
```

El workflow se encarga automáticamente de:

1. Validar formato de versión.
2. Derivar la versión base y `source_folder`.
3. Verificar `boards.txt`, `platform.txt` y `PRE_RELEASE.md`.
4. Rechazar un GitHub Release o tag ya existente.
5. Sembrar los índices desde `main`.
6. Regenerar el ZIP desde el branch de release.
7. Empaquetar siempre con raíz `folder`.
8. Validar la estructura del ZIP.
9. Calcular `SHA-256` y `size`.
10. Actualizar el índice dev.
11. Actualizar el índice estable sólo cuando el canal sea estable.
12. Crear/actualizar la rama automática del índice.
13. Abrir el PR de índice hacia `main`.
14. Crear GitHub Release / PreRelease.
15. Subir el artifact del workflow.

## Reglas de canal

El canal se deriva automáticamente desde la versión.

| Versión | Canal | GitHub | Dev index | Stable index |
|---|---|---|---|---|
| `2.1.0-alpha.10` | alpha | PreRelease | Sí | No |
| `2.1.0-beta.1` | beta | PreRelease | Sí | No |
| `2.1.0` | stable | Release | Sí | Sí |

No se expone `update_public_index` como opción manual en el flujo normal.

## Re-publicación de una misma versión

El workflow normal nunca sobrescribe un GitHub Release/tag existente.

```text
FAIL_IF_RELEASE_EXISTS=true
```

Si una publicación interna se descarta deliberadamente, primero deben eliminarse manualmente el GitHub Release y el tag. El índice dev puede conservar todavía una entrada de esa versión; por eso el generador permite reemplazar esa entrada al volver a publicar, pero sólo después de haber comprobado que Release/tag ya no existen.

Esto permite reparar una alpha interna sin abrir múltiples opciones peligrosas en la UI.

## Auto Release desde README

`.github/workflows/auto-release-jwplc-on-readme.yml` detecta `JWPLC_RELEASE_VERSION` en `README.md` y dispara el mismo workflow pasando únicamente:

```text
version=<versión detectada>
```

La política de empaquetado permanece centralizada en `release-jwplc-package.yml`.

## Herramienta Python

`JWPLC/tools/jwplc_release.py` conserva argumentos avanzados para mantenimiento y diagnóstico local. Esos argumentos no forman parte de la UI normal de publicación.

Ejemplo mínimo equivalente a la política normal:

```bash
python JWPLC/tools/jwplc_release.py prepare \
  --version 2.1.0-alpha.10 \
  --channel auto \
  --source-folder JWPLC/2.1.0 \
  --recreate-zip \
  --archive-root-mode folder \
  --update-public-index auto \
  --replace-existing-index-entry
```

## Principio adoptado

```text
RELEASE_UI_EDITABLE_INPUTS=VERSION_ONLY
PACKAGE_ARCHIVE_ROOT=FOLDER_FIXED
PACKAGE_ZIP_REGENERATED=ALWAYS
CHANNEL=AUTO
PUBLIC_INDEX=AUTO_STABLE_ONLY
INDEX_PR_BASE=MAIN
RELEASE_OVERWRITE=FORBIDDEN
ZIP_STRUCTURE_VALIDATION=REQUIRED
```

El objetivo es reducir errores de operación: las decisiones que forman parte del contrato del package se codifican en el workflow y no se vuelven a preguntar en cada publicación.
