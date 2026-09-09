# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.11 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es mantener una experiencia cercana a Arduino, con las E/S industriales y periféricos del JWPLC integrados al runtime del package: TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485, Modbus RTU y TCA/I/O.

---

## Estado actual

| Canal / ciclo | Estado |
|---|---|
| `v2.0.0` | Release estable pública. |
| `v2.1.0-alpha.10` | Última PreRelease publicada en el índice dev. |
| `v2.1.0-alpha.11` | **Cierre técnico PASS; candidato pendiente de PR/merge/publicación.** |

```text
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PUBLICATION=PENDING_PR_MERGE
```

El índice dev no debe declararse actualizado a Alpha11 hasta que el workflow de publicación complete el merge y genere el artefacto final.

---

## Índices de Boards Manager

| Canal | Archivo | Estado |
|---|---|---|
| Dev / PreRelease | `JWPLC/package_jwplc_index_dev.json` | Actualmente publica Alpha10; Alpha11 pendiente. |
| Estable | `JWPLC/package_jwplc_index.json` | `v2.0.0`. |

URL dev:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

URL estable:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

En Arduino IDE:

```text
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

Luego:

```text
Herramientas > Placa > Gestor de tarjetas
```

Buscar:

```text
JW Control ESP32 Boards
```

---

# JWPLC Basic

JWPLC Basic es una plataforma industrial basada en ESP32. El package permite programarla con sintaxis Arduino sin exponer al usuario los detalles internos de expansores, buses y pines para el uso normal.

Perfil completo:

- 8 entradas digitales industriales;
- 8 salidas por relé;
- TCA6424A / I/O industrial;
- TFT ST7789 320×170;
- botonera frontal de 6 teclas;
- RTC;
- FRAM 8 KiB;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- arbitraje SPI compartido;
- capacidades ESP32 como Wi-Fi/Bluetooth/ESP-NOW cuando la aplicación las usa.

Ejemplo de E/S:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

No se retiran periféricos del autoload normal sólo para reducir tiempo de compilación.

---

## Placas / FQBN

| Placa | FQBN | Uso |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Desarrollo ESP32 genérico dentro del package. |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Hardware completo JWPLC Basic. |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Validación del core y perfil esencial. |

FQBN recomendado para producto completo:

```text
jwplc:esp32:jwplcbasic
```

Para desarrollo local se utiliza el namespace `jwplc_local`.

---

## Compatibilidad de periféricos

| Periférico / API | ESP32 Board | JWPLC Basic | JWPLC Basic Core |
|---|---:|---:|---:|
| `pinMode()` / `digitalRead()` / `digitalWrite()` industrial | No automático | Sí | Sí |
| TCA6424A | No automático | Sí | Sí |
| `JWPLC_Display` | No automático | Sí | Sí |
| `JWPLC_Buttons` | No automático | Sí | Sí |
| `JWPLC_RTC` / `JWPLC_Time` | No automático | Sí | Sí |
| `JWPLC_FRAM` | No automático | Sí | Disabled |
| `JWPLC_SD` | No automático | Sí | Disabled |
| `JWPLC_Ethernet` | No automático | Sí | Disabled |
| `JWPLC_RS485` | No automático | Sí | Sí |
| `JWPLC_ModbusRTU` | No automático | Sí | Sí |

Estados `disabled` en `JWPLC Basic Core` son esperados cuando el periférico no forma parte de ese perfil.

---

## APIs globales principales

```cpp
JWPLC_Display
JWPLC_Buttons
JWPLC_IO
JWPLC_Time
JWPLC_RTC
JWPLC_FRAM
JWPLC_SD
JWPLC_Ethernet
JWPLC_RS485
```

El package prioriza APIs de alto nivel y conserva compatibilidad con APIs históricas cuando no existe motivo para romper sketches probados.

---

# Alpha11 — JWPLC HMI Designer V1

Alpha11 introduce/cierra la herramienta visual para diseñar la TFT del JWPLC Basic.

```text
HMI_DESIGNER_V1=PASS_USER
TARGET=ST7789_320x170_ROT3_RGB565
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE_WEB_SERIAL=PASS
CODEGEN=PASS
```

El Designer genera:

```text
JWPLC_HMI_Generated.h
```

El `.ino` mantiene la lógica de aplicación.

Proyecto recomendado:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

Documentación:

```text
tools/jwplc-hmi-designer/README.md
docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md
```

---

## HMI declarativa

El motor `JWPLC_UI` soporta hasta 32 fields y varias páginas.

Tipos V1:

```text
TEXT
VALUE
BOOL
BAR
```

Uso típico:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperatura);
JWPLC_Display.setValue(FIELD_STATUS, "READY");
JWPLC_Display.setValue(FIELD_RUN, true);
JWPLC_Display.setBar(FIELD_LOAD, 75.0f);
```

El Designer genera la configuración/registro normal automáticamente.

---

## PixelMap Alpha11

El Designer incorpora edición PixelMap RGB565 con capas, brush/eraser, fill, eyedropper, línea, rectángulo y undo/redo.

Codegen disponible:

```text
RGB565_RUN
PACKED_SPAN16
```

`PACKED_SPAN16` se selecciona cuando reduce memoria/código y la paleta cabe en 16 colores.

Visibilidad runtime:

```cpp
JWPLC_Display.setPixelMapVisible(PIXELMAP_INDEX, true);
```

---

## Navegación multipágina

Indicador:

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

Alpha11 limpia input pendiente al volver al selector para evitar reingresos fantasma.

---

## LIVE Preview

LIVE usa Web Serial:

```text
SERIAL_BAUD=921600
SERIAL_RX_BUFFER=8192
FRAME_BUFFER_ROWS=32
FLOW_CONTROL=ACK
DIRTY_REGION=JWH2
LATEST_STATE_COALESCING=YES
```

El transporte quedó validado físicamente y congelado para Alpha11.

---

# Integración Windows / Arduino IDE

## Aplicación standalone

Instalador:

```text
tools/jwplc-hmi-designer/Install-JWPLC-HMI-Designer.cmd
```

Destino:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

Entrada:

```text
JWPLC-HMI-Designer.exe
```

Identidad visual:

```text
DESKTOP_SHORTCUT_ICON=PASS
WINDOW_ICON=PASS
TASKBAR_ICON=PASS
```

---

## Arduino IDE 2.3.4

La integración no requiere fork ni parche del IDE.

VSIX final Alpha11:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Instalación:

```text
%USERPROFILE%\.arduinoIDE\plugins
```

Aporta:

- comando `JWPLC: Abrir HMI Designer`;
- botón `JW HMI`;
- icono best-effort en editor;
- autocompletado contextual de `JWPLC_Display`.

Gate:

```text
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
A11_6_ARDUINO_IDE_AUTOCOMPLETE=PASS_USER_2_3_4
```

---

## Autocompletado curado

Al escribir:

```cpp
JWPLC_Display.
```

se ofrece la API recomendada, evitando getters/aliases redundantes que puedan confundir.

Contextos:

```text
setIdleWakeMode(     -> IDLE_WAKE_*
setIdleWakeButton(   -> BTN_*
setIdleReturnMode(   -> IDLE_RETURN_*
setIdleReturnButton( -> BTN_*
setUserRefreshMode(  -> USER_REFRESH_*
```

Las APIs compatibles continúan existiendo aunque no se prioricen en las sugerencias.

---

# Robustez de runtime Alpha11

## Botonera

Un sketch normal puede usar:

```cpp
JWPLC_Buttons.pressed(BTN_OK);
JWPLC_Buttons.released(BTN_ESC);
JWPLC_Buttons.isDown(BTN_UP);
```

sin añadir `delay()` ni Serial para estabilizar el scanner.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
USER_DELAY_REQUIRED=NO
```

---

## `digitalWrite()` repetido / runtime cerrado

Alpha11 también cerró un caso donde un `loop()` escribiendo continuamente una salida podía congelar RTC/Display cuando el `core.a` precompilado estaba desfasado respecto del source.

Caso soportado:

```cpp
void loop()
{
    digitalWrite(Q0_0, condicion);
}
```

No hace falta detectar manualmente cambios ni agregar `delay(1)`.

El source Alpha11 usa shadow TCA6424A y prioridad de servicio coherente; el `core.a` final fue regenerado y probado físicamente.

```text
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

Core final:

```text
ARCHIVE=JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
COMMIT=3cf37145d4555689b9bff80c8b3793128bb9090e
```

---

# Precompilación

## Core JWPLC Basic

Flujo normal:

```text
jwcontrol_precompiled_stub + precompiled/core/JWPLCBASIC/core.a
```

Gate Alpha11:

```text
SOURCE_BUILD_JWCONTROL_TUS=64
NORMAL_BUILD_JWCONTROL_TUS=0
NORMAL_BUILD_STUB_TUS=1
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

## JWPLC_Display

Archive final:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Identidad:

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
```

---

# Benchmark Alpha11

Tres réplicas, Basic/Core, dos sketches y seis fases por caso:

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
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
```

Conclusión:

```text
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

El archive Display se acepta por evitar recompilación de sus TUs con paridad source/archive, no por una afirmación artificial de aceleración global.

Documento:

```text
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
```

---

# Modelo de librerías

Flujo soportado:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

Las librerías JW/JWPLC se distribuyen con el package. No se recomienda instalar copias manuales paralelas en el sketchbook.

Se conservan protecciones específicas para dependencias externas vendorizadas cuando corresponde.

---

# Decisiones de configuración heredadas

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

No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

No se declara una FlashFreq universal futura.

OpenPLC no forma parte del autoload Arduino de Alpha11.

---

# Documentación Alpha11

```text
docs/v2.1.0-alpha.11/ALPHA11_STATUS.md
docs/v2.1.0-alpha.11/ALPHA11_CLOSURE_CHECKLIST.md
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md
JWPLC/2.1.0/libraries/JWPLC_Display/README.md
tools/jwplc-hmi-designer/README.md
```

---

# Estado del candidato

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_DISPLAY_PRECOMPILED=PASS
ALPHA11_CORE_PRECOMPILED=PASS
ALPHA11_BUILD_SPEED=PASS_WITH_HOST_VARIATION
ALPHA11_RUNTIME_REGRESSION_GATE=PASS_PHYSICAL
ALPHA11_AUTOCOMPLETE=PASS_USER
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PR_READY=YES
ALPHA11_PUBLICATION=PENDING_PR_MERGE
```
