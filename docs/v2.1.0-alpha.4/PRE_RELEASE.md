# JWPLC ESP32 v2.1.0-alpha.4

Pre-release técnica del package Arduino **JW Control ESP32 Boards** enfocada en acelerar la compilación de JWPLC Basic sin retirar periféricos del autoload normal ni romper compatibilidad con Arduino IDE.

## Resumen

`v2.1.0-alpha.4` reduce de forma importante el costo de compilación mediante optimización de library discovery y precompilación controlada de core/librerías.

El cold final P8 observado fue de **59.901 s**, frente a **136.509 s** del baseline oficial Alpha3 instalado, una reducción aproximada de **56.12 %** en ese comparativo.

La alpha también cierra tres decisiones de configuración que estaban pendientes:

- app-only se mantiene como herramienta auxiliar, no como upload normal;
- JWPLC Basic v2.0 queda fijado en FlashFreq 40 MHz y sin `bootloader.bin` precompilado definitivo;
- JWPLC Basic adopta una partición Max App de 4 MB con 4,063,232 bytes disponibles para la aplicación y coredump conservado.

## Cambios principales

### Build speed / cache

Se consolidan las etapas D1 y P1–P8:

- optimización de library discovery;
- precompilación de librerías JWPLC seleccionadas;
- core JWPLC Basic precompilado con enlace compatible con la caché de Arduino IDE;
- Display precompilado;
- backend Ethernet W5x00 vendorizado y precompilado;
- stack Adafruit ST77xx/GFX/BusIO precompilado;
- FS + SD precompilados;
- Wire + SPI precompilados.

P4, basado en precompilar `JWPLC_GlobalPeripherals`, fue evaluado y rechazado al no aportar una mejora útil frente al estado de referencia.

### Resultado de tiempos

| Estado | Cold |
|---|---:|
| Alpha3 oficial instalado | 136.509 s |
| P6 full Adafruit stack — PC principal | 67.322 s |
| P7 FS + SD — laptop | 63.870 s |
| P8 Wire + SPI — promedio A-B-B-A | **59.901 s** |

En la comparación directa P8 del mismo host:

- source-only Wire + SPI: 64.885 s;
- precompilado P8: 59.901 s;
- mejora: 4.985 s / 7.68 %;
- TUs restantes: 5.

La tabla completa está en:

`tools/build-speed-benchmark/JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md`

## Compatibilidad con Arduino IDE

El core P2 se enlaza explícitamente mediante:

`precompiled/core/JWPLCBASIC/core.a`

Esto evita depender de reemplazar el `core.a` que Arduino IDE puede reutilizar desde su caché global.

También se aisló el include del core P2 por placa para preservar compatibilidad con otros perfiles.

La rama fue validada tanto con Arduino CLI como con Arduino IDE y se realizó validación cruzada en un segundo equipo.

## Periféricos preservados

La mejora de tiempos no elimina del autoload normal:

- Display;
- Ethernet;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / I/O.

Se conserva el mutex SPI global usado para arbitrar TFT, W5500, microSD y FRAM.

## Ethernet

El backend W5x00 queda vendorizado dentro del package JWPLC.

Durante los gates físicos se corrigió una espera de inicialización que retenía el mutex SPI global durante una pausa genérica del backend. La corrección no elimina el reset hardware del W5500 ni aumenta timeouts SPI para ocultar problemas de ownership.

Se validaron físicamente:

- DHCP / W5500;
- coexistencia Ethernet + TFT + FRAM + SD;
- HTTP continuo.

## Wire + SPI

P8 precompila Wire y SPI.

Durante la validación se identificó además un comportamiento preexistente de `Wire.begin()` cuando el HAL I2C ya se encontraba inicializado. Se corrigió la reserva de buffers internos antes del early return por `i2cIsInit()`.

## App-only

App-only queda validado como herramienta de desarrollo, pero **no se adopta como upload normal**.

El upload completo continúa siendo la ruta segura para:

- primera programación;
- reinstalación;
- cambios de bootloader;
- cambios de particiones;
- cambios de configuración de flash.

No se añade un menú público `UploadMode` en esta alpha.

## Bootloader y configuración de flash

Para JWPLC Basic v2.0 se fija:

- flash: 4 MB;
- FlashFreq: 40 MHz;
- modo de imagen: DIO;
- `build.boot=qio`;
- bootloader en `0x1000`.

El bootloader reproducible usado en los gates se genera desde:

`bootloader_qio_40m.elf`

Datos validados:

- tamaño: 25,072 bytes;
- SHA-256: `68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931`.

El `bootloader.bin` antiguo de la variante correspondía a 80 MHz y fue retirado. Alpha4 mantiene la generación normal desde el SDK y **no publica un bootloader precompilado como definitivo**.

La evaluación de 80 MHz se difiere a una revisión futura y no bloquea esta alpha.

## Partición Max App para JWPLC Basic

JWPLC Basic adopta:

`jwplc_max_app_4mb`

Layout:

| Partición | Offset | Tamaño |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| otadata | `0xE000` | `0x2000` |
| app0 | `0x10000` | `0x3E0000` |
| coredump | `0x3F0000` | `0x10000` |

Resultado:

- aplicación máxima: **4,063,232 bytes**;
- ganancia frente a `huge_app`: **917,504 bytes / 896 KiB / 29.17 %**;
- coredump: 64 KiB conservado;
- NVS: conservado;
- `otadata`: conservado;
- SPIFFS: retirado del perfil JWPLC Basic.

El package dispone de FRAM y microSD para las necesidades de persistencia previstas actualmente.

Conservar `otadata` y `ota_0` no define una política OTA para el producto.

`JWPLC Basic Core` permanece con `huge_app` y requiere una evaluación separada antes de adoptar el mismo layout.

## Validación física

El gate local final confirmó:

- Display ready;
- RTC;
- FRAM;
- microSD;
- botones UP/DOWN/LEFT/RIGHT/CANCEL/OK;
- 8 entradas digitales;
- 8 salidas/relés;
- TFT visual.

Resultado:

`ALPHA4_LOCAL_PHYSICAL_GATE=PASS`

El arranque ROM observado con la configuración final fue:

`mode:DIO, clock div:2`

Las comunicaciones se validaron en gates separados:

- Ethernet / W5500;
- HTTP;
- RS-485;
- Modbus RTU FC03;
- Modbus RTU FC06.

## Alcance explícitamente diferido

Esta alpha no:

- asume OpenPLC integrado como parte del package optimizado;
- define OTA;
- migra JWPLC Basic v2.0 a FlashFreq 80 MHz;
- migra JWPLC Basic Core a Max App;
- introduce la futura arquitectura ESP32-S3;
- inicia P9.

## Documentación técnica

La evidencia detallada se conserva en `tools/build-speed-benchmark/`, incluyendo:

- `JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md`;
- `JWPLC_ALPHA4_APP_ONLY_CONCLUSION.md`;
- `JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`;
- `JWPLC_ALPHA4_PARTITION_CONCLUSION.md`;
- `JWPLC_ALPHA4_LOCAL_PHYSICAL_GATE.md`;
- `JWPLC_ALPHA4_COMMUNICATION_PHYSICAL_GATES.md`;
- `JWPLC_ALPHA4_AUTOLOAD_CONTRACT_FINAL.md`;
- `JWPLC_ALPHA4_P7_FS_SD_RESULT.md`;
- `JWPLC_ALPHA4_P8_WIRE_SPI_RESULT.md`.

## Estado de esta PreRelease

La fase técnica de Alpha4 está cerrada y documentada. Antes de publicar la versión mediante Boards Manager todavía corresponde completar el flujo de release:

1. merge del PR hacia `release/v2.1.x`;
2. generación del ZIP de Alpha4;
3. cálculo de tamaño y SHA-256;
4. actualización del índice dev;
5. tag y GitHub PreRelease;
6. instalación y validación final desde Boards Manager.

No iniciar P9 antes de completar ese cierre de publicación.
