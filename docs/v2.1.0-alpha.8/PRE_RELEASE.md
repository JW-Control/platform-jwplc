# v2.1.0-alpha.8 — HMI Arduino, Display y botonera

`v2.1.0-alpha.8` es una PreRelease técnica del package Arduino para JWPLC Basic.

Esta versión se concentra en la experiencia HMI sobre la TFT integrada: corrige la interacción entre Display y botonera, incorpora una HMI declarativa eficiente y añade vistas cacheadas de I/O/RTC para interfaces de usuario sin aumentar innecesariamente las lecturas de periféricos.

## Cambios principales

### Display / IDLE

- `IDLE_WAKE_DISABLED` pasa a ser el comportamiento por defecto;
- wake automático sigue disponible mediante configuración explícita;
- la navegación del Display usa flancos físicos propios;
- el Display ya no consume los latches `pressed()` / `released()` del sketch;
- `ESC` puede ser observado por el Display y por la aplicación;
- se preservan las APIs existentes de `JWPLC_Display`.

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

Ejemplos:

```cpp
uint8_t inputs = JWPLC_IO.inputs();
bool i0 = JWPLC_IO.input(0);

uint8_t hour = JWPLC_Time.hour();
bool rtcValid = JWPLC_Time.valid();
```

### Botonera

- `JWPLC_Buttons` continúa siendo el objeto global recomendado;
- se preservan `pressed`, `released`, `isDown`, repeat y event queue;
- el Display deja de apropiarse de latches de aplicación;
- `anyPressedOrRepeated()` distingue `PRESS/REPEAT` de `RELEASE`;
- el refresh visual se solicita por cambio físico de estado y no por eventos residuales.

## Precompilación

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

Archive Display candidato final:

```text
libJWPLC_Display.a
642576 bytes
SHA256 D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
```

`JW_MatrixButtons 1.0.5` no modifica su fuente en Alpha8, por lo que conserva su archive existente.

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
- mantenimiento del warm build de una sola compilación;
- HMI no enlazada cuando no se utiliza;
- reducción de 3456 bytes en `01_empty` frente al estado Alpha8 previo al lazy-link.

## Validación física

El ejemplo:

```text
Display_Alpha8_HMI_Gate
```

fue validado en un JWPLC Basic real.

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

Se comprobaron:

- 180 s de IDLE sin autowake inesperado;
- RTC avanzando;
- los seis botones;
- entrada USER explícita por OK;
- páginas LEFT/RIGHT;
- barra UP/DOWN;
- ESC retorna a IDLE y también llega al sketch;
- botones disponibles en IDLE sin despertar USER cuando wake está deshabilitado;
- reentrada USER;
- valores HMI de tipo numérico/texto/bool/bar;
- pulsación sostenida sin congelamiento;
- TFT estable sin flicker problemático observado.

## Incidente histórico de taller

En un taller anterior se observaron cuelgues aparentes con comportamiento variable entre laptops/placas.

No se preservó el entorno exacto de todas esas máquinas y la causa única no se reproduce de forma concluyente.

Se registra como:

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

Alpha8 actual supera los gates físicos dirigidos y se considera estable para continuar el roadmap.

## Documentación

Se actualizan:

- README raíz;
- README de `JWPLC_Display`;
- README de `JW_MatrixButtons`;
- README de `JWPLC_GlobalPeripherals`.

Y se añaden documentos dedicados de:

- build speed/lazy-link;
- validación física HMI/botonera;
- checklist de cierre;
- PR técnico;
- PreRelease.

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

- integra la HMI con OpenPLC/Ladder;
- convierte OpenPLC en dependencia obligatoria del package Arduino;
- define OTA;
- fija FlashFreq universal definitiva;
- cambia la política de bootloader;
- migra a ESP32-S3.

La integración HMI hacia OpenPLC se reserva para Alpha9.

## Canal

`v2.0.0` continúa siendo la versión estable recomendada para producción.

`v2.1.0-alpha.8` pertenece al canal dev/PreRelease y debe instalarse desde `package_jwplc_index_dev.json` una vez completada su publicación automática.

## Gate post-publicación

Antes de declarar Alpha8 totalmente cerrado se debe repetir desde el package publicado:

```text
ISOLATED_INSTALL
ISOLATED_COMPILE
USED_PLATFORM/LIBRARY
PHYSICAL_UPLOAD
POST_UPLOAD_BOOT
POST_UPLOAD_TFT
```

Estado técnico previo a publicación:

```text
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```
