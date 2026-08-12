# JWPLC v2.1.0-alpha.4 — P8 Wire + SPI precompilados

## Objetivo

Evaluar si precompilar `Wire` y `SPI` reduce el tiempo cold de `JWPLC Basic` sin retirar periféricos del autoload, sin romper las APIs Arduino ya probadas y conservando source fallback.

FQBN principal: `jwplc_local:esp32:jwplcbasic`.

Sketch de benchmark: `tools/build-speed-benchmark/sketches/01_empty`.

Gate físico: `tools/build-speed-benchmark/sketches/05_p8_wire_spi_gate/05_p8_wire_spi_gate.ino`.

## Cambio

Se añadió `precompiled=full` a `Wire` y `SPI` y se generaron los archives ESP32:

| Archive | Bytes | SHA-256 |
|---|---:|---|
| `libWire.a` | 166980 | `A864851EBFCB8CD3FEE55D3D7834B81254AD4EBE0D75F6ED9EBC846355F9C4AA` |
| `libSPI.a` | 79714 | `F9883DFD39CA299F7CB76673744F8F0DE4EA01C5A53F186A3841199FCA289245` |

El source fallback permanece disponible.

## Rendimiento

Perfil preliminar P8 (`compile-profile-work/20260810_153616`):

- discovery: 40.861 s;
- TUs desde fuente: 5;
- suma secuencial de TUs: 8.388 s;
- cold `-j 0`: 54.371 s.

Perfil controlado P8 con la laptop conectada (`compile-profile-work/20260810_191329`):

- discovery: 59.363 s;
- TUs desde fuente: 5;
- suma secuencial de TUs: 9.595 s;
- cold `-j 0`: 61.224 s.

Para la decisión de rendimiento se ejecutó una comparación directa A-B-B-A, sin profiling previo:

| Run | Configuración | Cold |
|---|---|---:|
| A1 | Wire + SPI source-only | 66.270 s |
| B1 | P8 precompilado | 60.643 s |
| B2 | P8 precompilado | 59.158 s |
| A2 | Wire + SPI source-only | 63.500 s |

Promedios:

- source-only: **64.885 s**;
- P8: **59.901 s**;
- ganancia P8: **4.985 s / 7.68 %**.

Ambos runs P8 fueron más rápidos que ambos runs source-only.

## Condición del host

Se comprobó que la laptop reduce fuertemente su rendimiento funcionando en batería. Con aproximadamente 6 GB de RAM libre y Wire+SPI source-only:

- sin cargador (`BatteryStatus=1`): 118.807 s;
- con cargador (`BatteryStatus=2`): 62.390 s.

El run anómalo de ~291 s no se toma como regresión de P8. Para comparaciones formales en esta laptop se fija como condición estar conectada al cargador.

## Compatibilidad Wire con I2C inicializado por JWPLC

Durante P8 se encontró un bug preexistente: JWPLC inicializa el HAL I2C antes de `setup()`, y el `TwoWire::begin()` original podía retornar por `i2cIsInit(num)` antes de reservar sus buffers. El síntoma source-only fue:

```txt
Wire.begin() = true
Wire.write(0x00) = 0
Wire.endTransmission() = 4
JWPLC_RTC.read() = true
```

Se corrigió `Wire.cpp` para ejecutar `allocateWireBuffer()` antes de la comprobación `i2cIsInit(num)`. No se cambia la API pública de Wire.

El fix source-only pasó TX, RX y repeated-start. Después se regeneró `libWire.a` (`precompile-work/20260810_174435`) y el archive final volvió a pasar las mismas pruebas físicas.

## Wire.end()

Se confirmó que `Wire.end()` deinitializa el HAL I2C (`Wire.getClock() after end = 0`). El bridge I2C de JWPLC detecta el bus apagado y lo reinicializa antes de sus operaciones; físicamente `JWPLC_RTC.read()` volvió a funcionar después de `Wire.end()`.

No se añadió una semántica especial de ownership a Wire en Alpha4.

## Gate físico Wire + SPI

Resultado del gate combinado sobre JWPLC Basic:

```txt
P8_WIRE_GATE=PASS
P8_SPI_GATE=PASS
P8_WIRE_SPI_GATE=PASS
```

El gate validó:

- Wire TX;
- Wire RX;
- repeated-start;
- RTC JWPLC;
- SPI;
- microSD write/read/verify/remove;
- coexistencia con el autoload normal y Display.

Arduino IDE compiló, enlazó y subió físicamente usando `libWire.a` y `libSPI.a`, con carga a 921600 y verificación de flash satisfactoria.

## Gate cross-board

Se validó compilación/link usando los archives P8 en:

```txt
jwplc_local:esp32:jwplcbasic      -> PASS
jwplc_local:esp32:jwplcbasiccore  -> PASS
jwplc_local:esp32:esp32            -> PASS
```

Durante este gate se detectó una regresión independiente: `platform.local.txt` añadía globalmente `cores/jwcontrol` para soportar el stub P2. El ESP32 genérico fallaba también con Wire/SPI source-only, descartando P8 como causa.

El aislamiento P2 por placa quedó corregido por separado en:

`d971f81 fix(build): aislar include del core P2 por placa`

Después de ese fix, los tres FQBN pasaron con P8 activo.

## Decisión

**P8 Wire + SPI queda aprobado para adopción.**

Gates cerrados:

- rendimiento A-B-B-A: PASS, **-4.985 s / -7.68 %**;
- TUs: **7 -> 5**;
- Wire TX/RX/repeated-start: PASS;
- RTC: PASS;
- recuperación JWPLC después de `Wire.end()`: PASS;
- SPI + microSD: PASS;
- Arduino IDE + upload físico: PASS;
- cross-board Basic / Basic Core / ESP32 genérico: PASS de compilación/link;
- source fallback: preservado;
- autoload normal: preservado.

Los gates generales restantes de Alpha4 siguen siendo independientes de este cierre específico.
