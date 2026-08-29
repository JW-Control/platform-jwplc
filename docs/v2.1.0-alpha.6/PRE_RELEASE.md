# v2.1.0-alpha.6 — Ethernet cooperativo y diagnóstico integrado

`v2.1.0-alpha.6` es una PreRelease técnica del package Arduino para JWPLC Basic.

Esta versión consolida el runtime Ethernet/W5500 cooperativo, mejora la recuperación DHCP/link sin reset y amplía el diagnóstico visible en el IDLE mediante `BUS`, `ETH` y `ERR`, manteniendo todos los periféricos integrados y preservando la arquitectura final de Alpha5.

## Corrección de base antes de publicar

Durante el cierre se detectó que la línea Alpha6 original no estaba construida sobre el cierre funcional definitivo de Alpha5. La publicación se detuvo, se creó un backup del estado validado y Alpha6 se integró nuevamente sobre:

```text
release/v2.1.x @ 64068556
```

Después de esa corrección se repitieron los gates afectados: build source, adopción/paridad de Display, cold de producción y benchmark Basic/Core.

## Cambios principales

### Ethernet y W5500

- consolidación del backend W5500 dentro de `JWPLC_Ethernet`;
- eliminación de la librería backend W5500 separada;
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

Archive final regenerado sobre la base corregida:

```text
libJWPLC_Display.a
368174 bytes
SHA256 4da9143e5e80d8ad0890e25bda8802ecee489b2a8c452c3ef1be556cff9541a7
```

La paridad source/archive quedó exacta:

```text
mismos miembros .o
mismo conjunto de símbolos
misma RAM
misma ocupación APP
mismo binario de aplicación
0 TUs Display recompiladas en modo precompiled
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
ALPHA6_ALPHA5_SOURCE_FALLBACK_SMOKE=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
```

## Rendimiento de compilación

Benchmark final corregido:

```text
20260828_174534
HEAD 379246c9
```

| Target | Fase | Alpha5 | Alpha6 | Cambio |
|---|---|---:|---:|---:|
| Basic | managed cold | 54.594 s | 60.683 s | +11.15 % |
| Basic | managed warm | 24.804 s | 22.122 s | -10.81 % |
| Basic | managed touch | 23.760 s | 22.922 s | -3.53 % |
| Basic | explicit cold | 55.387 s | 60.369 s | +8.99 % |
| Basic | explicit warm | 22.462 s | 21.774 s | -3.06 % |
| Basic | explicit touch | 22.219 s | 21.813 s | -1.83 % |
| Core | managed cold | 60.717 s | 68.545 s | +12.89 % |
| Core | managed warm | 20.934 s | 20.803 s | -0.63 % |
| Core | managed touch | 21.085 s | 20.589 s | -2.35 % |
| Core | explicit cold | 58.617 s | 62.366 s | +6.40 % |
| Core | explicit warm | 21.393 s | 20.065 s | -6.21 % |
| Core | explicit touch | 21.221 s | 19.905 s | -6.20 % |

Lectura agregada:

```text
cold promedio combinado = +9.88 %
warm promedio combinado = -4.43 %
```

Alpha6 tiene una regresión cold explícita cercana al 10 %, asociada a 7 TUs source adicionales del Ethernet consolidado. Se acepta como costo conocido porque la prioridad es la estabilidad/corrección del runtime y no se retiran periféricos del autoload para recuperar tiempo. Las recompilaciones warm mejoran en promedio.

```text
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_WARM_AVG_IMPROVEMENT=4.43_PERCENT
```

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
- autoload normal del package;
- core Basic precompilado de Alpha5 mediante `jwcontrol_precompiled_stub` + `core.a`.

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

## Configuración

```text
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

Alpha6 mantiene el perfil actual validado, sin declararlo como configuración universal para futuras revisiones.

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
