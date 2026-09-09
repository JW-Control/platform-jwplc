# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.11 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es mantener una experiencia cercana a Arduino, con las E/S industriales y periféricos del JWPLC integrados al runtime del package: TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485, Modbus RTU y TCA/I/O.

---

## Estado actual

| Canal | Versión | Estado |
|---|---|---|
| Estable | `v2.0.0` | Release pública estable. |
| Dev / PreRelease | `v2.1.0-alpha.11` | **Publicada, instalada, compilada, subida y validada en hardware real.** |

```text
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
ALPHA11_STATUS=CLOSED_PUBLISHED
```

---

## Índices de Boards Manager

| Canal | Archivo | Estado |
|---|---|---|
| Dev / PreRelease | `JWPLC/package_jwplc_index_dev.json` | `v2.1.0-alpha.11` |
| Estable | `JWPLC/package_jwplc_index.json` | `v2.0.0` |

URL dev:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

URL estable:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

El índice estable no cambia en Alpha11.

---

## JWPLC Basic

FQBN recomendado:

```text
jwplc:esp32:jwplcbasic
```

Periféricos del perfil completo:

- 8 entradas digitales industriales;
- 8 salidas por relé;
- TCA6424A / I/O industrial;
- TFT ST7789 320×170;
- botonera frontal de 6 teclas;
- RTC;
- FRAM 8 KiB;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- arbitraje del SPI compartido.

No se retiran periféricos del autoload normal sólo para reducir tiempos de compilación.

---

## Alpha11 — JWPLC HMI Designer V1

Alpha11 incorpora el primer cierre completo de **JWPLC HMI Designer V1** para la TFT integrada del JWPLC Basic.

```text
HMI_DESIGNER_V1=PASS_USER
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE=PASS
CODEGEN=PASS
```

El Designer genera:

```text
JWPLC_HMI_Generated.h
```

Proyecto recomendado:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

PixelMap soporta RGB565, capas, brush/eraser, fill, eyedropper, línea, rectángulo, undo/redo, visibilidad runtime y codegen optimizado `RGB565_RUN` / `PACKED_SPAN16`.

LIVE Preview usa Web Serial con transporte validado físicamente en Alpha11.

---

## Arduino IDE 2.3.4

Integración experimental sin fork ni parche del IDE:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Incluye:

- comando `JWPLC: Abrir HMI Designer`;
- botón `JW HMI`;
- integración best-effort en el editor;
- autocompletado contextual de `JWPLC_Display`.

El autocompletado muestra una API curada y propone valores válidos dentro de setters como `IDLE_WAKE_*`, `IDLE_RETURN_*`, `USER_REFRESH_*` y `BTN_*`. Los getters/aliases compatibles continúan disponibles, pero no se priorizan cuando pueden confundir al usuario.

```text
ARDUINO_IDE_2_3_4=PASS_USER
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

---

## Robustez de runtime

Alpha11 cierra dos regresiones importantes:

1. la botonera no requiere pausas artificiales para que `pressed()`, `released()` e `isDown()` funcionen en loops intensivos;
2. `digitalWrite(Q0_0, condicion)` puede ejecutarse continuamente sin congelar RTC/Display ni requerir `delay(1)`.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

---

## Precompilación validada

Core JWPLC Basic:

```text
ARCHIVE=JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

Display:

```text
ARCHIVE=JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
```

---

## Benchmark Alpha11

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

No se reclama un porcentaje global exacto de aceleración debido a variación del host. El beneficio del Display precompilado se acepta por evitar recompilar sus TUs manteniendo paridad source/archive.

---

## PreRelease publicada

```text
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
```

La versión publicada se instaló en un entorno Arduino CLI aislado bajo `%TEMP%`, se verificaron los hashes de `core.a` y `libJWPLC_Display.a`, se compiló y se subió a hardware real. TFT, RTC y Q0 continuaron operativos sin `delay(1)`.

---

## Modelo de librerías y decisiones vigentes

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

No se publica `bootloader.bin` como definitivo. No se fija una FlashFreq universal futura. OpenPLC continúa fuera del autoload Arduino normal.

---

## Documentación Alpha11

- `docs/v2.1.0-alpha.11/ALPHA11_STATUS.md`
- `docs/v2.1.0-alpha.11/ALPHA11_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md`
- `docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md`
- `docs/v2.1.0-alpha.11/ALPHA11_PUBLISHED_VALIDATION.md`
- `docs/v2.1.0-alpha.11/PRE_RELEASE.md`
- `JWPLC/2.1.0/libraries/JWPLC_Display/README.md`
- `tools/jwplc-hmi-designer/README.md`

---

## Sincronización de ramas

Por los squash merges históricos, `main` y `release/v2.1.x` pueden mostrar contadores `ahead/behind` distintos aun cuando su contenido sea idéntico. El criterio de cierre es **paridad exacta de árbol/contenido**, no igualdad de ancestría.

```text
TREE_PARITY_CRITERION=REQUIRED
GIT_ANCESTRY_PARITY=NOT_REQUIRED
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA11_STATUS=CLOSED_PUBLISHED
NEXT_ALPHA=UNBLOCKED
```
