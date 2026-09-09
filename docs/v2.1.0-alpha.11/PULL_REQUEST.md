# PR — v2.1.0-alpha.11: JWPLC HMI Designer V1 y cierre de runtime Display

## Resumen

Esta PR cierra `v2.1.0-alpha.11` consolidando **JWPLC HMI Designer V1** para la TFT integrada del JWPLC Basic, junto con mejoras de runtime y herramientas de integración con Arduino IDE 2.

El alcance principal incluye:

- HMI declarativa TEXT / VALUE / BOOL / BAR;
- navegación multipágina con selector `NN/TT`;
- PixelMap RGB565 con capas y codegen optimizado;
- LIVE Preview por Web Serial;
- generación de `JWPLC_HMI_Generated.h`;
- proyectos `.jwhmi` vinculados al sketch;
- aplicación Windows standalone;
- launcher y autocompletado contextual en Arduino IDE 2.3.4;
- robustez de botonera en loops cerrados;
- corrección del runtime cuando `digitalWrite()` se ejecuta continuamente;
- sincronización del `core.a` precompilado con el source Alpha11;
- cierre reproducible de `libJWPLC_Display.a`;
- benchmark final Basic/Core.

No se retiran periféricos del autoload normal.

---

## HMI Designer V1

Estado funcional:

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE_WEB_SERIAL=PASS
CODEGEN=PASS
HMI_DESIGNER_V1=PASS_USER
```

Target:

```text
ST7789
320x170
rotation 3
RGB565
```

El Designer genera:

```text
JWPLC_HMI_Generated.h
```

El `.ino` conserva la lógica de proceso.

Convención de proyecto:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

---

## Fields declarativos

Tipos V1:

```text
TEXT
VALUE
BOOL
BAR
```

Máximo actual:

```text
JWPLC_UI_MAX_FIELDS=32
```

API recomendada de actualización:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperatura);
JWPLC_Display.setValue(FIELD_STATUS, "READY");
JWPLC_Display.setValue(FIELD_RUN, true);
JWPLC_Display.setBar(FIELD_LOAD, 75.0f);
```

`setText()` y `setBool()` se conservan por compatibilidad/especialización.

---

## Navegación multipágina

El selector visible usa:

```text
NN/TT
```

Semántica:

```text
PAGE_SELECT
  LEFT / RIGHT -> cambiar página
  OK           -> entrar

PAGE_CONTENT
  LEFT / RIGHT / UP / DOWN / OK -> aplicación
  ESC                            -> selector
```

Se limpia input pendiente al volver de CONTENT a SELECT para evitar reingresos fantasma.

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_BUTTON_PENDING_INPUT_CLEANUP=PASS_PHYSICAL
```

---

## PixelMap

Alpha11 incorpora workbench PixelMap con:

- edición RGB565;
- capas;
- brush/eraser;
- fill;
- eyedropper;
- línea;
- rectángulo;
- undo/redo dedicado;
- visibilidad runtime;
- persistencia en proyecto;
- codegen optimizado.

Formatos de runtime:

```text
RGB565_RUN
PACKED_SPAN16
```

El Designer selecciona `PACKED_SPAN16` cuando reduce memoria/código y la paleta cabe en 16 colores.

---

## LIVE Preview

Transporte final Alpha11:

```text
Web Serial
SERIAL_BAUD=921600
SERIAL_RX_BUFFER=8192
FRAME_BUFFER_ROWS=32
FLOW_CONTROL=ACK
DIRTY_REGION=JWH2
LATEST_STATE_COALESCING=YES
```

```text
A11_LIVE_WEB_SERIAL=PASS
A11_LIVE_EVENT_DRIVEN=PASS
A11_LIVE_DIRTY_REGION_JWH2=PASS
A11_LIVE_PHYSICAL_GATE=PASS
```

LIVE queda congelado en Alpha11; no se introduce un segundo protocolo/runtime.

---

## Aplicación Windows

La aplicación se instala fuera del repositorio en:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

Entrada:

```text
JWPLC-HMI-Designer.exe
```

Identidad visual validada:

```text
DESKTOP_SHORTCUT_ICON=PASS
WINDOW_ICON=PASS
TASKBAR_ICON=PASS
```

El usuario no depende de la carpeta de desarrollo del repositorio para ejecutar el Designer.

---

## Arduino IDE 2.3.4

La integración no requiere fork ni parchear Arduino IDE.

VSIX final:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Instalación:

```text
%USERPROFILE%\.arduinoIDE\plugins
```

Funciones:

- comando `JWPLC: Abrir HMI Designer`;
- botón `JW HMI`;
- icono editor/title best-effort;
- autocompletado contextual de `JWPLC_Display`.

Gates:

```text
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
A11_6_ARDUINO_IDE_AUTOCOMPLETE=PASS_USER_2_3_4
IDE_COMPILE_UPLOAD_REGRESSION=0
```

El instalador del VSIX quedó desacoplado de la presencia del Designer: el autocompletado puede instalarse aunque la app todavía no esté disponible.

---

## Autocompletado `JWPLC_Display`

La extensión ofrece una lista **curada** de API recomendada.

Ejemplo:

```cpp
JWPLC_Display.
```

No se priorizan getters/aliases redundantes que puedan confundir al usuario.

Contextos:

```text
setIdleWakeMode(     -> IDLE_WAKE_*
setIdleWakeButton(   -> BTN_*
setIdleReturnMode(   -> IDLE_RETURN_*
setIdleReturnButton( -> BTN_*
setUserRefreshMode(  -> USER_REFRESH_*
```

Las APIs compatibles continúan existiendo aunque no se muestren como camino principal.

---

## Robustez de botonera

Se cerró el fallo histórico donde un `loop()` intensivo consultando `pressed()/released()` podía dejar la interacción aparentemente congelada.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
USER_DELAY_REQUIRED=NO
SERIAL_REQUIRED=NO
```

El sketch no necesita llamar manualmente `JWPLC_Buttons.update()`.

---

## Runtime cerrado / `digitalWrite()` repetido

Durante el cierre se reprodujo otro caso crítico:

```cpp
void loop()
{
    digitalWrite(Q0_0, condicion);
}
```

El hardware sólo funcionaba correctamente si el usuario añadía `delay(1)`.

El diagnóstico mostró que el `core.a` versionado estaba desfasado respecto del source Alpha11: el source ya contenía shadow TCA6424A y prioridad de servicio corregida, pero el archive precompilado seguía ejecutando comportamiento anterior.

Se regeneró el core y se repitió el gate físico.

Resultado:

```text
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_CORE_PRECOMPILED_SYNC=PASS
```

El sketch original volvió a compilar/subir sin `delay(1)` y quedó estable.

---

## Core precompilado final

Archive:

```text
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

Identidad:

```text
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
COMMIT=3cf37145d4555689b9bff80c8b3793128bb9090e
```

Gate:

```text
SOURCE_BUILD_JWCONTROL_TUS=64
NORMAL_BUILD_JWCONTROL_TUS=0
NORMAL_BUILD_STUB_TUS=1
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

---

## `JWPLC_Display` precompilado final

Archive:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Identidad:

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
COMMIT=4142f801fbacc9388bf63c3a6352696522d6b445
```

Gates:

```text
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS
```

Los cambios posteriores al archive fueron de header/autocompletado y no modificaron los `.cpp` que componen `libJWPLC_Display.a`.

---

## Benchmark final

Metodología:

```text
Targets: Basic, Core
Sketches: 01_empty, 02_io_basic
Réplicas: 3
Fases por caso: 6
TOTAL_PHASES=72
```

Resultado:

```text
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

Alpha11 conserva el rendimiento warm de Alpha10 dentro de la variación del host. El precompilado Display se acepta por evitar recompilación de sus TUs con paridad source/archive, no por una afirmación artificial de speedup global.

---

## TFT / arranque

Alpha11 estabiliza la secuencia de arranque de la TFT:

```text
TFT_CS_INITIAL=HIGH
TFT_RST_HELD_LOW_DURING_AUTOLOAD=YES
FIRST_IDLE_FRAME_IMMEDIATE_AFTER_DISPLAY_BEGIN=YES
```

La TFT/IDLE quedó funcionalmente validada. Cualquier patrón visible estrictamente anterior a la ejecución del firmware puede depender también del estado eléctrico/backlight y no se trata como bloqueo funcional del alpha.

---

## Compatibilidad / periféricos

No se retiran periféricos del autoload normal:

```text
DISPLAY=YES
ETHERNET_W5500=YES
MICROSD=YES
FRAM=YES
RTC=YES
BUTTONS=YES
RS485=YES
MODBUS_RTU=YES
TCA_IO=YES
SPI_ARBITRATION=YES
```

```text
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

---

## Decisiones heredadas

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

No se declara una FlashFreq universal futura.

OpenPLC no se integra al autoload Arduino de Alpha11.

---

## Documentación

```text
docs/v2.1.0-alpha.11/ALPHA11_STATUS.md
docs/v2.1.0-alpha.11/ALPHA11_CLOSURE_CHECKLIST.md
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md
JWPLC/2.1.0/libraries/JWPLC_Display/README.md
tools/jwplc-hmi-designer/README.md
```

---

## Estado de la PR

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_DISPLAY_PRECOMPILED=PASS
ALPHA11_CORE_PRECOMPILED=PASS
ALPHA11_BUILD_SPEED=PASS_WITH_HOST_VARIATION
ALPHA11_RUNTIME_REGRESSION_GATE=PASS_PHYSICAL
ALPHA11_AUTOCOMPLETE=PASS_USER
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PUBLICATION=PENDING_PR_MERGE
```

Destino:

```text
v2.1.0-alpha.11/feature/hmi-designer
    -> release/v2.1.x
```

Antes del merge se requiere CI verde sobre el commit documental final.
