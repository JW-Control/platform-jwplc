# Alpha13 — TFT-PRE6-P1A R3 — reconstrucción idéntica de fuentes

Fecha: 2026-10-09
Estado: `CLOSED_PASS`

## Resultado real

```text
GATE=A13-TFT-PRE6-P1A
STATUS=PASS
REASON=CANONICAL_SOURCE_TRANSFORM_REPRODUCED
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
WORKTREE_FINAL=CLEAN
HEAD=e17b85dc9826b284534fcd465c8678c5f7a25c21
```

La primera salida del generador Python reportó
`REASON=CANONICAL_SOURCE_TRANSFORMATION_REPRODUCED`, y
la capa del gate PowerShell validó sus campos y reportó el contrato
canónico `CANONICAL_SOURCE_TRANSFORM_REPRODUCED`.

## Recuperación de la evidencia F109

```text
PROOF_PARSER_SELF_TEST=PASS
P0_HANDOFF_RECOVERED=YES
P0_PROOF_SOURCE=VERSIONED_CLOSURE_AND_ARCHIVE_SHA256
P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING
P0_ARCHIVE_BYTES=1091942
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
VERSIONED_P0_PROOF_BLOB=15a33a9bb686fdfb4b22277e792fd194669c1010
VERSIONED_PRE4_PROOF_BLOB=f9f9b2b683d7fcd85644e50a3a55f8bf54f1a673
VERSIONED_PRE3_PROOF_BLOB=53f636a7c90a38929402e5696c89fe9ffd8277cd
```

El SUMMARY.log previo no fue recreado ni P0 recompilado.

## Regeneración exacta desde fuentes canónicas

```text
CANDIDATE_JWPLC_TFT_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
CANDIDATE_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
CANDIDATE_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
TEMP_BACKEND_CPP_INSTRUMENTED_SHA256=3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350
GLOBAL_TFT_ESPI_MUTATED=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
```

Directorio temporal de la corrida:

```text
%TEMP%\jwplc_a13_tft_pre6_p1a_20261009_165802
```

El generador versionado `a13_tft_pre6_p1_source_regen.py`
reproduce los cambios de PRE4 sobre `JWPLC_TFT.cpp`,
`tft_setup.h` y `ST7789_Init.h` con hashes exactos y copies
temporales del backend TFT_eSPI 2.5.43, sin modificar sus archivos
originales.

## Próximo gate: TFT-PRE6-P1B

P1B toma los fuentes P1A a través de un manifiesto temporal cuyo
contenido se verifica por SHA; no depende de los SUMMARY.log efímeros.
Compilará un `JWPLC_TFT.cpp.o` y un `TFT_eSPI.cpp.o`
nuevos, construirá un nuevo `libJWPLC_TFT.a` en
`%TEMP%`, exigirá dos miembros y paridad byte a byte, y compilará
los mismos tres consumidores normales de P0.

```text
PRODUCT_ARCHIVE_REFRESH=NOT_STARTED
PHYSICAL_NORMAL_PACKAGE_GATE=NOT_STARTED
NEXT=TFT_PRE6_P1B
```
