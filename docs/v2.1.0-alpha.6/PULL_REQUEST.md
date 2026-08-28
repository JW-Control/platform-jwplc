## Objetivo

Cerrar `v2.1.0-alpha.6` consolidando el runtime Ethernet/W5500 cooperativo y no bloqueante, la recuperación DHCP/link sin reset y los diagnósticos `BUS`, `ETH` y `ERR` del IDLE, manteniendo todos los periféricos del autoload normal y preservando la arquitectura final de Alpha5.

## Corrección de base antes de publicación

Durante el cierre se detectó que la línea Alpha6 original no estaba construida sobre el cierre funcional definitivo de Alpha5. El PR quedó bloqueado y se corrigió la integración antes de publicar.

La línea final validada parte de:

```text
release/v2.1.x @ 64068556
```

Branch corregido:

```text
v2.1.0-alpha.6/integration/rebase-alpha5-final
```

Se preservan explícitamente:

```text
jwplcbasic.build.core=jwcontrol_precompiled_stub
precompiled/core/JWPLCBASIC/core.a
```

y se traslada únicamente el delta funcional Alpha6. La integración no produjo conflictos reales.

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
ALPHA6_ALPHA5_SOURCE_FALLBACK_SMOKE=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
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

Alpha6 consolida una única implementación W5500 dentro de `JWPLC_Ethernet` y elimina la librería backend separada.

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

### BUS

Validación física:

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

## JWPLC_Display precompilado final

Archive regenerado sobre la base corregida:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Metadatos:

```text
Bytes  : 368174
SHA256 : 4da9143e5e80d8ad0890e25bda8802ecee489b2a8c452c3ef1be556cff9541a7
```

Paridad source/archive:

```text
Archive members exactos  : True
Source Display compiles  : 2
Archive Display compiles : 0
Precompiled observed     : True
Source app               : 409765
Archive app              : 409765
Source RAM               : 27668
Archive RAM              : 27668
App delta                : 0
Linker fill delta        : 0
Raw .bin delta           : 0
Source-only symbols      : 0
Archive-only symbols     : 0
Structural parity        : True
```

```text
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
```

## Cold compile final de producción

Sobre el HEAD técnico final:

```text
HEAD                    : 379246c9
Compile exit            : 0
Tiempo                   : 62.261 s
Application .ino.bin     : 456816 bytes
Display precompiled      : True
Display source objects   : 0
Basic core.a observado   : True
Git status               : clean
```

```text
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
```

## Benchmark final Alpha5 vs Alpha6

Run Alpha6:

```text
20260828_174534
alpha6-integrated-final-379246c9
```

| Target | Fase | Alpha5 | Alpha6 | Cambio |
|---|---|---:|---:|---:|
| Basic | managed_cold | 54.594 s | 60.683 s | +11.15 % |
| Basic | managed_warm_nochange | 24.804 s | 22.122 s | -10.81 % |
| Basic | managed_warm_touch | 23.760 s | 22.922 s | -3.53 % |
| Basic | explicit_cold | 55.387 s | 60.369 s | +8.99 % |
| Basic | explicit_warm_nochange | 22.462 s | 21.774 s | -3.06 % |
| Basic | explicit_warm_touch | 22.219 s | 21.813 s | -1.83 % |
| Core | managed_cold | 60.717 s | 68.545 s | +12.89 % |
| Core | managed_warm_nochange | 20.934 s | 20.803 s | -0.63 % |
| Core | managed_warm_touch | 21.085 s | 20.589 s | -2.35 % |
| Core | explicit_cold | 58.617 s | 62.366 s | +6.40 % |
| Core | explicit_warm_nochange | 21.393 s | 20.065 s | -6.21 % |
| Core | explicit_warm_touch | 21.221 s | 19.905 s | -6.20 % |

Resumen agregado:

```text
12/12 fases = PASS
cold promedio combinado = +9.88 %
warm promedio combinado = -4.43 %
Basic cold compilers = 15
Core cold compilers = 78
warm compilers = 1
```

Alpha6 introduce una regresión cold cercana al 10 % frente a Alpha5, asociada a 7 TUs source adicionales del Ethernet consolidado. Se acepta como costo conocido porque la prioridad del alpha es estabilidad/corrección del runtime y no se eliminan periféricos del autoload para recuperar tiempo. Las recompilaciones warm mejoran en promedio.

```text
ALPHA6_BUILD_SPEED=PASS
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_WARM_AVG_IMPROVEMENT=4.43_PERCENT
```

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

No se retiró ningún periférico para acelerar la compilación.

## App-only

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
```

Es una herramienta auxiliar de desarrollo; el full upload sigue siendo la ruta normal.

## Bootloader

```text
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
```

Este PR no publica un `bootloader.bin` definitivo.

## Configuración actual

```text
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

Alpha6 no fija una configuración universal definitiva para futuras revisiones.

## Documentación

Documentos de cierre Alpha6:

- `docs/v2.1.0-alpha.6/ALPHA6_BASE_CORRECTION_20260828.md`
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

- [x] base corregida a Alpha5 final;
- [x] Ethernet cooperativo validado;
- [x] T1/T2 físico validado;
- [x] recuperación sin reset validada;
- [x] BUS/ETH/ERR validados;
- [x] Display precompilado final validado con paridad exacta;
- [x] cold build de producción aprobado;
- [x] benchmark final Basic/Core aprobado;
- [x] regresión cold documentada y aceptada;
- [x] autoload normal preservado;
- [x] App-only cerrado;
- [x] bootloader cerrado;
- [x] configuración actual/pending documentada;
- [ ] CI del PR aprobado.

## Después del merge

El push del merge a `release/v2.1.x`, al incluir el marcador:

```text
<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.6 -->
```

y `docs/v2.1.0-alpha.6/PRE_RELEASE.md`, debe activar `Auto Release JWPLC from README`.

Ese workflow dispara `Release JWPLC Arduino Package`, que prepara el ZIP, calcula SHA-256/tamaño, crea la GitHub PreRelease, actualiza el índice dev mediante una rama de automatización y abre el PR correspondiente hacia `main`.

No avanzar a otra alpha hasta cerrar y validar esa publicación.
