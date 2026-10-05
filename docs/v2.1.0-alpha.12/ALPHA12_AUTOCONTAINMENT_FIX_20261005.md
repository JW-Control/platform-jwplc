# Alpha12 — Corrección pre-release de autocontención

Fecha: 2026-10-05

## Hallazgo

Durante el Verify final de Arduino IDE se observó que, para
`Adafruit_SPIDevice.h`, Arduino seleccionó una instalación externa de
`Adafruit_BusIO` del sketchbook en lugar de la copia incluida en el package.

Esto no produjo fallo de compilación, pero no es aceptable como evidencia de
autocontención estricta.

## Auditoría

`JW_FRAM` usa directamente:

```cpp
#include <Adafruit_SPIDevice.h>
```

y declara dependencia de Adafruit BusIO.

En cambio, el stack gráfico actual es:

```text
JWPLC_Display
-> JWPLC_TFT
-> backend TFT_eSPI encapsulado/precompilado
```

`JWPLC_Display` y `JWPLC_LogicRuntime_UI` ya trabajan con
`JWPLC_TFTClass`. Las librerías gráficas Adafruit heredadas ya no son parte
del backend productivo.

## Cambio

Commit productivo:

```text
17be4204223f6c5c3dde0f079debd31533abe167
```

Cambios:

```text
KEEP=Adafruit_BusIO
REMOVE=Adafruit_GFX_Library
REMOVE=Adafruit_ST7735_and_ST7789_Library
```

`JW_FRAM.h` ahora fuerza primero el marcador exclusivo:

```cpp
#include <JWPLC_Bundled_Adafruit_BusIO.h>
#include <Adafruit_SPIDevice.h>
```

Así Arduino Builder debe descubrir primero la copia bundled antes de resolver
el header genérico.

## Consecuencia

```text
PREVIOUS_SOURCE_FREEZE=INVALIDATED
PREVIOUS_FINAL_IDE_GATE=SUPERSEDED
P7_GLOBAL_ARCHIVE_INVENTORY=REVALIDATION_REQUIRED
PHYSICAL_RETEST=NOT_REQUIRED_BY_THIS_CHANGE
```

No cambia la API pública ni el runtime funcional de FRAM; cambia la resolución
de dependencia durante build y se limpia el package de dos librerías gráficas
legacy.

## Gate requerido

```text
tools/alpha12/gates/run_alpha12_autocontainment_gate.ps1
```

Debe demostrar:

```text
LEGACY_GFX_ABSENT=True
LEGACY_ST77XX_ABSENT=True
BUNDLED_BUSIO_PRESENT=True
FRAM_BUNDLED_MARKER_BEFORE_SPI_DEVICE=True
BUNDLED_BUSIO_SELECTED=True
LEGACY_GFX_SELECTED=False
LEGACY_ST77XX_SELECTED=False
COMPILE_EXIT=0
ALPHA12_AUTOCONTAINMENT_GATE=PASS
```
