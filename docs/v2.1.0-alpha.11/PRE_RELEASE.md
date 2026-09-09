# v2.1.0-alpha.11 — JWPLC HMI Designer V1

## Resumen

`v2.1.0-alpha.11` incorpora el primer cierre completo de **JWPLC HMI Designer V1** para JWPLC Basic y endurece el runtime del package para que la TFT, RTC, botonera y E/S sigan funcionando correctamente incluso con `loop()` intensivos.

Esta versión está orientada a desarrollo/validación y se publicará en el canal **PreRelease / índice dev** después del merge correspondiente.

---

## Novedades principales

### JWPLC HMI Designer V1

Se añade una herramienta visual para construir interfaces de la TFT integrada del JWPLC Basic.

Incluye:

- TEXT;
- VALUE;
- BOOL;
- BAR;
- múltiples páginas;
- selector `NN/TT`;
- PixelMap RGB565;
- capas;
- edición por píxel;
- LIVE Preview;
- codegen automático;
- proyectos `.jwhmi`;
- aplicación Windows standalone.

Estado:

```text
HMI_DESIGNER_V1=PASS_USER
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE=PASS
CODEGEN=PASS
```

---

## Flujo de proyecto

El Designer genera:

```text
JWPLC_HMI_Generated.h
```

El usuario mantiene la lógica de aplicación en el `.ino`.

Estructura recomendada:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

El Designer no modifica automáticamente el `.ino`.

---

## PixelMap

Alpha11 incorpora PixelMap con:

- RGB565;
- capas;
- brush/eraser;
- fill;
- eyedropper;
- línea;
- rectángulo;
- undo/redo;
- visibilidad runtime.

El codegen puede usar:

```text
RGB565_RUN
PACKED_SPAN16
```

`PACKED_SPAN16` se selecciona cuando reduce memoria/código y la paleta cabe en 16 colores.

---

## Navegación multipágina

Indicador visible:

```text
NN/TT
```

Controles:

```text
Selector:
LEFT/RIGHT -> cambiar página
OK         -> entrar

Contenido:
LEFT/RIGHT/UP/DOWN/OK -> aplicación
ESC                    -> volver al selector
```

Se corrigió además la limpieza de eventos pendientes para evitar reingresos fantasma.

---

## LIVE Preview

LIVE utiliza Web Serial:

```text
baud=921600
RX_BUFFER=8192
FLOW_CONTROL=ACK
DIRTY_REGION=JWH2
LATEST_STATE_COALESCING=YES
```

El transporte quedó validado físicamente en Alpha11.

---

## Aplicación Windows

Instalación objetivo:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

Entrada:

```text
JWPLC-HMI-Designer.exe
```

El icono se valida en:

```text
Escritorio
ventana
taskbar
```

La aplicación puede instalarse y ejecutarse fuera del repositorio de desarrollo.

---

## Arduino IDE 2.3.4

Alpha11 incluye una integración experimental mediante VSIX, sin fork ni parche del IDE.

Versión:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Funciones:

- `JWPLC: Abrir HMI Designer`;
- botón `JW HMI`;
- icono best-effort en el editor;
- autocompletado contextual de `JWPLC_Display`.

Gate:

```text
ARDUINO_IDE_2_3_4=PASS_USER
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

---

## Autocompletado de Display

Al escribir:

```cpp
JWPLC_Display.
```

Arduino IDE muestra una lista curada de API recomendada.

También propone valores válidos dentro de setters:

```text
setIdleWakeMode(     -> IDLE_WAKE_*
setIdleWakeButton(   -> BTN_*
setIdleReturnMode(   -> IDLE_RETURN_*
setIdleReturnButton( -> BTN_*
setUserRefreshMode(  -> USER_REFRESH_*
```

Los getters/aliases compatibles continúan disponibles, pero no se priorizan cuando pueden confundir al usuario.

---

## Robustez de botonera

Alpha11 cierra el caso donde `pressed()/released()/isDown()` podían depender de pausas artificiales en un `loop()` intensivo.

El usuario puede escribir:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    // acción
}
```

sin añadir `delay()` ni Serial para estabilizar la botonera.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
USER_DELAY_REQUIRED=NO
```

---

## `digitalWrite()` sin `delay(1)`

También se cerró un problema detectado durante el gate final: un sketch escribiendo continuamente una salida podía congelar el servicio de RTC/Display cuando el `core.a` instalado estaba desfasado respecto del source Alpha11.

Caso soportado:

```cpp
void loop()
{
    digitalWrite(Q0_0, condicion);
}
```

No se requiere:

```cpp
delay(1);
```

Resultado físico:

```text
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

---

## Core precompilado actualizado

Archive final:

```text
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

```text
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
```

Gate:

```text
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

---

## `JWPLC_Display` precompilado final

Archive:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
```

---

## Benchmark

Resultado final de tres réplicas:

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
```

Estructura:

```text
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
```

Conclusión:

```text
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

Alpha11 no reclama una aceleración porcentual global artificial. El beneficio verificable del precompilado Display es evitar recompilar sus TUs con paridad source/archive.

---

## Periféricos integrados

Se conservan en el autoload normal del JWPLC Basic:

```text
Display
Ethernet W5500
microSD
FRAM
RTC
botonera
RS-485
Modbus RTU
TCA/I/O
SPI compartido
```

```text
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

---

## Compatibilidad

Las APIs previamente probadas se conservan cuando no existe una razón de producto para romperlas.

La API recomendada puede ser más estrecha que la API compatible; el autocompletado prioriza el camino más claro para código nuevo.

---

## Decisiones que no cambian

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

No se publica `bootloader.bin` como definitivo.

No se fija una FlashFreq universal futura.

OpenPLC continúa fuera del autoload Arduino.

---

## Estado de publicación

Antes de generar el artefacto final:

```text
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PUBLICATION=PENDING_PR_MERGE
```

Después del merge se deben registrar:

```text
TAG=v2.1.0-alpha.11
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=<PENDING>
SHA256=<PENDING>
DEV_INDEX_UPDATE=<PENDING>
PUBLISHED_INSTALL=<PENDING>
PUBLISHED_UPLOAD=<PENDING>
PUBLISHED_RUNTIME=<PENDING>
```

El índice estable debe permanecer sin cambios salvo decisión explícita.
