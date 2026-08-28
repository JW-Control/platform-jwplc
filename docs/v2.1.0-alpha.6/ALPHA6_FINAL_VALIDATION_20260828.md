# v2.1.0-alpha.6 — Validación final

Fecha: 2026-08-28

Branch validado:

```text
v2.1.0-alpha.6/integration/rebase-alpha5-final
```

Base funcional real de Alpha5:

```text
64068556
```

HEAD técnico del benchmark/cold final:

```text
379246c9
```

## Resumen

Alpha6 cierra el runtime Ethernet/W5500 cooperativo, la recuperación de red sin reset, el mantenimiento DHCP T1/T2, los diagnósticos `BUS`, `ETH` y `ERR` del IDLE y la adopción final de `JWPLC_Display` precompilado sobre la base definitiva de Alpha5.

Se mantiene el contrato del package: periféricos integrados, compatibilidad Arduino IDE/CLI y APIs ya validadas.

## Corrección de base de integración

La línea Alpha6 original había partido de una base anterior al cierre funcional definitivo de Alpha5. Antes de publicar se detectó la divergencia y se bloqueó el PR.

Se conservó una rama de respaldo del Alpha6 previamente validado y se trasladó únicamente el delta funcional Alpha6 sobre:

```text
release/v2.1.x @ 64068556
```

La integración se aplicó sin conflictos reales. Se verificó explícitamente que Alpha5 mantiene:

```text
jwplcbasic.build.core=jwcontrol_precompiled_stub
precompiled/core/JWPLCBASIC/core.a
```

La consolidación Alpha6 elimina el backend W5500 separado y lo integra dentro de `JWPLC_Ethernet`.

Gate source-fallback posterior a la corrección:

```text
Compile exit           : 0
Display source objects : 2
ALPHA6_ALPHA5_SOURCE_FALLBACK_SMOKE=PASS
```

## Ethernet y DHCP

Validado sobre hardware real durante Alpha6:

- W5500 detectado y operativo;
- IP estática y HTTP real;
- DHCP con router;
- arranque y servicio sin bloquear el loop;
- desconexión y reconexión de link;
- recuperación router -> laptop sin DHCP -> router sin reset;
- mantenimiento DHCP cooperativo;
- T1 renew físico;
- T2 rebind físico;
- lease utilizable durante mantenimiento;
- stress SPI/Ethernet de 10 minutos;
- 60 solicitudes HTTP durante stress sin fallas de W5500, mutex, FRAM, SD o DHCP.

Marcadores funcionales:

```text
DHCP_SERVICE_NONBLOCKING=PASS
ALPHA6_DHCP_T1_T2=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ALPHA6_PRODUCTION_BUILD_CLEAN=PASS
DHCP_TEST_HOOKS_EXCLUDED=PASS
```

Los hooks usados para forzar T1/T2 existen únicamente bajo macro de prueba y no quedan presentes en el build normal de producción.

Tras corregir la base no se repitió toda la campaña física porque la integración no produjo conflictos semánticos en estos componentes. Sí se repitieron los gates de compilación afectados por la nueva base: source fallback, archive Display, cold de producción y benchmark completo.

## Display e IDLE

El panel IDLE conserva `PWR`, `RUN`, `ERR`, `BUS` y `ETH`.

Alpha6 añade/valida:

- `ERR` alfanumérico mediante `setErrCode()`;
- compatibilidad con `setErrLed(bool)`;
- `BUS` con diagnóstico de RS-485/Modbus;
- `ETH` con diagnóstico del runtime Ethernet;
- estados visuales de actividad, espera y error.

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

Archive final regenerado sobre Alpha5 final + Alpha6:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Metadatos:

```text
Bytes  : 368174
SHA256 : 4da9143e5e80d8ad0890e25bda8802ecee489b2a8c452c3ef1be556cff9541a7
```

Gate estructural:

```text
Archive members exactos    : True
Source Display compiles    : 2
Archive Display compiles   : 0
Precompiled observed       : True
Source app bytes           : 409765
Archive app bytes          : 409765
App delta bytes            : 0
Source RAM bytes           : 27668
Archive RAM bytes          : 27668
Linker fill delta          : 0
.flash.rodata delta        : 0
Source-only symbols        : 0
Archive-only symbols       : 0
Raw .bin delta             : 0
Structural parity          : True
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
```

La paridad source/archive es exacta para aplicación, RAM, símbolos, secciones relevantes y binario final.

## Compilación normal final

Cold compile normal de producción usando el sketch de aceptación Alpha6:

```text
HEAD                         : 379246c9
Compile exit                 : 0
Tiempo                        : 62.261 s
Application .ino.bin          : 456816 bytes
Display precompiled           : True
Display source objects        : 0
Basic core.a observado        : True
Git status                    : clean
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
```

## Rendimiento final

Run final:

```text
20260828_174534
alpha6-integrated-final-379246c9
```

Resultado:

```text
12/12 fases = PASS
Basic cold compilers = 15
Core cold compilers = 78
Warm compilers = 1
ALPHA6_BUILD_SPEED=PASS
```

Comparado con Alpha5:

```text
promedio cold combinado : +9.88 %
promedio warm combinado : -4.43 %
```

La regresión cold es real y se registra como costo conocido de las 7 TUs source adicionales del Ethernet consolidado. No se retiran periféricos del autoload para ocultar esa penalización. Las recompilaciones warm mejoran en promedio.

```text
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_WARM_AVG_IMPROVEMENT=4.43_PERCENT
```

La comparación detallada está en `BUILD_SPEED_COMPARISON_ALPHA5_ALPHA6_FINAL_20260828.md`.

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

```text
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

El perfil actual permanece validado, sin declararlo como configuración universal final para futuras revisiones.

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
