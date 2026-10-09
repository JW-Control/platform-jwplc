# Alpha13 — TFT-PRE6-P0 — Cierre del archive temporal

Fecha: 2026-10-09
Estado: `CLOSED_PASS`

## Resultado

```text
GATE=A13-TFT-PRE6-P0
STATUS=PASS
REASON=TEMP_PRECOMPILED_ARCHIVE_QUALIFIED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
ENVIRONMENT_FAILURE=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
WORKTREE_FINAL=CLEAN
```

## Archivos y miembros

```text
HEAD=89e461123474310e8528e4daf50c97262a86390b
CANDIDATE_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
CANDIDATE_ARCHIVE_BYTES=1091942
TFT_OBJECT_SHA256=0a84fc685255b7b2e42a8050d0c643ce12afd6fcb628273993822b77755b74bb
BACKEND_OBJECT_SHA256=f4b771ba6847cb2b8dca3a7d9c29879725e3e160047df2e9b68b5944fae244cf
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
```

Archivo creado bajo `%TEMP%`, sin reemplazar
`JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a`.

## Compilación normal autocontenida

| Caso | Compile exit | Archive temporal | TFT source .o | Backend source .o | TFT_eSPI externa |
| --- | ---: | --- | ---: | ---: | ---: |
| DIRECT_TFT | 0 | seleccionado | 0 | 0 | no seleccionada |
| DISPLAY_INTEGRATION | 0 | seleccionado | 0 | 0 | no seleccionada |
| NORMAL_AUTOLOAD | 0 | seleccionado | 0 | 0 | no seleccionada |

Todos los casos:

```text
TFT_PRECOMPILED=True
STUB_CORE=True
CORE_ARCHIVE_LINKED=True
```

La fuente del archive candidato son los **objetos de PRE5 R3**, ya
compilados temporalmente en el experimento físico. Por tanto P0
verifica empaquetado y enlazado, pero **no cierra reproducibilidad desde
fuentes canónicas del package**.

## Próximo gate

```text
NEXT=TFT-PRE6-P1A
OBJECTIVE=REGENERATE_PRE4_THREE_SOURCE_HASHES_FROM_CANONICAL_INPUTS
PRODUCT_CHANGE=NO
COMPILE=NO
UPLOAD=NO
P1B_SOURCE_COMPILATION=ONLY_AFTER_P1A_PASS
```

P1A exige versiones/hashes exactos del código JWPLC_TFT y de la
instalación de mantenimiento TFT_eSPI 2.5.43. Acepta exclusivamente una
normalización CRLF→LF al verificar el hash de blobs Git, por compatibilidad
con checkout Windows, y reproduce los tres SHA candidatos de PRE4.
