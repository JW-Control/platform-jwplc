# v2.1.0-alpha.8 — HMI Arduino, Display y botonera

`v2.1.0-alpha.8` es una PreRelease técnica del package Arduino para JWPLC Basic.

Esta versión se concentra en la experiencia HMI sobre la TFT integrada: corrige la interacción entre Display y botonera, incorpora una HMI declarativa eficiente, añade vistas cacheadas de I/O/RTC y deja una serie numerada de ejemplos compactos para taller.

## Cambios principales

### Display / IDLE

- `IDLE_WAKE_DISABLED` pasa a ser el comportamiento por defecto;
- wake automático sigue disponible mediante configuración explícita;
- navegación Display usa flancos físicos propios;
- Display ya no consume los latches `pressed()` / `released()` del sketch;
- `ESC` puede ser observado por Display y aplicación;
- APIs existentes de `JWPLC_Display` preservadas.

### HMI declarativa

Se incorpora una capa HMI con:

- hasta 32 campos;
- valor numérico;
- texto;
- booleano;
- barra;
- varias páginas USER;
- formato numérico;
- colores;
- layout y alineación;
- bool text;
- rango de barra;
- dirty refresh;
- refresh periódico o bajo demanda.

API principal:

```cpp
JWPLC_Display.setFields(...);
JWPLC_Display.setValue(...);
JWPLC_Display.setText(...);
JWPLC_Display.setBool(...);
JWPLC_Display.setBar(...);
JWPLC_Display.setUserPage(...);
```

### Vistas cacheadas

Alpha8 añade:

```cpp
JWPLC_IO
JWPLC_Time
```

para leer snapshots ya mantenidos por el runtime sin forzar nuevas transacciones físicas.

### Botonera

- `JWPLC_Buttons` continúa siendo el objeto global recomendado;
- se preservan `pressed`, `released`, `isDown`, repeat y event queue;
- Display deja de apropiarse de latches de aplicación;
- `anyPressedOrRepeated()` distingue `PRESS/REPEAT` de `RELEASE`;
- refresh visual solicitado por cambios físicos de estado y no por eventos residuales.

## Precompilación / lazy-link

`JWPLC_Display` continúa como `precompiled=full`.

Alpha8 separa el motor HMI del Display base mediante lazy-link:

```text
Sketch sin HMI -> motor JWPLC_UI no enlazado
Sketch con HMI -> motor JWPLC_UI enlazado
```

Gate:

```text
EMPTY_UI_ENGINE_REFERENCES=0
EMPTY_UI_API_REFERENCES=0
HMI_UI_ENGINE_REFERENCES=175
HMI_UI_API_REFERENCES=118
```

APP del sketch vacío:

```text
399696 -> 396240 bytes
Delta  = -3456 bytes
```

Archive Display final:

```text
Archivo : libJWPLC_Display.a
Bytes   : 642576
SHA256  : D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
Commit  : 08073e5a0244d9461d3889f02d411b68f727d638
```

El gate final confirmó 0 compilaciones source de Display en Basic, Basic Core y el ejemplo HMI usando el archive final.

`JW_MatrixButtons 1.0.5` no modifica su fuente en Alpha8 y conserva su archive validado.

## Build speed

Alpha8 recupera la misma estructura de compilaciones del baseline corregido Alpha6:

```text
Basic cold = 15 TUs
Core cold  = 78 TUs
Warm       = 1 TU
```

Los tiempos absolutos mostraron variación del host entre réplicas, por lo que esta versión no reclama una mejora global de segundos frente a Alpha6.

La mejora defendible es:

- eliminación del TU adicional introducido inicialmente por RuntimeView;
- warm build de una sola compilación;
- HMI no enlazada cuando no se utiliza;
- reducción de 3456 bytes en `01_empty` frente al estado Alpha8 previo al lazy-link.

## Validación física

El ejemplo `Display_Alpha8_HMI_Gate` fue validado en un JWPLC Basic real.

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

Se comprobaron:

- 180 s de IDLE sin autowake inesperado;
- RTC avanzando;
- los seis botones;
- entrada USER explícita por OK;
- páginas LEFT/RIGHT;
- barra UP/DOWN;
- ESC retorna a IDLE y también llega al sketch;
- reentrada USER;
- valores HMI numérico/texto/bool/bar;
- pulsación sostenida sin congelamiento;
- TFT estable sin flicker problemático observado.

## Incidente histórico de taller

En un taller anterior se observaron cuelgues aparentes con comportamiento variable entre laptops/placas.

No se preservó el entorno exacto de todas esas máquinas y la causa única no se reproduce de forma concluyente. Alpha8 actual supera los gates físicos dirigidos y se considera estable para continuar el roadmap.

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

## Ejemplos numerados de taller

Se incorporan 19 ejemplos comentados con prefijo `XX.`:

```text
JWPLC_GlobalPeripherals = 6
JWPLC_Display           = 4
JWPLC_Ethernet          = 3
JWPLC_RS485             = 3
JWPLC_ModbusRTU         = 3
TOTAL                    = 19
```

Los ejemplos específicos de RTC, FRAM, microSD y botonera quedan en `JWPLC_GlobalPeripherals`, mientras que los drivers genéricos `JW_*` permanecen reutilizables y sin acoplarse al hardware JWPLC Basic.

Gate final:

```text
TOTAL=19
PASS=19
FAIL=0
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
```

## Documentación

Se actualizan README de:

- raíz del package;
- `JWPLC_Display`;
- `JW_MatrixButtons`;
- `JWPLC_GlobalPeripherals`;
- `JWPLC_Ethernet`;
- `JWPLC_RS485`;
- `JWPLC_ModbusRTU`.

Documentos Alpha8:

- `ALPHA8_BUILD_SPEED_AND_LAZY_LINK.md`;
- `ALPHA8_HMI_BUTTON_VALIDATION.md`;
- `ALPHA8_WORKSHOP_EXAMPLES.md`;
- `ALPHA8_CLOSURE_CHECKLIST.md`;
- `ALPHA8_TECHNICAL_CLOSURE.md`;
- `ALPHA8_TO_ALPHA9_OPENPLC_HANDOFF.md`;
- `PULL_REQUEST.md`;
- `PRE_RELEASE.md`.

## Compatibilidad

Se preservan:

- Arduino IDE;
- Arduino CLI;
- I/O JWPLC;
- TFT e IDLE;
- botonera;
- RTC;
- FRAM;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- mutex SPI global;
- autoload normal del package.

No se retira ningún periférico por rendimiento.

## App-only, bootloader y configuración

Alpha8 no cambia las decisiones heredadas:

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
```

No se publica un `bootloader.bin` definitivo.

## No incluido

Alpha8 no:

- integra HMI con OpenPLC/Ladder;
- convierte OpenPLC en dependencia obligatoria del package Arduino;
- define OTA;
- fija FlashFreq universal definitiva;
- cambia la política de bootloader;
- migra a ESP32-S3.

La integración HMI hacia OpenPLC se reserva para Alpha9.

## Canal

`v2.0.0` continúa siendo la versión estable recomendada para producción.

`v2.1.0-alpha.8` pertenece al canal dev/PreRelease y debe instalarse desde `package_jwplc_index_dev.json` una vez completada su publicación automática.

## Estado técnico

```text
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_STATUS=TECHNICALLY_CLOSED
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```

## Gate post-publicación

Antes de declarar Alpha8 totalmente cerrado debe repetirse desde el package publicado:

```text
ISOLATED_INSTALL
ISOLATED_COMPILE
USED_PLATFORM/LIBRARY
PHYSICAL_UPLOAD
POST_UPLOAD_BOOT
POST_UPLOAD_TFT
ARDUINO_IDE_COMPATIBILITY
```
