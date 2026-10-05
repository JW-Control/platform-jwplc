# Alpha12 — Source Final Freeze tras P8

Fecha: 2026-10-04

## Resultado

```text
P8A_MODBUS_TCP_CLIENT_API_IMPLEMENTATION_CLOSURE=PASS
P8_EXAMPLES_API_REVIEW=PASS
BACKWARD_COMPATIBILITY_REVIEW=PASS_WITH_ACCEPTED_EXCEPTION
SOURCE_FINAL_FREEZE=PASS
```

## Source congelado

El último commit que modifica el package bajo `JWPLC/2.1.0/` es:

```text
PACKAGE_SOURCE_HEAD=ab4379a177492ee851c1afccf5398e649c55dd4f
```

Desde ese commit hasta el cierre de P8 sólo se añadieron o modificaron archivos
de `docs/` y `tools/`.

Regla desde este punto:

```text
CHANGE_UNDER_JWPLC_2_1_0
-> INVALIDATE_SOURCE_FINAL_FREEZE
-> REQUALIFY_AFFECTED_SCOPE
```

## Compatibilidad TFT

Se acepta explícitamente la migración:

```text
Alpha11: Adafruit_ST7789&
Alpha12: JWPLC_TFTClass&
```

El patrón recomendado:

```cpp
auto &tft = JWPLC_Display.tft();
```

permanece compatible.

Un sketch que tipa explícitamente:

```cpp
Adafruit_ST7789 &tft = JWPLC_Display.tft();
```

requiere migración a `JWPLC_TFTClass&` o `auto&`.

Decisión:

```text
EXPLICIT_ADAFRUIT_REFERENCE=BREAKS_ACCEPTED
LEGACY_ADAFRUIT_BACKEND_REINTRODUCED=NO
```

No se reintroduce Adafruit como segundo backend únicamente para conservar un
tipo legacy que todavía no tiene adopción externa oficial.

## Decisiones diferidas retenidas

```text
OPENPLC_RUNTIME_AUTOLOAD=NO
OTA=NOT_DEFINED
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_BIN_FINAL=NO
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL_NOT_DEFAULT
```

## Consecuencia para validación

La consolidación previa de `JWPLC_ModbusTCP_Client.cpp` cambió package source y
redujo el número de translation units de Modbus TCP Client. Por tanto, aunque
`JWPLC_ModbusTCP` sigue siendo source-only y no exige regenerar archives de
otras librerías, deben repetirse sobre este source congelado:

```text
P7_RELEASE_LIKE_VALIDATION=PENDING_RERUN
FINAL_BUILD_SPEED_BENCHMARK=PENDING_RERUN
```

Sólo después se continúa con los gates finales CLI/IDE/upload.
