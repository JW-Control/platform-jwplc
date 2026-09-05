# Alpha10 - Cierre técnico reabierto

## Resumen

El cierre técnico previo de Alpha10 se reabre antes de continuar con otro alpha.

La primera versión de Alpha10 añadió un guard para impedir que una copia antigua de `JWPLC_Ethernet` instalada manualmente en el sketchbook tuviera prioridad sobre la copia del package. El fix funcionó, pero su benchmark mostró un coste warm aproximado de `+5.6%` incluso usando un solo marker.

Dado que el package JWPLC administra sus propias librerías y el despliegue actual puede controlarse, Alpha10 se redefine como un ciclo de limpieza y recuperación de tiempo de compilación.

## Contrato de producto

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

No se garantiza que una copia antigua/manual de `JW_*` o `JWPLC_*` instalada en paralelo al package sea ignorada automáticamente por Arduino Builder.

Ese entorno debe corregirse eliminando/renombrando la copia conflictiva o ajustando la instalación, no añadiendo un coste permanente al autoload normal.

## Cambio técnico

Commit:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
perf(alpha10): retirar guard de shadowing de JWPLC_Ethernet
```

Cambios:

```text
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_GlobalPeripherals_Auto.h=RESTORED_TO_ALPHA9_BEHAVIOR
JWPLC_Ethernet_VERSION=1.0.0
Verify-JWPLCUnifiedEthernetSelection.ps1=REMOVED
```

No cambia:

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Protecciones heredadas auditadas

Los markers de Adafruit se conservan:

```text
JWPLC_Bundled_Adafruit_ST77xx.h=KEEP
JWPLC_Bundled_Adafruit_GFX.h=KEEP
JWPLC_Bundled_Adafruit_BusIO.h=KEEP
```

Motivo: corresponden a dependencias externas vendorizadas/precompiladas. Es normal que un usuario tenga otras versiones de Adafruit instaladas mediante Library Manager, y Alpha5 ya registró una selección real de BusIO desde sketchbook durante un gate genérico.

También se conserva `JWPLC_LIBRARY_DISCOVERY_PHASE` porque forma parte del autoload liviano y de la selección reproducible del stack Display.

Detalle: `ALPHA10_PROTECTION_AUDIT.md`.

## Periféricos

Alpha10 mantiene integrados:

- Display;
- Ethernet W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA/I/O;
- arbitraje SPI compartido.

No se obtiene el benchmark retirando periféricos.

## Decisiones heredadas sin cambio

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

OpenPLC continúa externo/opcional al runtime Arduino; este alpha no redefine esa arquitectura.

## Validación pendiente

Antes de volver a declarar Alpha10 cerrado deben existir:

- tres réplicas de benchmark del candidato;
- comparación con la evidencia histórica Alpha9/M0 y Alpha10/M1;
- matriz funcional 5/5;
- compilación desde Arduino IDE;
- validación física rápida de periféricos críticos;
- conclusión actualizada de app-only;
- conclusión actualizada de bootloader precompilado;
- decisión o pendiente explícito sobre configuración final;
- PR en español actualizado;
- PreRelease en español actualizada;
- checklist actualizado;
- publicación reemplazada y validada desde índice cuando corresponda.

## Estado actual

```text
ALPHA10_SCOPE=BUILD_SPEED_CLEANUP
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_TECHNICAL_CHANGE=COMMITTED
ALPHA10_LOCAL_BENCHMARK=PENDING
ALPHA10_FUNCTIONAL_MATRIX=PENDING_REVALIDATION
ALPHA10_PHYSICAL_VALIDATION=PENDING
ALPHA10_TECHNICAL_CLOSURE=PENDING_REVALIDATION
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_CLOSED
```
