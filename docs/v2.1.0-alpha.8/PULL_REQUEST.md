# PR — v2.1.0-alpha.8: HMI Arduino, Display y botonera

## Resumen

Este PR cierra técnicamente `v2.1.0-alpha.8` del package Arduino JWPLC Basic.

Alpha8 consolida una HMI Arduino eficiente sobre la TFT integrada, corrige la interacción entre Display y botonera, mantiene todos los periféricos del autoload normal y añade una serie numerada de ejemplos compactos para taller.

Branch técnico:

```text
v2.1.0-alpha.8/fix/buttons-display-autowake
```

Destino:

```text
release/v2.1.x
```

## Display / navegación

- `IDLE_WAKE_DISABLED` pasa a ser el comportamiento por defecto;
- wake automático sigue disponible por API explícita;
- navegación Display basada en flancos físicos propios;
- Display deja de consumir los latches `pressed()` / `released()` del sketch;
- `ESC` puede retornar IDLE y seguir llegando a la aplicación;
- entrada/salida USER absorbe correctamente estados sostenidos;
- API pública `JWPLC_Display` preservada.

## HMI declarativa

Se incorpora una HMI USER con:

- hasta 32 campos;
- VALUE, TEXT, BOOL y BAR;
- múltiples páginas;
- formato numérico;
- colores;
- layout/alineación;
- bool text;
- rango de barra;
- dirty refresh;
- refresh bajo demanda/periódico;
- helpers `JWPLC_UI*Field`;
- setters de alto nivel en `JWPLC_Display`.

API principal:

```cpp
JWPLC_Display.setFields(...);
JWPLC_Display.setValue(...);
JWPLC_Display.setText(...);
JWPLC_Display.setBool(...);
JWPLC_Display.setBar(...);
JWPLC_Display.setUserPage(...);
```

## Vistas cacheadas

Se añaden:

```cpp
JWPLC_IO
JWPLC_Time
```

Estas fachadas leen snapshots ya mantenidos por el runtime y no fuerzan nuevas transacciones físicas de I/O o RTC.

## Botonera

- `JWPLC_Buttons` sigue siendo el objeto global recomendado;
- se preservan `pressed()`, `released()`, `isDown()`, repeat y event queue;
- `anyPressedOrRepeated()` distingue `PRESS/REPEAT` de `RELEASE`;
- el router de refresh responde a cambios físicos de la matriz, no a contenido residual de la cola.

## Build speed / lazy-link

La implementación inicial de RuntimeView añadía un TU al cold build. La implementación se reintegró en `JWPLC_GlobalPeripherals.cpp` y recuperó la estructura del baseline Alpha6:

```text
Basic cold = 15 compilaciones
Core cold  = 78 compilaciones
Warm       = 1 compilación
```

Lazy-link HMI:

```text
01_empty UI engine linked = NO
HMI gate UI engine linked = YES
```

APP vacía:

```text
399696 -> 396240 bytes
Delta  = -3456 bytes
```

Los tiempos absolutos mostraron variación significativa del host, por lo que Alpha8 no reclama una mejora global de wall-clock frente a Alpha6. La conclusión se limita a paridad estructural de TUs/cache, warm de una TU y no-enlace del motor HMI cuando no se utiliza.

## Precompilación final

`JWPLC_Display` continúa con `precompiled=full`.

Archive final:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
Bytes   : 642576
SHA256  : D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
Objetos : 4
Commit  : 08073e5a0244d9461d3889f02d411b68f727d638
```

Miembros:

```text
JWPLC_Display.cpp.o
JWPLC_IdleScreen.cpp.o
JWPLC_UI.cpp.o
JWPLC_UI_API.cpp.o
```

Gate final:

```text
Basic / 01_empty Display source TUs      = 0
Basic Core / 01_empty Display source TUs = 0
Basic / HMI Display source TUs           = 0
```

`JW_MatrixButtons 1.0.5` no modifica su fuente en Alpha8 y conserva el archive validado:

```text
Bytes  : 129506
SHA256 : 55BE8D7791DDAD79D613DBB199C10A504DE0F20CDF3330B6679A35DD64E25C81
```

## Validación física

Gate principal:

```text
JWPLC_Display/examples/Display_Alpha8_HMI_Gate
```

Resultados:

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

Se verificó IDLE estable, RTC, seis botones, entrada USER, páginas, barra, ESC compartido Display/sketch, reentrada USER y ausencia de congelamiento visible en la regresión final.

## Incidente histórico de taller

Los cuelgues aparentes observados anteriormente se registran como incidente histórico. El entorno exacto de todas las laptops/placas no se preservó, por lo que este PR no atribuye retrospectivamente todos los casos a una única causa no reproducida de forma concluyente.

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

Alpha8 actual supera los gates dirigidos y se considera estable operacionalmente.

## Ejemplos numerados de taller

Se incorporan 19 ejemplos compactos, comentados y numerados con prefijo `XX.`.

Distribución:

```text
JWPLC_GlobalPeripherals = 6
JWPLC_Display           = 4
JWPLC_Ethernet          = 3
JWPLC_RS485             = 3
JWPLC_ModbusRTU         = 3
TOTAL                    = 19
```

Los ejemplos específicos de RTC, FRAM, microSD y botonera se ubican en `JWPLC_GlobalPeripherals`; no se contaminan los drivers genéricos `JW_*` con dependencias específicas del JWPLC Basic.

Gate de compilación:

```text
TOTAL=19
PASS=19
FAIL=0
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
```

## Documentación

Se actualizan:

- README raíz;
- README `JWPLC_Display`;
- README `JW_MatrixButtons`;
- README `JWPLC_GlobalPeripherals`;
- README `JWPLC_Ethernet`;
- README `JWPLC_RS485`;
- README `JWPLC_ModbusRTU`.

Se añaden:

- `ALPHA8_BUILD_SPEED_AND_LAZY_LINK.md`;
- `ALPHA8_HMI_BUTTON_VALIDATION.md`;
- `ALPHA8_WORKSHOP_EXAMPLES.md`;
- `ALPHA8_CLOSURE_CHECKLIST.md`;
- `ALPHA8_TECHNICAL_CLOSURE.md`;
- `ALPHA8_TO_ALPHA9_OPENPLC_HANDOFF.md`;
- `PULL_REQUEST.md`;
- `PRE_RELEASE.md`.

## Compatibilidad preservada

- Arduino IDE / Arduino CLI;
- `JWPLC_Display`;
- `JWPLC_Buttons`;
- I/O JWPLC;
- TFT IDLE/USER;
- Ethernet/W5500;
- RTC;
- FRAM;
- microSD;
- RS-485;
- Modbus RTU;
- TCA/I/O;
- mutex SPI global;
- core precompilado vigente;
- autoload normal del package.

## Fuera de alcance

Alpha8 no:

- integra HMI con OpenPLC/Ladder;
- convierte OpenPLC en dependencia del runtime Arduino;
- define OTA;
- fija una FlashFreq universal definitiva;
- adopta un `bootloader.bin` definitivo;
- cambia la decisión de app-only;
- migra a ESP32-S3;
- elimina periféricos por velocidad.

La integración HMI -> OpenPLC queda para Alpha9.

## Decisiones heredadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
```

## Gates de merge

Antes del PR quedaron aprobados:

- [x] archive Display final versionado con SHA esperado;
- [x] `git diff --check`;
- [x] working tree limpio;
- [x] Basic compila;
- [x] Basic Core compila;
- [x] HMI Alpha8 compila usando Display precompilado;
- [x] 19/19 ejemplos de taller compilan;
- [ ] CI del PR verde.

## Gate post-publicación

Después de publicar `v2.1.0-alpha.8`:

- [ ] instalación aislada desde `package_jwplc_index_dev.json`;
- [ ] `jwplc:esp32@2.1.0-alpha.8` instalado;
- [ ] `Used platform/library` correcto;
- [ ] compilación aislada;
- [ ] upload físico;
- [ ] boot/TFT post-upload;
- [ ] compatibilidad Arduino IDE del package publicado;
- [ ] índice dev actualizado;
- [ ] topología `release/v2.1.x -> main` validada.

## Resultado técnico

```text
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_STATUS=TECHNICALLY_CLOSED
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```