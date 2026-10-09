# Alpha13 — TFT-PRE6-P1B — cierre de reconstrucción independiente

Fecha: 2026-10-09
Estado: CLOSED_PASS

## Resultado real

```text
GATE=A13-TFT-PRE6-P1B
STATUS=PASS
REASON=CANONICAL_SOURCE_ARCHIVE_REBUILT_AND_LINKED
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
WORKTREE_FINAL=CLEAN
HEAD=8598a0dbee6006b27c28f1232ca2877d882423fe
```

## Procedencia y compilación source-first

```text
P1A_SOURCE_ROOT=%TEMP%\jwplc_a13_tft_pre6_p1a_20261009_165802
SOURCE_COMPILE_EXIT=0
SOURCE_JWPLC_TFT_TEMP_SELECTED=True
SOURCE_TFT_eSPI_TEMP_SELECTED=True
SOURCE_CORE_STUB=True
SOURCE_CORE_ARCHIVE_LINKED=True
SOURCE_DISPLAY_PRECOMPILED=True
JWPLC_TFT_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
SKETCH_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
BACKEND_INSTRUMENTED_CPP_SHA256=3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350
BACKEND_INSTALLED_ORIGINAL_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
```

## Nuevo archive

```text
CANDIDATE_ARCHIVE=%TEMP%\jwplc_a13_tft_pre6_p1b_20261009_171029\libraries\JWPLC_TFT\src\esp32\libJWPLC_TFT.a
CANDIDATE_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
CANDIDATE_ARCHIVE_BYTES=1091990
REGENERATED_TFT_OBJECT_SHA256=03063a848b5d5e20d8010b7d0bc19189b7cc5be412489588c1b14ded66815adf
REGENERATED_BACKEND_OBJECT_SHA256=59b4050dd44b5f6db7a99b2050aee7ff076008eb28032d18d222f4cf57b06732
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER_PARITY=PASS
```

Los hashes binarios difieren de P0 porque son objetos recién compilados.
No exigir igualdad de archive SHA con P0: son compilaciones
independientes. Exigir fuentes correctas, dos miembros exactos,
paridad y tests normales exitosos.

## Tres pruebas de Arduino normales

| Sketch | Compilación | JWPLC_TFT precompilada | TFT_eSPI externa | Core archive |
| --- | --- | --- | --- | --- |
| 04.Display_TFT_Direct | PASS | Sí | No seleccionada | Enlazado |
| Display_UserUI_Callbacks | PASS | Sí | No seleccionada | Enlazado |
| 01_empty (autoload) | PASS | Sí | No seleccionada | Enlazado |

```text
COMPILE_CASES_PASS=3
GLOBAL_TFT_ESPI_REQUIRED_FOR_NORMAL_SKETCH=NO
OFFICIAL_ARCHIVES_PRESERVED=YES
PRODUCT_REPO_MODIFIED=NO
```

## Interpretación y siguiente gate

P1B demuestra la generación reproducible del binario candidato
**sin reusar los objetos PRE5/P0**. El archive oficial del package
todavía conserva el SHA anterior:
5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738.

```text
NEXT=TFT-PRE6-P2A
ACTION=LOCAL_GUARDED_ADOPTION
FILES=JWPLC_TFT.cpp,tft_setup.h,src/esp32/libJWPLC_TFT.a
PHYSICAL_UPLOAD=NO
COMMIT=NO
FAILURE_ROLLBACK=REQUIRED
FOLLOWUP=TFT-PRE6-P2B_PHYSICAL_USB_STARTUP
```

P2A instalará la pareja source/archive candidata en **worktree local**
con copia de seguridad bajo %TEMP%, comprobará hashes, y compilará
cuatro casos normales desde la librería de producto. La rama sólo
quedará modificada localmente hasta gate físico P2B y revisión de
regresiones posteriores. No hacer commit del archive antes del resultado
de P2B.
