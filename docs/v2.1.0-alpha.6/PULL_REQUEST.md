## Objetivo

Cerrar `v2.1.0-alpha.6` consolidando el runtime Ethernet/W5500 cooperativo y no bloqueante, la recuperación DHCP/link sin reset y los diagnósticos `BUS`, `ETH` y `ERR` del IDLE, manteniendo todos los periféricos del autoload normal y el rendimiento de compilación alcanzado en Alpha5.

## Resultado principal

Alpha6 valida sobre hardware real:

- Ethernet W5500 con IP estática y DHCP;
- recuperación de link sin reset;
- router -> laptop sin DHCP -> router;
- DHCP T1 renew;
- DHCP T2 rebind;
- stress SPI/Ethernet de 10 minutos;
- diagnóstico visual ETH;
- diagnóstico BUS con timeout y recuperación Modbus RTU;
- API ERR alfanumérica;
- exclusión de hooks de prueba del build normal.

Marcadores principales:

```text
DHCP_SERVICE_NONBLOCKING=PASS
ALPHA6_DHCP_T1_T2=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ALPHA6_PRODUCTION_BUILD_CLEAN=PASS
DHCP_TEST_HOOKS_EXCLUDED=PASS
ALPHA6_FINAL_PRODUCTION_COLD=PASS
```

## Ethernet cooperativo

`JWPLC_Ethernet.service()` mantiene el runtime sin introducir esperas largas de DHCP dentro del flujo normal.

Se validaron:

- estados de arranque y link;
- DHCP inicial;
- mantenimiento asíncrono/cooperativo;
- T1 renew;
- T2 rebind;
- conservación de red utilizable durante mantenimiento;
- recuperación tras desconexión;
- coexistencia con TFT, FRAM y SD sobre SPI.

Alpha6 consolida una única implementación W5500 dentro de `JWPLC_Ethernet`.

## Diagnóstico IDLE

El panel IDLE conserva:

```text
PWR | RUN | ERR | BUS | ETH
```

y añade información operativa de diagnóstico.

### ERR

API nueva:

```cpp
JWPLC_Display.setErrCode("A01");
JWPLC_Display.errCode();
```

Admite 1 a 4 caracteres alfanuméricos y mantiene `setErrLed(bool)` por compatibilidad.

Validación física:

```text
1
A01
TEMP
ZZZZ
0 -> clear
legacy red/no text
```

### BUS

Se validó físicamente:

```text
TMO -> rojo
RTU recuperado -> actividad verde
```

### ETH

El IDLE refleja estados como:

```text
INI
PHY
LNK
DHC
HW
IP
SPI
DIS
---
```

sin convertir errores de aplicación en errores Ethernet.

## JWPLC_Display precompilado final

Archive:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Metadatos:

```text
Bytes  : 368202
SHA256 : a0094a9d9bf5c40bbd91a18514d97c488b2e8ba1ba6c18ec8161cb74445b416e
```

Paridad estructural:

```text
Archive members exactos  : True
Archive source compiles  : 0
Precompiled observed     : True
Source RAM               : 27668
Archive RAM              : 27668
App delta                : +8
Linker fill delta        : +8
.flash.rodata delta      : +8
Source-only symbols      : 0
Archive-only symbols     : 0
Structural parity        : True
```

La diferencia de 8 bytes queda explicada por padding/alineamiento del linker. Los miembros del `.a` son byte-idénticos a los objetos fuente usados para producirlo.

```text
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
```

## Cold compile final de producción

Sobre el HEAD documental final:

```text
HEAD                    : 412b5b99
Compile exit            : 0
Tiempo                   : 59.867 s
Application .ino.bin     : 456768 bytes
Precompiled observed     : True
Display source objects   : 0
Display source TUs       : 0
Git status               : clean
```

```text
ALPHA6_FINAL_PRODUCTION_COLD=PASS
```

## Benchmark final Alpha5 vs Alpha6

Run Alpha6:

```text
20260828_141058
alpha6-final-412b5b99
```

| Target | Fase | Alpha5 | Alpha6 | Cambio |
|---|---|---:|---:|---:|
| Basic | managed_cold | 54.594 s | 54.912 s | +0.58 % |
| Basic | managed_warm_nochange | 24.804 s | 21.343 s | -13.95 % |
| Basic | managed_warm_touch | 23.760 s | 21.264 s | -10.51 % |
| Basic | explicit_cold | 55.387 s | 54.241 s | -2.07 % |
| Basic | explicit_warm_nochange | 22.462 s | 20.618 s | -8.21 % |
| Basic | explicit_warm_touch | 22.219 s | 20.525 s | -7.62 % |
| Core | managed_cold | 60.717 s | 61.708 s | +1.63 % |
| Core | managed_warm_nochange | 20.934 s | 20.990 s | +0.27 % |
| Core | managed_warm_touch | 21.085 s | 20.957 s | -0.61 % |
| Core | explicit_cold | 58.617 s | 61.543 s | +4.99 % |
| Core | explicit_warm_nochange | 21.393 s | 20.462 s | -4.35 % |
| Core | explicit_warm_touch | 21.221 s | 20.989 s | -1.09 % |

Resultado:

```text
12/12 fases = PASS
ALPHA6_BUILD_SPEED=PASS
```

Las recompilaciones warm mejoran de forma general; los cold permanecen dentro de variación aceptable frente a Alpha5.

## Autoload preservado

Se conservan en el flujo normal:

- Display;
- Ethernet / W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA / I/O;
- mutex SPI global.

No se retiró ningún periférico para mejorar los tiempos.

## App-only

Se mantiene la decisión de Alpha5:

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
```

Es una herramienta auxiliar de desarrollo; el full upload sigue siendo la ruta normal.

## Bootloader

Se mantiene:

```text
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
```

Este PR no publica un `bootloader.bin` definitivo.

## Configuración actual

El perfil actualmente validado se conserva como perfil de trabajo.

```text
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

Alpha6 no fija una configuración universal definitiva para futuras revisiones.

## Documentación

Se actualizaron los README de las librerías `JWPLC_` del package y el README raíz. Las librerías `JW_` mantienen su documentación fuente en el repositorio `JW_Libraries`.

Documentos de cierre Alpha6:

- `docs/v2.1.0-alpha.6/ALPHA6_CLOSURE_CHECKLIST.md`
- `docs/v2.1.0-alpha.6/ALPHA6_FINAL_VALIDATION_20260828.md`
- `docs/v2.1.0-alpha.6/BUILD_SPEED_COMPARISON_ALPHA5_ALPHA6_FINAL_20260828.md`
- `docs/v2.1.0-alpha.6/PRE_RELEASE.md`

## Decisiones que NO toma este PR

Este PR no:

- integra OpenPLC como runtime obligatorio del package;
- define OTA;
- fija FlashFreq universal final;
- publica un bootloader definitivo;
- migra a ESP32-S3;
- elimina periféricos del autoload;
- cambia la versión de `JWPLC_LogicRuntime_UI`.

La normalización/versionado adicional de `JWPLC_LogicRuntime_UI` queda para un alpha posterior.

## Checklist para merge

- [x] Ethernet cooperativo validado.
- [x] T1/T2 físico validado.
- [x] recuperación sin reset validada.
- [x] BUS/ETH/ERR validados.
- [x] Display precompilado final validado estructuralmente.
- [x] cold build de producción aprobado.
- [x] benchmark final Basic/Core aprobado.
- [x] comparación Alpha5 vs Alpha6 documentada.
- [x] autoload normal preservado.
- [x] App-only cerrado.
- [x] bootloader cerrado.
- [x] configuración actual/pending documentada.
- [x] README de librerías JWPLC y raíz actualizados.
- [ ] CI del PR aprobado.

## Después del merge

El push del merge a `release/v2.1.x`, al incluir el marcador:

```text
<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.6 -->
```

y `docs/v2.1.0-alpha.6/PRE_RELEASE.md`, debe activar `Auto Release JWPLC from README`.

Ese workflow dispara `Release JWPLC Arduino Package`, que prepara el ZIP, calcula SHA-256/tamaño, crea la GitHub PreRelease, actualiza el índice dev mediante una rama de automatización y abre el PR correspondiente hacia `main`.

No avanzar a otra alpha hasta cerrar y validar esa publicación.
