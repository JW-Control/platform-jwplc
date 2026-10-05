# Alpha12 — Gate final Arduino IDE post-freeze

Fecha: 2026-10-05

## Entorno

```text
ARDUINO_IDE=2.3.4
BOARD=JWPLC Basic
SKETCH=tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
ACTION=Verify/Compile
UPLOAD_PERFORMED=NO
```

## Resultado

```text
ARDUINO_IDE_FINAL_COMPILE=PASS
BUNDLED_ADAFRUIT_BUSIO_SELECTED=YES
EXTERNAL_SKETCHBOOK_BUSIO_SELECTED=NO
FLASH_USED_BYTES=431957
FLASH_MAX_BYTES=4063232
FLASH_PERCENT=10
RAM_USED_BYTES=29396
RAM_PERCENT=8
RAM_FREE_BYTES=298284
```

## Evidencia de autocontención

Arduino IDE resolvió `Adafruit BusIO` desde:

```text
C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\2.1.0\libraries\Adafruit_BusIO
```

La copia externa del sketchbook no fue seleccionada.

## Clasificación

```text
FINAL_ARDUINO_IDE_GATE=PASS
STRICT_AUTOCONTAINMENT_ARDUINO_IDE=PASS
PRODUCT_FAILURE=NO
UPLOAD_REQUIRED_BY_SCOPE=NO
```
