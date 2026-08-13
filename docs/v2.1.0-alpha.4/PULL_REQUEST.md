# Pull Request — JWPLC v2.1.0-alpha.4

## Título sugerido

`perf(alpha4): optimizar compilación y cerrar configuración de JWPLC Basic`

## Rama

- Base: `release/v2.1.x`
- Head: `v2.1.0-alpha.4/feature/build-speed-cache`

## Objetivo

Cerrar `v2.1.0-alpha.4` con una reducción significativa del tiempo de compilación de JWPLC Basic sin retirar periféricos del autoload normal, manteniendo compatibilidad con Arduino IDE y preservando las APIs ya validadas.

Esta alpha también cierra explícitamente los pendientes técnicos de app-only, bootloader/configuración de flash y particionado de la flash de 4 MB.

## Resultado de rendimiento

La tabla formal se encuentra en:

`tools/build-speed-benchmark/JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md`

Resultados principales:

| Estado | Cold |
|---|---:|
| Alpha3 oficial instalado | 136.509 s |
| Local pre-D1 | 148.649 s |
| D1 | 121.732 s |
| P1 | 105.940 s |
| P6 stack Adafruit completo — PC principal | 67.322 s |
| P7 FS + SD — laptop | 63.870 s |
| P8 Wire + SPI — promedio A-B-B-A | **59.901 s** |

Frente al baseline oficial Alpha3, el resultado P8 representa aproximadamente:

- **76.608 s menos**;
- **56.12 % de reducción** del cold observado.

En P8, la comparación controlada Wire + SPI source-only vs precompilado dio:

- source-only: 64.885 s promedio;
- P8: 59.901 s promedio;
- mejora: 4.985 s / 7.68 %;
- TUs restantes: 5.

## Cambios principales

### Discovery y precompilación

Se redujo trabajo repetitivo del build mediante precompilación controlada, conservando source fallback cuando corresponde.

La evolución incluye:

- D1: optimización de library discovery;
- P1: librerías JWPLC precompiladas;
- P2: core JWPLC Basic precompilado con enlace explícito compatible con la caché de Arduino IDE;
- P3: Display;
- P5: backend Ethernet W5x00 vendorizado;
- P6: Adafruit ST77xx + GFX + BusIO;
- P7: FS + SD;
- P8: Wire + SPI.

P4 (`JWPLC_GlobalPeripherals` precompilado) fue evaluado y rechazado al no aportar mejora suficiente.

### Compatibilidad Arduino IDE

P2 se ajustó para no depender de sustituir el `core.a` cacheado por Arduino IDE. El linker recibe explícitamente:

`precompiled/core/JWPLCBASIC/core.a`

mientras el IDE puede conservar su core stub cacheado.

También se aisló el include de P2 por placa para no contaminar otros perfiles.

### Ethernet

Se vendorizó el backend W5x00 y se mantuvo dentro del package JWPLC.

Durante los gates físicos se corrigió una espera genérica dentro de la inicialización Ethernet que retenía el mutex SPI global y podía bloquear temporalmente FRAM/SD. La corrección conserva el hardware reset del W5500 y no incrementa timeouts para ocultar ownership del bus.

### Wire

Durante P8 se detectó un caso preexistente en `Wire.begin()` cuando el HAL I2C ya estaba inicializado. Se corrigió reservando los buffers internos antes del early return por `i2cIsInit()`.

## Autoload preservado

La optimización no elimina del uso normal:

- Display;
- Ethernet;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / I/O.

El mutex SPI global se conserva.

## App-only

Documento:

`tools/build-speed-benchmark/JWPLC_ALPHA4_APP_ONLY_CONCLUSION.md`

Decisión:

- app-only funciona y puede ser útil durante desarrollo;
- no se adopta como upload normal por defecto;
- el upload full continúa siendo el camino seguro para Arduino IDE, primera programación y cambios de bootloader/particiones/configuración de flash;
- no se añade `UploadMode` público en Alpha4.

## Bootloader y FlashFreq

Documento:

`tools/build-speed-benchmark/JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`

Se cierra la configuración de JWPLC Basic v2.0 en:

- ESP32;
- flash 4 MB;
- FlashFreq 40 MHz;
- DIO;
- `build.boot=qio`;
- bootloader en `0x1000`.

El bootloader reproducible generado desde `bootloader_qio_40m.elf` tiene:

- tamaño: 25,072 bytes;
- SHA-256: `68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931`.

Se detectó que el `bootloader.bin` antiguo de la variante correspondía a 80 MHz. Se retiró del package y se mantiene la generación normal desde el SDK.

No se publica un `bootloader.bin` definitivo en esta alpha.

## Particionado Max App

Documento:

`tools/build-speed-benchmark/JWPLC_ALPHA4_PARTITION_CONCLUSION.md`

JWPLC Basic adopta:

`JWPLC/2.1.0/tools/partitions/jwplc_max_app_4mb.csv`

Layout principal:

| Partición | Offset | Tamaño |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| otadata | `0xE000` | `0x2000` |
| app0 | `0x10000` | `0x3E0000` |
| coredump | `0x3F0000` | `0x10000` |

Resultado:

- APP máxima: **4,063,232 bytes**;
- ganancia frente a `huge_app`: **917,504 bytes / 896 KiB / 29.17 %**;
- coredump conservado;
- NVS conservado;
- `otadata` conservado;
- SPIFFS retirado del perfil JWPLC Basic.

`JWPLC Basic Core` permanece con `huge_app`; no se extrapola automáticamente esta decisión.

## Gates físicos

### Gate local

Se validó físicamente:

- Display;
- RTC;
- FRAM;
- microSD;
- botones UP/DOWN/LEFT/RIGHT/CANCEL/OK;
- 8 entradas digitales;
- 8 salidas/relés;
- TFT visual.

Resultado final:

`ALPHA4_LOCAL_PHYSICAL_GATE=PASS`

El arranque final mostró:

`mode:DIO, clock div:2`

### Comunicaciones

Se validaron por separado:

- W5500 / DHCP;
- coexistencia Ethernet + TFT + FRAM + SD;
- HTTP físico;
- RS-485 físico;
- Modbus RTU FC03;
- Modbus RTU FC06.

Documento:

`tools/build-speed-benchmark/JWPLC_ALPHA4_COMMUNICATION_PHYSICAL_GATES.md`

## Validación cruzada

Se comprobó el package optimizado en un segundo equipo con Arduino IDE, incluyendo compilación, caché, enlace, subida física y funcionamiento de periféricos.

La comparación P7/P8 en la laptop se realizó conectada al cargador para evitar sesgo por throttling en batería.

## Decisiones que NO toma este PR

Este PR no:

- integra OpenPLC como parte obligatoria del package;
- define OTA;
- migra JWPLC Basic v2.0 a FlashFreq 80 MHz;
- migra JWPLC Basic Core a Max App;
- introduce arquitectura ESP32-S3;
- elimina periféricos por velocidad;
- inicia P9.

## Documentación de cierre

- `tools/build-speed-benchmark/JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_APP_ONLY_CONCLUSION.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_PARTITION_CONCLUSION.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_LOCAL_PHYSICAL_GATE.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_COMMUNICATION_PHYSICAL_GATES.md`
- `docs/v2.1.0-alpha.4/ALPHA4_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.4/PRE_RELEASE.md`

## Checklist para merge

- [x] Tabla final de tiempos documentada.
- [x] App-only cerrado.
- [x] Bootloader precompilado cerrado.
- [x] FlashFreq final para v2.0 cerrada en 40 MHz.
- [x] Particionado JWPLC Basic cerrado.
- [x] Arduino CLI validado.
- [x] Arduino IDE validado.
- [x] Segundo host validado.
- [x] Gate físico local aprobado.
- [x] Ethernet aprobado.
- [x] RS-485 aprobado.
- [x] Modbus RTU aprobado.
- [x] Autoload normal preservado.
- [ ] CI del PR aprobado.
- [ ] Revisión final del diff contra `release/v2.1.x`.

## Después del merge

El cierre de publicación continúa con ZIP, checksum, índice dev, tag, GitHub PreRelease y validación mediante Boards Manager. No iniciar P9 antes de completar esa publicación.
