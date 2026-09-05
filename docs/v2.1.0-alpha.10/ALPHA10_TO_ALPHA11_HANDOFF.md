# Transferencia Alpha10 -> Alpha11

Fecha de actualización: 2026-09-05.

## Regla de avance

Alpha10 ya fue publicado nuevamente y validado desde el índice dev. Antes de abrir formalmente Alpha11 sólo se exige completar la paridad final de contenido entre `release/v2.1.x` y `main`.

```text
ALPHA10_RELEASE_PUBLICATION=PASS
ALPHA10_PUBLISHED_PACKAGE_GATE=PASS
NEXT_ALPHA=BLOCKED_ONLY_BY_FINAL_TREE_PARITY
```

## Qué cerró Alpha10

Alpha10 queda acotado a limpieza de build/library discovery:

```text
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_VERSION=1.0.0
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Commit técnico principal:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

No se reintroducen guards generalizados para copias manuales de librerías JW/JWPLC salvo que aparezca un caso reproducible dentro del flujo soportado y el coste sea medido.

## Publicación final

```text
PUBLISHED_PACKAGE_SOURCE_SHA=f365738e8b0903bca9f93f5c42dfee8310e074b2
TAG=v2.1.0-alpha.10
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=24464282
SHA256=5ca5a71d6de0ddd25c81442d7ea4f840ad48603dd024afcd2925235dc4d1b0bf
PACKAGE_ROOT=2.1.0/
```

El primer ZIP de reemplazo generado con `archive_root_mode=contents` fue descartado porque Arduino CLI exige una raíz única. El workflow final usa una única carpeta `2.1.0/` y valida `boards.txt` + `platform.txt` antes de publicar.

Validación desde el package publicado:

```text
ALPHA10_PUBLISHED_EXACT_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
```

## Benchmark de referencia

```text
M0_NONE = 22.094 s
M1_ETH  = 23.327 s
M4      = 26.888 s
M7      = 30.353 s

Candidato Basic/01_empty/managed_warm_touch:
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
```

No usar un porcentaje exacto de recuperación como conclusión del alpha; se registró variación del host.

Paridad conservada:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
```

## Evidencia funcional transferida

Matriz Alpha10:

```text
DigitalIO_Basic=PASS
Buttons_Basic=PASS
Display_HMI_Fields=PASS
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
```

Gate físico:

```text
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

Ethernet y RS-485/Modbus no cambiaron de runtime en Alpha10. Se conserva su evidencia física cerrada en Alpha6/Alpha7/Alpha9 y la regresión de compilación Alpha10.

## Decisiones que Alpha11 hereda

```text
PACKAGE_MANAGED_JW_LIBRARIES=YES
MANUAL_JW_LIBRARY_SHADOWING_GUARD=NO
THIRD_PARTY_ADAFRUIT_SELECTION_GUARDS=YES
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_BIN_FINAL=NO
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
```

## Pendientes transferidos a Alpha11

- configuración de baudrate RTU desde el Backplane/UI;
- configuración de formato serial RTU y propagación hasta HAL;
- referencias tipadas de timers como `TON0.Q`, `TOF0.Q` y `TP0.Q`;
- source freeze reproducible del fork OpenPLC Editor;
- integración futura de HMI Arduino con Ladder/OpenPLC;
- prueba Remote I/O con múltiples bits simultáneos;
- continuar las mejoras de HMI/TFT sin romper la API ya validada;
- mantener como regla el modelo package-managed sin markers JW/JWPLC generalizados.

La definición exacta del alcance Alpha11 debe cerrarse al crear su branch; esta transferencia no convierte pendientes en APIs ya implementadas.

## Último pendiente de Alpha10

```text
RELEASE_MAIN_CONTENT_SYNC=PENDING
TREE_PARITY_CRITERION=REQUIRED
GIT_ANCESTRY_PARITY=NOT_REQUIRED
```

Debido a los squash merges históricos, la historia de `release/v2.1.x` y `main` puede seguir divergente. El criterio final es que ambos branches terminen con el mismo árbol/contenido después del sync controlado.

## Estado

```text
ALPHA10_TO_ALPHA11_HANDOFF=UPDATED
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_RELEASE_PUBLICATION=PASS
ALPHA10_PUBLISHED_PACKAGE_GATE=PASS
HANDOFF_EXECUTION=PENDING_ONLY_FINAL_TREE_PARITY
```
