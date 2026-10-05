# Alpha12 — Gate final Arduino IDE

Fecha: 2026-10-05

## Entorno

```text
ARDUINO_IDE=2.3.4
BOARD=JWPLC Basic
SKETCH=tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
ACTION=Verify/Compile
UPLOAD=NOT_REQUIRED_BY_SCOPE
```

## Resultado

La compilación final desde Arduino IDE terminó correctamente.

```text
FLASH_USED=439617
FLASH_MAX=4063232
FLASH_PERCENT=10

RAM_GLOBAL_USED=29692
RAM_MAX=327680
RAM_PERCENT=9
```

El IDE resolvió las librerías JWPLC desde el árbol local del repositorio
`JWPLC/2.1.0/libraries`, incluyendo Display, TFT, GlobalPeripherals, RTC,
FRAM, SD, MatrixButtons, Ethernet, RS485 y ModbusRTU.

## Cierre

```text
FINAL_ARDUINO_IDE_GATE=PASS
FINAL_PHYSICAL_RETEST=NOT_REQUIRED_BY_SCOPE
FINAL_UPLOAD_RETEST=NOT_REQUIRED_BY_SCOPE
SOURCE_FINAL_FREEZE=UNCHANGED
PRECOMPILED_FREEZE=PASS
FINAL_ARDUINO_CLI_GATE=PASS
NEXT=ALPHA12_RELEASE_CLOSURE
```
