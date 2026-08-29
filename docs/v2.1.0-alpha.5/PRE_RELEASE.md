# JWPLC ESP32 v2.1.0-alpha.5

Pre-release técnica del package Arduino **JW Control ESP32 Boards**
enfocada en recuperar precompilación segura y compatibilidad Arduino IDE
manteniendo todos los periféricos integrados del JWPLC Basic.

## Resumen

`v2.1.0-alpha.5` corrige los problemas de compatibilidad detectados al usar
librerías precompiladas ESP32 con el perfil JWPLC Basic y normaliza la
arquitectura del core precompilado.

La alpha termina recuperando de forma segura:

`21/24 TUs`

de las candidatas evaluadas.

Se mantienen deliberadamente desde fuente:

- `JWPLC_Display`: 2 TUs;
- `JW_RTC`: 1 TU.

La prioridad fue preservar estabilidad y compatibilidad antes que introducir
shims genéricos sólo para reducir tiempo de build.

---

## Rendimiento

Benchmark final de JWPLC Basic:

| Fase | Tiempo |
|---|---:|
| managed cold | 54.594 s |
| managed warm no-change | 24.804 s |
| managed warm touch | 23.760 s |
| explicit cold | **55.387 s** |
| explicit warm no-change | 22.462 s |
| explicit warm touch | 22.219 s |

En el cold explícito Alpha5 compila únicamente:

`8 TUs`

La comparación principal contra Alpha4 en el mismo PC es:

| Estado | Tiempo | TUs |
|---|---:|---:|
| Alpha4 P6 | 67.322 s | 12 |
| Alpha5 final | **55.387 s** | **8** |

Resultado:

- 11.935 s menos;
- 17.73 % de reducción;
- 33.33 % menos TUs compiladas.

El resultado Alpha4 P8 de 59.901 s / 5 TUs fue obtenido en otro equipo y
se conserva sólo como referencia histórica.

---

## Arquitectura del core

Alpha5 deja una única fuente funcional del core:

`JWPLC/2.1.0/cores/jwcontrol/`

JWPLC Basic normal utiliza un core stub:

`cores/jwcontrol_precompiled_stub`

junto al archive:

`precompiled/core/JWPLCBASIC/core.a`

JWPLC Basic Core continúa compilando desde la fuente canónica.

Esto evita mantener una segunda copia funcional del core.

---

## Compatibilidad de librerías

Se añadió una auditoría global de dependencias externas `jwplc...`.

Los archives ESP32 pueden depender únicamente de estos tres símbolos
bridge-compatible:

- `jwplc_pinMode`;
- `jwplc_digitalWrite`;
- `jwplc_digitalRead`.

Cualquier otra dependencia externa `jwplc...` se considera bloqueante.

Resultado final:

- archives auditados: 12;
- bloqueantes: 0;
- estado: `PASS BRIDGE-COMPATIBLE`.

No se introducen bridges genéricos SPI o I2C.

---

## JW_MatrixButtons

`JW_MatrixButtons` vuelve a utilizar precompilación ESP32 después de validar:

- Generic ESP32;
- JWPLC Basic;
- JWPLC Basic Core;
- ABI;
- bridge GPIO;
- seis botones físicos;
- compilación desde Arduino IDE.

Resultado:

`JW_MATRIXBUTTONS_PRECOMPILED=ADOPTED`

---

## Display y bus SPI

Durante la validación se detectó que la inicialización tardía del TFT podía
mantener ocupado el mutex SPI durante los delays obligatorios del ST7789.

Alpha5 mueve la inicialización física del TFT antes de `setup()` y evita
repetirla posteriormente.

Se conserva:

- mutex SPI;
- exclusión de bus;
- delays requeridos por el panel;
- APIs públicas;
- timeouts existentes.

Se validó físicamente la coexistencia de:

- TFT;
- W5500;
- FRAM;
- microSD.

Condiciones verificadas:

- Ethernet sin Link;
- Ethernet con IP estática;
- Ethernet con DHCP.

Resultado:

`ALPHA5_SPI_STARTUP_COEXISTENCE=PASS`

---

## Periféricos preservados

Alpha5 mantiene en el autoload normal:

- Display;
- Ethernet / W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / entradas y salidas.

No se retiró ningún periférico para acelerar la compilación.

---

## Arduino IDE y Arduino CLI

La configuración final fue validada con:

- Arduino CLI;
- Arduino IDE 2.x;
- JWPLC Basic;
- JWPLC Basic Core;
- ESP32 genérico en gates de compatibilidad.

Desde Arduino IDE se confirmó el uso de:

- `jwcontrol_precompiled_stub`;
- `precompiled/core/JWPLCBASIC/core.a`;
- `jwplc_max_app_4mb`;
- archives ESP32 precompilados;
- generación normal del bootloader desde el SDK.

El gate integral compila con:

- APP: 416353 bytes / 10 %;
- RAM global: 27660 bytes / 8 %.

---

## Validación en laptop

Se realizó una validación adicional en una laptop con Arduino CLI 1.5.1.

Con perfil Equilibrado:

| JWPLC Basic | AC | Batería |
|---|---:|---:|
| managed cold | 66.475 s | 87.032 s |
| explicit cold | 76.694 s | 87.533 s |
| explicit warm touch | 8.749 s | 11.556 s |

La prueba confirma que el perfil energético del host puede afectar
significativamente el tiempo observado.

Para benchmarks formales se recomienda usar la laptop conectada al cargador.

---

## App-only

App-only permanece disponible conceptualmente como herramienta auxiliar de
desarrollo, pero **no se adopta como upload normal por defecto**.

El upload completo sigue siendo la ruta recomendada para:

- primera programación;
- reinstalación;
- cambios de bootloader;
- cambios de particiones;
- cambios de configuración de flash.

No se añade un menú público `UploadMode`.

---

## Bootloader

Alpha5 mantiene la generación automática del bootloader desde el SDK.

No se adopta ni publica un `bootloader.bin` precompilado como artefacto
definitivo.

El flujo actual genera el bootloader desde:

`bootloader_qio_40m.elf`

Resultado:

`BOOTLOADER_PRECOMPILED=NOT_ADOPTED`

---

## Partición y configuración actual

JWPLC Basic conserva:

- ESP32;
- CPU 240 MHz;
- flash 4 MB;
- FlashFreq actual 40 MHz;
- DIO;
- `build.boot=qio`;
- bootloader `0x1000`;
- `jwplc_max_app_4mb`;
- APP máxima 4063232 bytes;
- coredump 64 KiB.

Este es el perfil actualmente validado.

No se declara como configuración universal para futuras revisiones del
producto.

La evaluación futura de otras configuraciones de flash queda fuera del
alcance de Alpha5.

---

## Validación física

Durante Alpha5 se realizaron gates físicos dirigidos sobre las partes
funcionalmente modificadas, incluyendo:

- botonera;
- microSD;
- TFT/GFX;
- W5500;
- FRAM;
- coexistencia SPI;
- Ethernet Link OFF/ON;
- DHCP;
- IP estática.

RTC, DI, DO, RS-485 y Modbus RTU conservan adicionalmente los gates físicos
de Alpha4 al no haber sido modificados funcionalmente durante esta fase.

El gate físico integral no se repitió sobre el último HEAD documental por
no disponer de hardware durante el cierre. El mismo sketch sí fue compilado
correctamente sobre el package Alpha5 final.

Se recomienda repetir esa regresión integral durante la validación de la
versión publicada.

---

## Alcance explícitamente diferido

Esta alpha no:

- asume OpenPLC integrado;
- define OTA;
- migra a FlashFreq 80 MHz;
- publica un bootloader.bin definitivo;
- introduce ESP32-S3;
- elimina periféricos por velocidad;
- incorpora bridges SPI/I2C genéricos.

---

## Documentación

La evidencia principal se conserva en:

- `docs/v2.1.0-alpha.5/ALPHA5_CLOSURE_CHECKLIST.md`;
- `docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md`;
- `docs/v2.1.0-alpha.5/LAPTOP_POWER_PROFILE_BENCHMARK_20260824.md`;
- `docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`;
- `docs/v2.1.0-alpha.5/UPLOAD_BOOTLOADER_CONFIGURATION_CONCLUSION_20260824.md`;
- `docs/v2.1.0-alpha.5/CORE_PRECOMPILED_REPRODUCIBILITY_20260824.md`;
- `docs/v2.1.0-alpha.5/SPI_STARTUP_TFT_PRESETUP_20260824.md`.

---

## Estado de esta PreRelease

La fase técnica de Alpha5 está cerrada y documentada.

Antes de publicar mediante Boards Manager corresponde:

1. revisión final del diff;
2. PR hacia `release/v2.1.x`;
3. CI y revisión del PR;
4. merge;
5. generación del ZIP;
6. cálculo de tamaño y SHA-256;
7. actualización del índice dev;
8. tag;
9. GitHub PreRelease;
10. instalación mediante Boards Manager;
11. validación de compilación y upload desde la instalación publicada;
12. regresión física integral cuando exista hardware disponible.

No avanzar a otra alpha antes de completar el cierre de publicación.
