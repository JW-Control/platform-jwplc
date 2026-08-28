# v2.1.0-alpha.6 — Validación final

Fecha: 2026-08-28

Branch:

```text
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

HEAD documental y de benchmark final:

```text
412b5b99
```

## Resumen

Alpha6 cierra el runtime Ethernet/W5500 cooperativo, la recuperación de red sin reset, el mantenimiento DHCP T1/T2, los diagnósticos `BUS`, `ETH` y `ERR` del IDLE y la regeneración final de `JWPLC_Display` como librería precompilada.

Se mantiene el contrato del package: periféricos integrados, compatibilidad Arduino IDE/CLI y APIs ya validadas.

## Ethernet y DHCP

Validado sobre hardware real:

- W5500 detectado y operativo.
- IP estática y HTTP real.
- DHCP con router.
- arranque y servicio sin bloquear el loop;
- desconexión y reconexión de link;
- recuperación router -> laptop sin DHCP -> router sin reset;
- mantenimiento DHCP cooperativo;
- T1 renew físico;
- T2 rebind físico;
- lease utilizable durante mantenimiento;
- stress SPI/Ethernet de 10 minutos;
- 60 solicitudes HTTP durante stress sin fallas de W5500, mutex, FRAM, SD o DHCP.

Marcadores de cierre:

```text
DHCP_SERVICE_NONBLOCKING=PASS
ALPHA6_DHCP_T1_T2=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ALPHA6_PRODUCTION_BUILD_CLEAN=PASS
DHCP_TEST_HOOKS_EXCLUDED=PASS
```

Los hooks usados para forzar T1/T2 existen únicamente bajo macro de prueba y se confirmó que no quedan presentes en el build normal de producción.

## Display e IDLE

El panel IDLE conserva `PWR`, `RUN`, `ERR`, `BUS` y `ETH`.

Alpha6 añade/valida:

- `ERR` alfanumérico mediante `setErrCode()`;
- compatibilidad con `setErrLed(bool)`;
- `BUS` con diagnóstico de RS-485/Modbus;
- `ETH` con diagnóstico del runtime Ethernet;
- estados visuales de actividad, espera y error;
- actualización automática sin trasladar errores de aplicación a Ethernet o Modbus.

Pruebas físicas de `ERR`:

```text
1
A01
TEMP
ZZZZ
0 -> clear
legacy setErrLed(true/false)
```

Prueba física de `BUS`:

```text
TMO -> rojo ante timeout
comunicación RTU recuperada -> actividad BUS verde
```

## JWPLC_Display precompilado final

Archive final:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Metadatos:

```text
Bytes  : 368202
SHA256 : a0094a9d9bf5c40bbd91a18514d97c488b2e8ba1ba6c18ec8161cb74445b416e
```

Gate estructural:

```text
Archive members exactos    : True
Source Display compiles    : 2
Archive Display compiles   : 0
Precompiled observed       : True
Source app bytes           : 409773
Archive app bytes          : 409781
App delta bytes            : 8
Source RAM bytes           : 27668
Archive RAM bytes          : 27668
Linker fill delta          : 8
.flash.rodata delta        : 8
Source-only symbols        : 0
Archive-only symbols       : 0
Structural parity          : True
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
```

La diferencia de 8 bytes de ocupación queda explicada por padding/alineamiento del linker. No aparecen ni desaparecen símbolos y los miembros del archive son byte-idénticos a los objetos fuente usados para generarlo.

## Compilación normal final

Última cold compile normal de producción, usando el sketch de aceptación Alpha6:

```text
HEAD                    : 412b5b99
Compile exit            : 0
Tiempo                   : 59.867 s
Application .ino.bin     : 456768 bytes
Precompiled observed     : True
Display .o en build      : 0
Display TU compiladas    : 0
Git status               : clean
ALPHA6_FINAL_PRODUCTION_COLD=PASS
```

## Rendimiento final

Run final:

```text
20260828_141058
alpha6-final-412b5b99
```

Resultado:

```text
12/12 fases = PASS
ALPHA6_BUILD_SPEED=PASS
```

La comparación detallada está en:

```text
BUILD_SPEED_COMPARISON_ALPHA5_ALPHA6_FINAL_20260828.md
```

## Decisiones heredadas que se mantienen

### App-only

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
```

App-only sigue siendo útil como herramienta de desarrollo, pero no reemplaza el upload completo y no se publica como modo predeterminado.

### Bootloader

```text
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
```

Alpha6 no publica un `bootloader.bin` definitivo.

### Configuración de flash

El perfil actualmente validado se conserva como perfil de trabajo; Alpha6 no declara una configuración universal final para futuras revisiones de hardware.

```text
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

## Fuera de alcance

Alpha6 no:

- integra OpenPLC como runtime obligatorio del package;
- define OTA;
- fija una política final de FlashFreq para todas las revisiones;
- publica un bootloader definitivo;
- migra a ESP32-S3;
- elimina periféricos del autoload;
- normaliza la versión de `JWPLC_LogicRuntime_UI`.

La revisión/versionado de `JWPLC_LogicRuntime_UI` queda explícitamente diferida a un alpha posterior.

## Resultado final técnico

```text
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_READY_FOR_PR=YES
```
