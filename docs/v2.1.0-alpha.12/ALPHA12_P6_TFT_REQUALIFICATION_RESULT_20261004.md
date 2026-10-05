# Alpha12 — P6 JWPLC_TFT precompiled requalification

Fecha: 2026-10-04

## Resultado

```text
P6_TFT_REQUALIFICATION=PASS_CLOSED
PRODUCT_FAILURE=NO
OFFICIAL_ARCHIVE_CHANGED=NO
PHYSICAL_UPLOAD_PERFORMED=NO
```

P6 requalifica el archive histórico de `JWPLC_TFT` sin regenerarlo.

## Identidad evaluada

Branch:

```text
v2.1.0-alpha.12/feature/modbus-tcp
```

HEAD de la corrida:

```text
e0e49bd38c4ca42eabdf07d5d2787a5d402d16cc
```

Artifact:

```text
JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
BYTES=1091098
SHA256=5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
```

El SHA-256 y tamaño coinciden exactamente con el artifact físicamente
cualificado/adoptado en H3E4.

## Source vigente

```text
CURRENT_HEADER_BLOB=c24373eddaff5f148092c49fe997fb59f94bdbdc
CURRENT_CPP_BLOB=2bdb55d504cfd2a1481ff561d33535d1740bb472
CURRENT_SETUP_BLOB=773f8123844f783bfc67c3123150c27f29f45d34
CURRENT_SOURCE_BLOB_IDENTITY=PASS
```

La compilación source-first reproduce la receta histórica de mantenimiento:

```text
-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1
```

Resultado:

```text
SOURCE_COMPILE_EXIT=0
SOURCE_TFT_OBJECT_COUNT=1
SOURCE_TFT_ESPI_OBJECT_COUNT=1
SOURCE_TFT_PRECOMPILED_MARKER=NO
SOURCE_WARNING_LINES=0
SOURCE_ERROR_LINES=0
SOURCE_TFT_ESPI_VERSION=2.5.43
SOURCE_FIRST_CURRENT_SOURCE=PASS
```

## Estructura del archive

```text
ARCHIVE_MEMBER_COUNT=2
ARCHIVE_MEMBER=JWPLC_TFT.cpp.o
ARCHIVE_MEMBER=TFT_eSPI.cpp.o
```

Los objetos reconstruidos hoy no son bit-for-bit idénticos a los objetos
históricos:

```text
JWPLC_TFT_OBJECT_BIT_FOR_BIT_REBUILD=NO
TFT_ESPI_OBJECT_BIT_FOR_BIT_REBUILD=NO
OBJECT_BIT_FOR_BIT_REBUILD_REQUIRED=NO
```

Esto queda como observabilidad y no como requisito de PASS. La adopción
histórica no definió reproducibilidad byte-a-byte entre corridas futuras.

## Equivalencia source / precompiled

```text
SOURCE_FLASH_BYTES=415821
CANDIDATE_FLASH_BYTES=415813
FLASH_DELTA_BYTES=-8

SOURCE_RAM_BYTES=28300
CANDIDATE_RAM_BYTES=28300
RAM_DELTA_BYTES=0

SOURCE_DEFINED_SYMBOL_COUNT=3416
CANDIDATE_DEFINED_SYMBOL_COUNT=3416
DEFINED_SYMBOL_NAME_TYPE_SIZE_PARITY=PASS
STRUCTURAL_EQUIVALENCE=PASS
```

El delta de -8 bytes se clasifica como layout de link y no como diferencia de
contrato.

## Candidate autocontenido

Tres builds independientes utilizaron el archive temporal release-like:

### Direct TFT

```text
DIRECT_TFT_COMPILE_EXIT=0
DIRECT_TFT_TFT_SOURCE_OBJECT_COUNT=0
DIRECT_TFT_TFT_ESPI_SOURCE_OBJECT_COUNT=0
DIRECT_TFT_TFT_PRECOMPILED_MARKER=YES
DIRECT_TFT_GLOBAL_TFT_ESPI_SELECTED=NO
DIRECT_TFT_WARNING_LINES=0
DIRECT_TFT_ERROR_LINES=0
DIRECT_TFT_PRECOMPILED_LINK=PASS
```

### Display integration

```text
DISPLAY_INTEGRATION_COMPILE_EXIT=0
DISPLAY_INTEGRATION_TFT_SOURCE_OBJECT_COUNT=0
DISPLAY_INTEGRATION_TFT_ESPI_SOURCE_OBJECT_COUNT=0
DISPLAY_INTEGRATION_TFT_PRECOMPILED_MARKER=YES
DISPLAY_INTEGRATION_GLOBAL_TFT_ESPI_SELECTED=NO
DISPLAY_INTEGRATION_WARNING_LINES=0
DISPLAY_INTEGRATION_ERROR_LINES=0
DISPLAY_INTEGRATION_PRECOMPILED_LINK=PASS
```

### Normal autoload

```text
NORMAL_AUTOLOAD_COMPILE_EXIT=0
NORMAL_AUTOLOAD_TFT_SOURCE_OBJECT_COUNT=0
NORMAL_AUTOLOAD_TFT_ESPI_SOURCE_OBJECT_COUNT=0
NORMAL_AUTOLOAD_TFT_PRECOMPILED_MARKER=YES
NORMAL_AUTOLOAD_GLOBAL_TFT_ESPI_SELECTED=NO
NORMAL_AUTOLOAD_WARNING_LINES=0
NORMAL_AUTOLOAD_ERROR_LINES=0
NORMAL_AUTOLOAD_PRECOMPILED_LINK=PASS
```

El log de normal autoload seleccionó además los periféricos/librerías normales
del package, incluyendo Display, RTC, FRAM, SD, botonera, Ethernet, RS-485 y
Modbus RTU. P6 no elimina periféricos para obtener la mejora de build.

## Evidencia entregada

ZIP externo:

```text
tft_precompiled_requalify_20261004_114112.zip
BYTES=28987
SHA256=0b3dc2f63f3b6f6108adcb5a402f188665a3e3038f29ae7f885c399f8419aaad
```

Contenido:

```text
candidate_direct_tft.log
candidate_display_integration.log
candidate_normal_autoload.log
MANIFEST.txt
source.log
SUMMARY.txt
```

La revisión del ZIP confirmó:

```text
LOG_WARNING_LINES=0
LOG_ERROR_LINES=0
SUMMARY_CONSISTENT_WITH_CONSOLE=YES
MANIFEST_ARCHIVE_IDENTITY_MATCH=YES
GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
FINAL_TRACKED_DIRTY_COUNT=0
```

Los resultados crudos permanecen fuera del árbol tracked según la política de
`tools/alpha12/results/`.

## Cierre P6

```text
P1_CORE=PASS_ADOPTED
P2_MODBUS_RTU=PASS_ADOPTED
P3_SPI=PASS_ADOPTED
P4_JW_SD=PASS_ADOPTED
P5_DISPLAY=PASS_ADOPTED
P6_TFT_REQUALIFICATION=PASS_CLOSED
```

P6 no implica todavía freeze global de precompilados.

Siguiente gate:

```text
P7_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT
-> PRECOMPILED_FREEZE
-> FINAL_BUILD_SPEED_BENCHMARK
-> FINAL_CLI_IDE_UPLOAD_GATES
-> RELEASE
```
