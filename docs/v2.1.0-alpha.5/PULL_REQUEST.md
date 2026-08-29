# Pull Request — JWPLC v2.1.0-alpha.5

## Título sugerido

`perf(alpha5): recuperar precompilados compatibles y normalizar core JWPLC Basic`

## Rama

- Base: `release/v2.1.x`
- Head: `v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

## Objetivo

Cerrar `v2.1.0-alpha.5` corrigiendo la compatibilidad de las librerías
precompiladas ESP32 del package JWPLC y recuperando rendimiento de
compilación sin retirar periféricos del autoload normal ni romper APIs
ya validadas.

Alpha5 prioriza:

1. estabilidad;
2. compatibilidad Arduino IDE;
3. reutilización segura de precompilados;
4. ausencia de shims genéricos SPI/I2C;
5. una única fuente funcional para el core JWPLC;
6. documentación explícita de decisiones y fallbacks.

---

## Resultado principal

El benchmark final de JWPLC Basic fue:

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 54.594 s | 8 |
| managed warm no-change | 24.804 s | 1 |
| managed warm touch | 23.760 s | 1 |
| explicit cold | **55.387 s** | **8** |
| explicit warm no-change | 22.462 s | 1 |
| explicit warm touch | 22.219 s | 1 |

JWPLC Basic Core, utilizado como control source-core:

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 60.717 s | 71 |
| explicit cold | 58.617 s | 71 |

Resultado:

`ALPHA5_FINAL_BENCHMARK=PASS`

---

## Comparación Alpha4 vs Alpha5

La comparación principal se realiza en el mismo PC y con metodología
cold comparable:

| Estado | Tiempo | TUs |
|---|---:|---:|
| Alpha4 P6 | 67.322 s | 12 |
| Alpha5 final | **55.387 s** | **8** |

Alpha5 representa:

- 11.935 s menos;
- 17.73 % menos tiempo;
- 12 -> 8 TUs;
- 33.33 % menos TUs compiladas.

Resultado:

`ALPHA5_VS_ALPHA4_SAME_HOST=IMPROVED`

El P8 de Alpha4, con 59.901 s / 5 TUs, se conserva como referencia
histórica pero no se utiliza como comparación causal directa porque fue
medido en otro host.

Documento:

`docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md`

---

## Recuperación de precompilados

Alpha5 termina con:

`PRECOMPILED_RECOVERED_TUS=21/24`

`SOURCE_FALLBACK_TUS=3/24`

Los únicos fallbacks deliberados son:

- `JWPLC_Display`: 2 TUs;
- `JW_RTC`: 1 TU.

Estos fallbacks se conservan para evitar introducir bridges o shims
genéricos del runtime sólo por rendimiento.

---

## Bridge GPIO genérico

Para permitir que archives ESP32 sean reutilizables tanto por JWPLC Basic
como por ESP32 genérico se incorporó un bridge limitado exclusivamente a:

- `jwplc_pinMode`;
- `jwplc_digitalWrite`;
- `jwplc_digitalRead`.

Implementación:

`JWPLC/2.1.0/cores/esp32/jwplc-gpio-compat.c`

No se permiten:

- bridges SPI genéricos;
- bridges I2C genéricos;
- wrappers generales del runtime;
- dependencias `jwplc...` adicionales en archives reutilizables.

La auditoría global final revisó 12 archives y encontró:

`BLOCKING_JWPLC_SYMBOLS=0`

Resultado:

`ALPHA5_PRECOMPILED_GLOBAL_AUDIT=PASS`

`GENERIC_GPIO_BRIDGE_POLICY=ENFORCED`

Documento:

`docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`

---

## JW_MatrixButtons

`JW_MatrixButtons` fue el último candidato recuperado de forma segura.

Se validó:

- generación del archive;
- ABI;
- Generic ESP32;
- JWPLC Basic;
- JWPLC Basic Core;
- funcionamiento físico de los seis botones;
- uso desde Arduino IDE.

Archive:

`JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a`

SHA-256:

`55be8d7791ddad79d613dbb199c10a504de0f20cdf3330b6679a35dd64e25c81`

Resultado:

`JW_MATRIXBUTTONS_PRECOMPILED=ADOPTED`

---

## Normalización del core

La fuente funcional canónica queda en:

`JWPLC/2.1.0/cores/jwcontrol/`

El perfil normal de JWPLC Basic usa:

`JWPLC/2.1.0/cores/jwcontrol_precompiled_stub/`

junto con:

`JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a`

Reglas adoptadas:

- los cambios funcionales se realizan únicamente en `jwcontrol`;
- el stub no contiene lógica funcional;
- `jwplcbasic` usa stub + archive;
- `jwplcbasiccore` compila el core desde fuente;
- se elimina `jwcontrol_p2` como arquitectura funcional paralela.

SHA-256 del core.a validado:

`7c2c0149fc19e5f363a46d49c9a805db268b2ab72ae0a4ebf2b679e781a2d669`

Reproducibilidad:

- 62/64 objetos determinísticos byte a byte;
- 2 objetos dependen de `__DATE__/__TIME__`;
- reproducibilidad semántica: PASS.

---

## Display y contención SPI

Durante Alpha5 se detectó una contención real durante el arranque del TFT.

La inicialización física del ST7789 se ejecutaba dentro del callback del
Display manteniendo el mutex SPI durante los delays obligatorios del panel.

Se adoptó:

- inicializar TFT desde `initPeripherals()` antes de `setup()`;
- mantener los delays del ST7789;
- mantener el mutex SPI;
- hacer idempotente el callback posterior mediante `g_tftReady`.

No se modificaron:

- frecuencias SPI;
- timeout de FRAM;
- timeout de SD;
- APIs públicas;
- lógica de arbitraje SPI.

Validación física:

- Ethernet sin Link: PASS;
- Ethernet con IP estática: PASS;
- Ethernet con DHCP: PASS;
- FRAM: PASS;
- microSD: PASS;
- redraw TFT: PASS;
- timeouts SPI Ethernet: 0.

Resultados:

`ALPHA5_SPI_STARTUP_COEXISTENCE=PASS`

`ALPHA5_TFT_PRESETUP=ADOPTED`

---

## Autoload preservado

Alpha5 conserva en el flujo normal:

- Display;
- Ethernet / W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / entradas y salidas.

También se conserva el mutex SPI global.

No se retiró ningún periférico para obtener los resultados de rendimiento.

---

## Arduino CLI y Arduino IDE

Se validó:

- JWPLC Basic;
- JWPLC Basic Core;
- Generic ESP32;
- Arduino CLI;
- Arduino IDE 2.x.

El gate integral heredado:

`tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate`

compila correctamente sobre el estado final Alpha5.

Desde Arduino IDE se confirmó:

- FQBN `jwplc_local:esp32:jwplcbasic`;
- core `jwcontrol_precompiled_stub`;
- partición `jwplc_max_app_4mb`;
- generación automática del bootloader;
- enlace del core precompilado;
- uso de los archives precompilados.

Resultado del sketch:

- APP: 416353 bytes / 10 %;
- máximo APP: 4063232 bytes;
- RAM global: 27660 bytes / 8 %.

Resultado:

`ALPHA5_ARDUINO_IDE_COMPILE=PASS`

---

## Validación física

Alpha5 cuenta con gates físicos específicos sobre las áreas modificadas:

- JW_MatrixButtons;
- microSD;
- GFX/TFT;
- Display pre-setup;
- Ethernet W5500;
- Ethernet Link OFF/ON;
- IP estática;
- DHCP;
- FRAM;
- coexistencia SPI;
- redraw TFT.

Las funciones no modificadas conservan adicionalmente la evidencia física
de Alpha4 para:

- RTC;
- 8 DI;
- 8 DO;
- RS-485;
- Modbus RTU FC03;
- Modbus RTU FC06.

No se repitió el gate físico integral sobre el HEAD documental final por
no disponer de un JWPLC Basic durante el cierre.

Esto no se considera bloqueante para preparar el PR porque:

1. las áreas funcionalmente modificadas sí fueron validadas físicamente;
2. desde la última modificación funcional sólo hubo documentación;
3. el gate integral compila sobre el estado final.

Se recomienda repetirlo durante la validación de publicación cuando exista
hardware disponible.

---

## Laptop de validación

Alpha5 fue caracterizada también en una laptop con Arduino CLI 1.5.1.

Comparación controlada con perfil Equilibrado:

| JWPLC Basic | AC | Batería |
|---|---:|---:|
| managed cold | 66.475 s | 87.032 s |
| explicit cold | 76.694 s | 87.533 s |
| explicit warm no-change | 8.630 s | 11.286 s |
| explicit warm touch | 8.749 s | 11.556 s |

Se confirmó que:

- la política energética afecta materialmente el rendimiento;
- el número de TUs de Basic permanece en 8;
- los benchmarks formales de laptop deben realizarse con cargador conectado.

Documento:

`docs/v2.1.0-alpha.5/LAPTOP_POWER_PROFILE_BENCHMARK_20260824.md`

---

## App-only

Alpha5 conserva la decisión previamente validada:

- app-only funciona como herramienta auxiliar de desarrollo;
- no se adopta como upload normal;
- full upload sigue siendo la ruta segura y por defecto;
- no se añade un menú público `UploadMode`.

`platform.txt` no cambió respecto al estado que sustentó esta decisión.

Resultado:

`APP_ONLY=VALIDATED_DEVELOPMENT_TOOL`

`APP_ONLY_DEFAULT_UPLOAD=NO`

---

## Bootloader

El bootloader precompilado continúa:

`BOOTLOADER_PRECOMPILED=NOT_ADOPTED`

La generación normal permanece:

`BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC`

Alpha5 no publica un `bootloader.bin` definitivo.

La validación de Arduino IDE confirma la generación desde:

`bootloader_qio_40m.elf`

---

## Partición y perfil actual

JWPLC Basic conserva:

- ESP32;
- CPU 240 MHz;
- flash 4 MB;
- FlashFreq actual 40 MHz;
- imagen DIO;
- `build.boot=qio`;
- bootloader `0x1000`;
- upload 921600;
- partición `jwplc_max_app_4mb`;
- APP máxima 4063232 bytes;
- coredump 64 KiB.

Este perfil se mantiene como configuración actualmente validada.

No se declara como configuración universal para futuras revisiones.

Resultados:

`CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE`

`FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING`

---

## Decisiones que NO toma este PR

Este PR no:

- integra OpenPLC como parte obligatoria del package;
- define una política OTA;
- migra a FlashFreq 80 MHz;
- publica un bootloader.bin definitivo;
- migra a ESP32-S3;
- elimina periféricos por velocidad;
- introduce bridges SPI/I2C genéricos;
- rompe APIs ya validadas.

---

## Documentación de cierre

- `docs/v2.1.0-alpha.5/ALPHA5_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md`
- `docs/v2.1.0-alpha.5/LAPTOP_POWER_PROFILE_BENCHMARK_20260824.md`
- `docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`
- `docs/v2.1.0-alpha.5/UPLOAD_BOOTLOADER_CONFIGURATION_CONCLUSION_20260824.md`
- `docs/v2.1.0-alpha.5/CORE_PRECOMPILED_REPRODUCIBILITY_20260824.md`
- `docs/v2.1.0-alpha.5/CORE_PRECOMPILED_STUB_NORMALIZATION_20260824.md`
- `docs/v2.1.0-alpha.5/GENERIC_GPIO_BRIDGE_VALIDATION_20260823.md`
- `docs/v2.1.0-alpha.5/PHYSICAL_COMPATIBILITY_VALIDATION_20260823.md`
- `docs/v2.1.0-alpha.5/SPI_STARTUP_TFT_PRESETUP_20260824.md`

---

## Checklist para merge

- [x] Benchmark final documentado.
- [x] Alpha4 vs Alpha5 documentado.
- [x] Segundo host validado.
- [x] Auditoría global final aprobada.
- [x] Bridge GPIO limitado y auditado.
- [x] Core precompilado normalizado.
- [x] MatrixButtons adoptado y validado físicamente.
- [x] Coexistencia SPI aprobada.
- [x] Arduino CLI validado.
- [x] Arduino IDE validado.
- [x] Autoload normal preservado.
- [x] App-only cerrado.
- [x] Bootloader precompilado cerrado.
- [x] Configuración actual/pending documentada.
- [ ] Revisión final del diff contra `release/v2.1.x`.
- [x] CI del PR aprobado.

---

## Después del merge

El flujo de publicación continúa con:

1. generación del ZIP de `v2.1.0-alpha.5`;
2. cálculo de tamaño y SHA-256;
3. actualización del índice dev;
4. creación del tag;
5. creación del GitHub PreRelease;
6. adjuntar ZIP;
7. instalación mediante Boards Manager;
8. compilación desde la instalación publicada;
9. upload desde la instalación publicada;
10. repetir el gate físico integral cuando haya hardware disponible.

No avanzar a otra alpha antes de completar el cierre de publicación.
