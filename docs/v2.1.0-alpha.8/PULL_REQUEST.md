# PR — v2.1.0-alpha.8: HMI Arduino, Display y botonera

## Resumen

Este PR cierra `v2.1.0-alpha.8` del package Arduino JWPLC Basic.

El objetivo del alpha es consolidar una HMI Arduino eficiente sobre la TFT integrada y corregir la interacción entre Display y botonera sin retirar periféricos del autoload normal ni romper las APIs públicas ya utilizadas.

## Base

Alpha8 parte del cierre publicado de Alpha7.

Branch técnico:

```text
v2.1.0-alpha.8/fix/buttons-display-autowake
```

Destino:

```text
release/v2.1.x
```

## Alcance

### Display / navegación

- `IDLE_WAKE_DISABLED` pasa a ser el comportamiento por defecto;
- wake automático sigue disponible por API explícita;
- navegación Display basada en flancos físicos propios;
- el Display deja de consumir latches `pressed()/released()` del sketch;
- entrada/salida USER absorbe correctamente estados sostenidos sin limpiar eventos de aplicación de forma destructiva;
- APIs existentes `JWPLC_Display` preservadas.

### HMI declarativa

Se incorpora un motor HMI para la pantalla USER con:

- hasta 32 campos;
- `VALUE`;
- `TEXT`;
- `BOOL`;
- `BAR`;
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

### Vistas cacheadas

Se añaden:

```cpp
JWPLC_IO
JWPLC_Time
```

Estas fachadas leen snapshots ya mantenidos por el runtime y no fuerzan transacciones físicas adicionales.

### Botonera

- se mantiene `JWPLC_Buttons` como objeto global;
- se preservan `pressed()`, `released()`, `isDown()`, repeat y event queue;
- `anyPressedOrRepeated()` vuelve a distinguir correctamente `PRESS/REPEAT` de `RELEASE`;
- el router de refresh del Display responde a cambios físicos de la matriz y no a contenido residual de la cola de eventos.

## Build speed y precompilación

Durante Alpha8 se detectó que una implementación inicial de `JWPLC_RuntimeView.cpp` añadía un TU al cold build.

Se integró la implementación dentro de `JWPLC_GlobalPeripherals.cpp`, recuperando:

```text
Basic cold = 15 compilaciones
Core cold  = 78 compilaciones
Warm       = 1 compilación
```

También se implementó lazy-link para el motor HMI.

Resultado:

```text
01_empty UI engine linked = NO
HMI gate UI engine linked = YES
```

APP vacía:

```text
399696 -> 396240 bytes
Delta  = -3456 bytes
```

Los tiempos absolutos mostraron variación significativa del host, por lo que Alpha8 no reclama una mejora global de wall-clock frente a Alpha6. La conclusión se limita a la paridad estructural de TUs/cache y al no-enlace del motor HMI cuando no se utiliza.

## Archive Display Alpha8

Candidato generado y validado:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
Bytes:  642576
SHA256: D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
Objetos: 4
```

Miembros esperados:

```text
JWPLC_Display.cpp.o
JWPLC_IdleScreen.cpp.o
JWPLC_UI.cpp.o
JWPLC_UI_API.cpp.o
```

`JW_MatrixButtons 1.0.5` no modifica su fuente en Alpha8, por lo que su archive existente no requiere regeneración por este alcance.

## Validación física

Gate:

```text
JWPLC_Display/examples/Display_Alpha8_HMI_Gate
```

Resultados principales:

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

Se verificó:

- IDLE estable sin autowake inesperado;
- RTC avanzando;
- los seis botones;
- entrada USER explícita por OK;
- navegación de páginas;
- barra UP/DOWN;
- ESC retorna IDLE y el sketch conserva su evento;
- botones disponibles en IDLE sin despertar USER cuando wake está deshabilitado;
- reentrada USER;
- valor/texto/bool/bar;
- ausencia de congelamiento visible durante el gate.

## Incidente histórico de taller

Durante un taller anterior se observaron cuelgues aparentes con comportamiento variable según laptop/placa.

El entorno exacto de esas máquinas no se conservó y no existe una reproducción controlada concluyente de todos los casos.

Por ello este PR no afirma una causa única retrospectiva.

Clasificación:

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

Alpha8 actual pasó los gates físicos dirigidos y se considera estable operacionalmente para continuar.

## Documentación

Se actualizan:

- README raíz;
- README `JWPLC_Display`;
- README `JW_MatrixButtons`;
- README `JWPLC_GlobalPeripherals`.

Se añaden:

- `ALPHA8_BUILD_SPEED_AND_LAZY_LINK.md`;
- `ALPHA8_HMI_BUTTON_VALIDATION.md`;
- `ALPHA8_CLOSURE_CHECKLIST.md`;
- `PULL_REQUEST.md`;
- `PRE_RELEASE.md`.

## Compatibilidad

Se preservan:

- Arduino IDE;
- Arduino CLI;
- `JWPLC_Display`;
- `JWPLC_Buttons`;
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
- archives precompilados ya validados que no fueron afectados por Alpha8.

## No incluido

Alpha8 no:

- integra la HMI con OpenPLC/Ladder;
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

Antes de mergear:

- [ ] archive Display final versionado con el SHA esperado;
- [ ] `git diff --check`;
- [ ] working tree limpio;
- [ ] Basic compila;
- [ ] Basic Core compila;
- [ ] ejemplo HMI Alpha8 compila;
- [ ] precompiled Display usado sin recompilar source;
- [ ] CI verde.

## Gate post-publicación

Después de publicar `v2.1.0-alpha.8`:

- [ ] instalación aislada desde `package_jwplc_index_dev.json`;
- [ ] `jwplc:esp32@2.1.0-alpha.8` instalado;
- [ ] `Used platform/library` correcto;
- [ ] compilación aislada;
- [ ] upload físico;
- [ ] boot/TFT post-upload;
- [ ] índice dev actualizado;
- [ ] topología `release/v2.1.x -> main` validada.

## Resultado técnico esperado

```text
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```
