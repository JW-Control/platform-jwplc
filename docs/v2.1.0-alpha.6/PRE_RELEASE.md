# v2.1.0-alpha.6 — Ethernet cooperativo y diagnóstico integrado

`v2.1.0-alpha.6` es una PreRelease técnica del package Arduino para JWPLC Basic.

Esta versión consolida el runtime Ethernet/W5500 cooperativo, mejora la recuperación DHCP/link sin reset y amplía el diagnóstico visible en el IDLE mediante `BUS`, `ETH` y `ERR`, manteniendo todos los periféricos integrados y el rendimiento de compilación de Alpha5.

## Cambios principales

### Ethernet y W5500

- consolidación del backend W5500 dentro de `JWPLC_Ethernet`;
- `JWPLC_Ethernet.service()` cooperativo;
- DHCP inicial no bloqueante para el runtime;
- mantenimiento T1 renew y T2 rebind;
- recuperación de link sin reset;
- recuperación router -> red sin DHCP -> router;
- conservación de la configuración utilizable durante mantenimiento;
- diagnósticos Ethernet visibles en IDLE.

### Display e IDLE

- códigos de estado `ETH`;
- códigos de estado `BUS`;
- códigos de error de aplicación `ERR` de 1 a 4 caracteres;
- nueva API `setErrCode()` / `errCode()`;
- compatibilidad preservada con `setErrLed(bool)`;
- actualización de README de las librerías `JWPLC_`.

### JWPLC_Display precompilado

Se regenera el archive final de Display:

```text
libJWPLC_Display.a
368202 bytes
SHA256 a0094a9d9bf5c40bbd91a18514d97c488b2e8ba1ba6c18ec8161cb74445b416e
```

La paridad source/archive se validó estructuralmente:

```text
mismos miembros .o
mismo conjunto de símbolos
misma RAM
0 TUs Display recompiladas en modo precompiled
delta de APP = delta de linker fill = 8 bytes
```

## Validación física

Alpha6 fue validada con:

- W5500;
- DHCP;
- IP estática;
- HTTP;
- desconexión/reconexión;
- recuperación sin reset;
- T1 renew;
- T2 rebind;
- TFT;
- FRAM;
- microSD;
- mutex SPI;
- stress SPI/Ethernet de 10 minutos;
- RS-485 / Modbus RTU para diagnóstico BUS;
- códigos ERR en TFT.

Marcadores principales:

```text
DHCP_SERVICE_NONBLOCKING=PASS
ALPHA6_DHCP_T1_T2=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ALPHA6_PRODUCTION_BUILD_CLEAN=PASS
DHCP_TEST_HOOKS_EXCLUDED=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
```

## Rendimiento de compilación

Benchmark final sobre el mismo entorno de comparación usado para Alpha5:

| Target | Fase | Alpha5 | Alpha6 | Cambio |
|---|---|---:|---:|---:|
| Basic | managed cold | 54.594 s | 54.912 s | +0.58 % |
| Basic | managed warm | 24.804 s | 21.343 s | -13.95 % |
| Basic | managed touch | 23.760 s | 21.264 s | -10.51 % |
| Basic | explicit cold | 55.387 s | 54.241 s | -2.07 % |
| Basic | explicit warm | 22.462 s | 20.618 s | -8.21 % |
| Basic | explicit touch | 22.219 s | 20.525 s | -7.62 % |
| Core | managed cold | 60.717 s | 61.708 s | +1.63 % |
| Core | managed warm | 20.934 s | 20.990 s | +0.27 % |
| Core | managed touch | 21.085 s | 20.957 s | -0.61 % |
| Core | explicit cold | 58.617 s | 61.543 s | +4.99 % |
| Core | explicit warm | 21.393 s | 20.462 s | -4.35 % |
| Core | explicit touch | 21.221 s | 20.989 s | -1.09 % |

Las recompilaciones warm mejoran de forma general y los cold permanecen en el mismo orden de magnitud.

## Compatibilidad

Se preservan:

- Arduino IDE / Arduino CLI;
- I/O JWPLC;
- TFT y botonera;
- RTC;
- FRAM;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- mutex SPI global;
- autoload normal del package.

No se retiró ningún periférico para acelerar la compilación.

## App-only y bootloader

No cambian las decisiones de Alpha5:

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
```

No se publica un `bootloader.bin` definitivo.

## No incluido

Esta PreRelease no:

- integra OpenPLC como runtime obligatorio;
- define OTA;
- fija una FlashFreq universal definitiva;
- migra a ESP32-S3;
- cambia la versión de `JWPLC_LogicRuntime_UI`.

Estos puntos permanecen fuera del alcance de Alpha6.

## Canal

`v2.0.0` continúa siendo la versión estable recomendada para producción.

`v2.1.0-alpha.6` pertenece al canal dev/PreRelease y está destinada a validación técnica antes de una futura versión estable del ciclo 2.1.0.
