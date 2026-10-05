# Alpha12 — Gate de autocontención

Fecha: 2026-10-05

## Resultado

```text
BRANCH=v2.1.0-alpha.12/feature/modbus-tcp
HEAD=da70c875f0c54a25287d38b54dd01fe43f4bf8be
ENTRY_DIRTY_COUNT=0

LEGACY_GFX_ABSENT=True
LEGACY_ST77XX_ABSENT=True
BUNDLED_BUSIO_PRESENT=True
BUNDLED_BUSIO_MARKER_PRESENT=True
FRAM_BUNDLED_MARKER_BEFORE_SPI_DEVICE=True

COMPILE_EXIT=0
BUSIO_SELECTION_COUNT=1
BUNDLED_BUSIO_SELECTED=True
LEGACY_GFX_SELECTED=False
LEGACY_ST77XX_SELECTED=False

WARNING_LINES=0
ERROR_LINES=0
FINAL_DIRTY_COUNT=0
ALPHA12_AUTOCONTAINMENT_GATE=PASS
```

## Evidencia de resolución

Arduino Builder seleccionó:

```text
C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\2.1.0\libraries\Adafruit_BusIO
```

aun existiendo una instalación externa en el sketchbook del usuario.

Por tanto:

```text
PACKAGE_AUTOCONTAINED_FOR_BUSIO=YES
EXTERNAL_BUSIO_REQUIRED=NO
LEGACY_ADAFRUIT_GRAPHICS_REQUIRED=NO
```

## Source candidate

```text
PACKAGE_SOURCE_HEAD=17be4204223f6c5c3dde0f079debd31533abe167
```

Desde ese commit los cambios posteriores son únicamente gates/documentación.

Siguiente paso:

```text
P7_REVALIDATION_AFTER_AUTOCONTAINMENT
```
