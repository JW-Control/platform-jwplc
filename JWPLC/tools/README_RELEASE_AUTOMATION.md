# Automatización de release del package JWPLC

Esta carpeta contiene herramientas para automatizar la generación del ZIP del package Arduino JWPLC, el cálculo de `SHA-256`, el cálculo de `size`, la actualización de los índices Arduino y la creación del GitHub Release / Pre-release.

## Archivos

| Archivo | Uso |
|---|---|
| `jwplc_release.py` | Script Python usado por GitHub Actions para preparar el ZIP y actualizar los JSON. |
| `.github/workflows/release-jwplc-package.yml` | Workflow manual para publicar releases del package JWPLC. |

## Qué automatiza

El workflow manual puede:

1. Validar la versión ingresada.
2. Detectar canal `alpha`, `beta` o `stable`.
3. Reutilizar un ZIP existente o generar uno nuevo.
4. Calcular `SHA-256`.
5. Calcular `size`.
6. Actualizar `JWPLC/package_jwplc_index_dev.json`.
7. Actualizar `JWPLC/package_jwplc_index.json` solo para versiones estables, salvo que se fuerce lo contrario.
8. Crear un GitHub Release o Pre-release.
9. Subir el ZIP como asset del release.
10. Publicar los archivos finales como artifact del workflow.

## Reglas de canal

| Versión | Canal detectado | GitHub Release | Dev index | Public index |
|---|---|---:|---:|---:|
| `2.1.0-alpha.1` | `alpha` | Pre-release | Sí | No |
| `2.1.0-beta.1` | `beta` | Pre-release | Sí | No |
| `2.1.0` | `stable` | Release | Sí | Sí |

## Comportamiento del ZIP

Nombre estándar:

```txt
jwplc-esp32-<version>.zip
```

Ejemplo:

```txt
jwplc-esp32-2.1.0-alpha.1.zip
```

### Si el ZIP ya existe

Si el ZIP existe en:

```txt
JWPLC/jwplc-esp32-<version>.zip
```

y `recreate_zip = false`, el workflow lo reutiliza.

Esto sirve si el ZIP ya fue generado/revisado manualmente.

### Si el ZIP no existe

Si no existe el ZIP, el workflow genera uno nuevo desde `source_folder` y lo coloca en:

```txt
dist/jwplc-esp32-<version>.zip
```

### Si `recreate_zip = true`

Si `recreate_zip = true`:

- Si se indicó `zip_file`, ese archivo puede ser regenerado/sobrescrito.
- Si no se indicó `zip_file`, el script **no sobrescribe** el ZIP ubicado en `JWPLC/`; genera uno nuevo en `dist/`.

Esto evita borrar por accidente un ZIP preparado manualmente dentro de `JWPLC/`.

## Comportamiento de los índices

### `package_jwplc_index_dev.json`

Siempre se actualiza.

El índice dev conserva histórico de alphas, betas y releases estables.

Si la versión ya existe:

- con `replace_existing_index_entry = false`, se conserva protección contra reemplazo accidental;
- con `replace_existing_index_entry = true`, se reemplaza la entrada existente.

### `package_jwplc_index.json`

Por defecto, solo se actualiza si el canal es `stable`.

Regla:

```txt
update_public_index = auto
```

Significa:

```txt
stable -> actualiza package_jwplc_index.json
alpha/beta -> no actualiza package_jwplc_index.json
```

También se puede forzar:

```txt
update_public_index = yes
update_public_index = no
```

Para uso normal, dejar `auto`.

## Comportamiento del GitHub Release

El workflow usa el tag:

```txt
v<version>
```

Ejemplo:

```txt
v2.1.0-alpha.1
```

### Si el release ya existe

Si `fail_if_release_exists = true`, el workflow se detiene.

Si `fail_if_release_exists = false`, el workflow puede editar el release existente.

Si además `overwrite_release_asset = true`, reemplaza el asset ZIP con el mismo nombre.

Para uso normal:

```txt
fail_if_release_exists = true
overwrite_release_asset = false
```

Así se evita pisar releases por accidente.

## Uso recomendado desde GitHub

Ir a:

```txt
Actions > Release JWPLC Arduino Package > Run workflow
```

### Para alpha

```txt
version: 2.1.0-alpha.1
channel: auto
source_folder: JWPLC/2.1.0
recreate_zip: false
archive_root_mode: contents
update_public_index: auto
replace_existing_index_entry: false
fail_if_release_exists: true
overwrite_release_asset: false
release_notes_file: docs/alpha32_openplc_integration/PRE_RELEASE.md
```

Resultado esperado:

```txt
- Crea Pre-release v2.1.0-alpha.1.
- Sube jwplc-esp32-2.1.0-alpha.1.zip.
- Actualiza package_jwplc_index_dev.json.
- No actualiza package_jwplc_index.json.
```

### Para release estable

```txt
version: 2.1.0
channel: auto
source_folder: JWPLC/2.1.0
recreate_zip: false
archive_root_mode: contents
update_public_index: auto
replace_existing_index_entry: false
fail_if_release_exists: true
overwrite_release_asset: false
release_notes_file: RELEASE_NOTES.md
```

Resultado esperado:

```txt
- Crea Release v2.1.0.
- Sube jwplc-esp32-2.1.0.zip.
- Actualiza package_jwplc_index_dev.json.
- Actualiza package_jwplc_index.json.
```

## Uso local del script Python

También puede ejecutarse localmente:

```bash
python JWPLC/tools/jwplc_release.py prepare \
  --version 2.1.0-alpha.1 \
  --channel auto \
  --source-folder JWPLC/2.1.0 \
  --archive-root-mode contents \
  --update-public-index auto
```

Para regenerar ZIP:

```bash
python JWPLC/tools/jwplc_release.py prepare \
  --version 2.1.0-alpha.1 \
  --channel auto \
  --source-folder JWPLC/2.1.0 \
  --recreate-zip
```

## Validaciones pendientes recomendadas

Antes de usarlo como flujo final estable:

- Probar primero con una versión alpha.
- Confirmar que el ZIP generado tiene la estructura correcta para Arduino Boards Manager.
- Confirmar que Arduino IDE descarga desde `package_jwplc_index_dev.json`.
- Confirmar que `package_jwplc_index.json` no cambia para alpha/beta.
- Confirmar que un release existente no se sobrescribe accidentalmente.
