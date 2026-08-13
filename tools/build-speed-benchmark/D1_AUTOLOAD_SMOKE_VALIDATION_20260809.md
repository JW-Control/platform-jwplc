# Validación D1 — contrato de autoload

Fecha: 2026-08-09

## Resultado

**APROBADO**.

El smoke sketch `03_autoload_contract` compiló correctamente con:

```text
jwplc_local:esp32:jwplcbasic
```

sobre la rama:

```text
v2.1.0-alpha.4/feature/build-speed-cache
```

## Evidencia funcional de build

El log recibido mostró:

```text
Successfully created ESP32 image.
```

y la generación completa de:

- aplicación `.bin`;
- particiones;
- imagen merged de 4 MB;
- `flash_args`;
- ELF final enlazado correctamente.

Tamaño reportado:

```text
Sketch: 406229 bytes (12%)
RAM global: 27908 bytes (8%)
```

## Contrato de autoload conservado

Sin añadir includes manuales del ecosistema JWPLC, el build resolvió y enlazó:

- JWPLC_Display
- JWPLC_GlobalPeripherals
- JW_RTC
- JW_FRAM
- JW_SD
- SD
- FS
- JW_MatrixButtons
- JWPLC_Ethernet
- Ethernet
- JWPLC_RS485
- JWPLC_ModbusRTU
- Adafruit ST7735/ST7789
- Adafruit GFX
- Adafruit BusIO
- Wire
- SPI

Por tanto, D1 no elimina periféricos del autoload normal.

## Observación pendiente

Arduino reportó múltiples candidatos para algunas librerías externas y seleccionó versiones del sketchbook del usuario para:

- Adafruit ST7735/ST7789
- Adafruit GFX
- Adafruit BusIO
- Ethernet

Mientras que `SD` fue seleccionado desde el package JWPLC.

Esto no bloquea D1, pero se mantiene como pendiente de reproducibilidad antes de cerrar Alpha4.

## Decisión

D1 queda aceptado como optimización válida de library discovery para continuar con P1.

Siguiente fase:

```text
P1 — precompilación/reutilización de librerías estables para reducir cold build
```
