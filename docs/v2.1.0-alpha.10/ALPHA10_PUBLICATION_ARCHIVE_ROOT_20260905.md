# Alpha10 - Incidente de raíz del ZIP de publicación

Fecha: 2026-09-05

## Resumen

Durante el gate final del package publicado de `v2.1.0-alpha.10`, Arduino CLI descargó correctamente el artefacto desde el índice dev, pero rechazó la instalación antes de copiar la plataforma.

Salida observada:

```text
Error during install: Cannot install platform: installing platform jwplc:esp32@2.1.0-alpha.10: searching package root dir: no unique root dir in archive, found '...\\cores' and '...\\libraries'
ALPHA10_PUBLISHED_INSTALL=FAIL_INVALID_ARCHIVE_ROOT
```

## Causa raíz

El workflow manual de release fue ejecutado con:

```text
source_folder=JWPLC/2.1.0
archive_root_mode=contents
recreate_zip=true
```

Con `archive_root_mode=contents`, el ZIP contiene directamente entradas como:

```text
cores/
libraries/
variants/
...
```

Arduino Boards Manager exige que el archive de una plataforma tenga una única carpeta raíz. Para este package la estructura válida es:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

Por tanto, la configuración correcta del workflow es:

```text
archive_root_mode=folder
```

## Alcance

El fallo pertenece únicamente al empaquetado/publicación.

```text
PACKAGE_SOURCE_CHANGED_BY_FAILURE=NO
RUNTIME_CHANGED_BY_FAILURE=NO
PRECOMPILED_ARCHIVES_CHANGED_BY_FAILURE=NO
LOCAL_TECHNICAL_GATES_INVALIDATED=NO
```

Los benchmarks, la matriz funcional 5/5 y el gate físico ejecutados sobre el candidato técnico continúan siendo válidos.

## Corrección de proceso

Antes de republicar Alpha10 se debe endurecer el tooling de release para que un ZIP sin raíz única no pueda avanzar a publicación:

- `jwplc_release.py` debe usar `folder` como default y validar una única raíz con `boards.txt` y `platform.txt`;
- el workflow de publicación debe rechazar `archive_root_mode != folder`;
- `README_RELEASE_AUTOMATION.md` debe documentar `folder` como único modo soportado para Boards Manager.

## Estado

```text
ALPHA10_RELEASE_WORKFLOW_RUN_18=SUCCESS_BUT_ARTIFACT_INVALID
ALPHA10_PUBLISHED_INSTALL=FAIL_INVALID_ARCHIVE_ROOT
ALPHA10_RELEASE_ARTIFACT_REPLACEMENT=REQUIRED
ALPHA10_STATUS=NOT_CLOSED
NEXT=FIX_RELEASE_TOOLING_AND_REPUBLISH
```
