# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.5 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo del package es que trabajar con un JWPLC Basic se sienta tan directo como programar una placa Arduino, pero conservando las funciones propias de un controlador industrial: E/S de campo, display, botonera, memoria no volátil, microSD, Ethernet, RS-485 y Modbus RTU ya integrados al entorno.

En lugar de partir de un core ESP32 genérico y configurar manualmente cada periférico, el package entrega una plataforma preparada y validada para el hardware JWPLC.

---

## ¿Qué aporta el package JWPLC?

### Programación Arduino sobre hardware industrial

Las entradas y salidas del JWPLC Basic pueden utilizarse con la sintaxis habitual de Arduino:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

El usuario no necesita manejar directamente el expansor de E/S, los pines internos del ESP32 ni la inicialización de bajo nivel del hardware.

### Periféricos integrados

El perfil normal de **JWPLC Basic** mantiene integrados:

- 8 entradas digitales industriales.
- 8 salidas digitales por relé.
- TFT ST7789.
- Botonera frontal de 6 teclas.
- RTC.
- FRAM de 8 KB.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU.
- TCA / I/O industrial.
- Bus SPI compartido gestionado por el runtime JWPLC.

### APIs de alto nivel

El package expone objetos globales listos para utilizar:

```cpp
JWPLC_Display
JWPLC_Ethernet
JWPLC_SD
JWPLC_FRAM
JWPLC_RTC
JWPLC_Buttons
JWPLC_RS485
JWPLC_ModbusRTU
```

Esto permite concentrar el sketch en la lógica de la aplicación y no en repetir código de inicialización de hardware.

### Instalación centralizada

El package puede instalarse mediante **Boards Manager**, incluyendo automáticamente las herramientas requeridas para compilar y cargar firmware.

### Validado sobre hardware real

Las versiones del package se validan mediante compilación y gates sobre JWPLC Basic físicos, cubriendo E/S, display, memorias, comunicaciones y coexistencia de periféricos.

---

## Estado actual

| Canal | Versión | Uso recomendado |
|---|---|---|
| Público / estable | `v2.0.0` | Usuarios finales y proyectos estables |
| Dev / PreRelease | `v2.1.0-alpha.5` | Compatibilidad, precompilación y rendimiento del ciclo 2.1.0 |

La versión estable pública continúa siendo **v2.0.0**.

`v2.1.0-alpha.5` es la PreRelease técnica más reciente. Su objetivo principal es **recuperar precompilación segura y compatible con Arduino IDE, normalizar el core precompilado y reducir el trabajo de compilación sin retirar periféricos ni romper APIs ya validadas**.

---

## v2.1.0-alpha.5 — precompilación compatible y core normalizado

Alpha5 continúa la optimización iniciada en Alpha4, pero pone el foco en que los componentes precompilados sigan siendo compatibles con los perfiles del package sin introducir dependencias ocultas del runtime JWPLC.

### Resultado de compilación

Comparación principal realizada en el mismo PC:

| Medición | Tiempo cold | TUs |
|---|---:|---:|
| Alpha4 P6 | 67.322 s | 12 |
| Alpha5 final | **55.387 s** | **8** |
| Mejora Alpha5 | **-11.935 s / -17.73 %** | **-33.33 %** |

Resultado final de recuperación:

```txt
PRECOMPILED_RECOVERED_TUS=21/24
SOURCE_FALLBACK_TUS=3/24
```

Las 3 TUs que permanecen deliberadamente desde fuente son:

- `JWPLC_Display`: 2 TUs.
- `JW_RTC`: 1 TU.

Se prefirió conservar estos componentes desde fuente antes que introducir bridges SPI/I2C o shims genéricos únicamente para ganar velocidad.

### Core JWPLC Basic

La fuente funcional canónica queda en:

```txt
JWPLC/2.1.0/cores/jwcontrol/
```

El perfil normal de JWPLC Basic utiliza:

```txt
JWPLC/2.1.0/cores/jwcontrol_precompiled_stub/
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

`JWPLC Basic Core` continúa compilando la fuente canónica y se utiliza como perfil de control y validación.

Esto evita mantener una segunda copia funcional del core.

### Compatibilidad de librerías

Alpha5 incorpora una política explícita para los archives ESP32. Los únicos símbolos `jwplc...` admitidos como bridge GPIO genérico son:

```txt
jwplc_pinMode
jwplc_digitalWrite
jwplc_digitalRead
```

No se introducen bridges genéricos SPI/I2C ni wrappers generales del runtime para recuperar rendimiento.

La auditoría global final revisó **12 archives** y terminó con **0 dependencias `jwplc...` bloqueantes**.

`JW_MatrixButtons` vuelve a utilizar precompilación ESP32 después de validar:

- ABI.
- ESP32 genérico.
- JWPLC Basic.
- JWPLC Basic Core.
- Arduino IDE.
- Los seis botones sobre hardware real.

### Display y coexistencia SPI

Durante Alpha5 se detectó una contención durante la inicialización tardía del TFT ST7789. La inicialización física del display pasa a realizarse antes de `setup()`, conservando:

- mutex SPI;
- delays requeridos por el panel;
- APIs públicas existentes;
- arbitraje del bus compartido.

Se validó coexistencia con:

- Ethernet W5500 sin Link.
- Ethernet con IP estática.
- Ethernet con DHCP.
- FRAM.
- microSD.
- redraw del TFT.

La frecuencia SPI efectiva de microSD queda unificada explícitamente en **20 MHz**, eliminando una definición duplicada sin cambiar el comportamiento ya validado.

### CI del package

Los PR hacia `release/v2.1.x` ejecutan automáticamente el smoke CI del package y un gate integral de compilación.

Las pruebas específicas de librerías o runtimes de aplicación pueden mantenerse separadas del gate del core/package en ciclos posteriores.

### Periféricos preservados

**No se retiraron Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU ni TCA/I/O para conseguir estas mejoras.**

Documentación principal de Alpha5:

- [`ALPHA5_CLOSURE_CHECKLIST.md`](docs/v2.1.0-alpha.5/ALPHA5_CLOSURE_CHECKLIST.md)
- [`BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md`](docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md)
- [`PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`](docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md)
- [`SPI_STARTUP_TFT_PRESETUP_20260824.md`](docs/v2.1.0-alpha.5/SPI_STARTUP_TFT_PRESETUP_20260824.md)
- [`UPLOAD_BOOTLOADER_CONFIGURATION_CONCLUSION_20260824.md`](docs/v2.1.0-alpha.5/UPLOAD_BOOTLOADER_CONFIGURATION_CONCLUSION_20260824.md)

---

## Instalación

### Canal público / estable

Para proyectos normales se recomienda el índice público:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

En Arduino IDE:

```txt
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

Después:

```txt
Herramientas > Placa > Gestor de tarjetas
```

Buscar:

```txt
JW Control ESP32 Boards
```

La versión estable publicada actualmente es:

```txt
2.0.0
```

### Canal dev / PreRelease

Para probar el ciclo `2.1.0` y Alpha5:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Versiones del ciclo publicadas en este canal:

```txt
2.1.0-alpha.1
2.1.0-alpha.2
2.1.0-alpha.3
2.1.0-alpha.4
2.1.0-alpha.5
```

El canal dev no se recomienda como canal principal de producción mientras la versión siga siendo PreRelease.

---

## Placas disponibles

| Placa | FQBN | Uso |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Desarrollo ESP32 genérico dentro del package JWPLC |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Perfil completo del JWPLC Basic |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Perfil de validación del core desde fuente |

Para un JWPLC Basic normal:

```txt
jwplc:esp32:jwplcbasic
```

---

## E/S industriales

El expansor TCA6424A está integrado al core JWPLC, por lo que las E/S físicas se utilizan como pines lógicos del controlador.

### Entradas

```txt
I0_0
I0_1
I0_2
I0_3
I0_4
I0_5
I0_6
I0_7
```

### Salidas

```txt
Q0_0
Q0_1
Q0_2
Q0_3
Q0_4
Q0_5
Q0_6
Q0_7
```

También existen APIs por bloque, útiles para lógica PLC y mapas de estados:

```cpp
uint32_t inputs  = JWPLC_readInputs();
uint32_t outputs = JWPLC_readOutputs();

JWPLC_writeOutputs(0x0000000F);
```

---

## Display y botonera integrados

`JWPLC_Display` gestiona la TFT ST7789 del JWPLC Basic y ofrece una base de interfaz gráfica integrada al controlador.

Entre las funciones disponibles están:

- pantalla IDLE con estado general del equipo;
- pantalla USER para interfaces propias del sketch;
- indicadores `PWR`, `RUN`, `ERR`, `BUS` y `ETH`;
- callbacks para interfaces de usuario;
- control del periodo de refresco;
- integración con la botonera frontal;
- acceso al objeto TFT para interfaces avanzadas.

Ejemplo:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setUserRefreshPeriodMs(100);

JWPLC_Display.setRunLed(true);
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

La botonera integrada expone:

```txt
UP
DOWN
LEFT
RIGHT
CANCEL
OK
```

Documentación:

- [`JWPLC_Display`](JWPLC/2.1.0/libraries/JWPLC_Display/README.md)

---

## RTC, FRAM y microSD

### RTC

`JWPLC_RTC` proporciona fecha y hora para históricos, tiempos de proceso, fechado de archivos e interfaces de usuario.

```cpp
auto now = JWPLC_RTC.now();
```

### FRAM

`JWPLC_FRAM` ofrece memoria no volátil de alta frecuencia de escritura para contadores, parámetros, setpoints y estados de máquina.

```cpp
uint32_t starts = 0;
JWPLC_FRAM.get(0, starts);
starts++;
JWPLC_FRAM.put(0, starts);
```

### microSD

`JWPLC_SD` permite utilizar la microSD integrada para datalogging, recetas, archivos de configuración y exportación de información.

Documentación:

- [`JW_RTC`](https://github.com/JW-Control/JW_RTC/blob/main/README.md)
- [`JW_FRAM`](https://github.com/JW-Control/JW_FRAM/blob/main/README.md)
- [`JW_SD`](https://github.com/JW-Control/JW_SD/blob/main/README.md)

---

## Ethernet W5500

`JWPLC_Ethernet` integra el W5500 al runtime del JWPLC Basic.

El package contempla:

- inicialización del periférico;
- DHCP o configuración de red;
- estado de link;
- reconexión;
- coexistencia con los demás periféricos SPI;
- integración con el indicador `ETH` del display.

Durante las validaciones se probaron W5500, DHCP, link, servidor HTTP y uso simultáneo de Ethernet + TFT + FRAM + microSD.

Documentación:

- [`JWPLC_Ethernet`](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md)

---

## RS-485 y Modbus RTU

JWPLC Basic incluye RS-485 físico y una API propia para utilizarlo sin gestionar manualmente la dirección del transceptor.

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

Sobre esta interfaz se encuentra `JWPLC_ModbusRTU`, con soporte para operación master/slave.

```cpp
JWPLC_ModbusRTU.begin();
```

Funciones base actualmente utilizadas en modo slave:

| Código | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

APIs principales en modo master:

```cpp
readHoldingRegisters();
writeSingleRegister();
```

Documentación:

- [`JWPLC_RS485`](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md)
- [`JWPLC_ModbusRTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md)

---

## Librerías del ecosistema JWPLC

### Librerías internas del package

| Librería | Función principal | Documentación |
|---|---|---|
| `JWPLC_Display` | TFT, pantallas IDLE/USER, indicadores y callbacks gráficos | [README](JWPLC/2.1.0/libraries/JWPLC_Display/README.md) |
| `JWPLC_Ethernet` | W5500, red, link, DHCP y coexistencia SPI | [README](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md) |
| `JWPLC_RS485` | Acceso al puerto RS-485 del JWPLC Basic | [README](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md) |
| `JWPLC_ModbusRTU` | Modbus RTU master/slave sobre RS-485 | [README](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md) |

`JWPLC_GlobalPeripherals` forma parte de la infraestructura interna que permite exponer objetos globales, pero no necesita ser utilizado directamente por un sketch normal.

### Librerías JW distribuibles

| Librería | Función principal | Documentación |
|---|---|---|
| `JW_FRAM` | Acceso a FRAM SPI | [README](https://github.com/JW-Control/JW_FRAM/blob/main/README.md) |
| `JW_RTC` | RTC DS3232M / DS3232 | [README](https://github.com/JW-Control/JW_RTC/blob/main/README.md) |
| `JW_SD` | Wrapper para microSD | [README](https://github.com/JW-Control/JW_SD/blob/main/README.md) |
| `JW_MatrixButtons` | Botonera matricial, debounce y navegación | [README](https://github.com/JW-Control/JW_MatrixButtons/blob/main/README.md) |
| `JW_DWIN_RS485` | Comunicación complementaria con pantallas DWIN | [README](https://github.com/JW-Control/JW_DWIN_RS485/blob/main/README.md) |

`JW_DWIN_RS485` es complementaria y no forma parte del runtime base del JWPLC Basic.

---

## OpenPLC Editor

JWPLC Basic ha sido validado como target externo para **OpenPLC Editor v4**.

Esta integración se mantiene separada del package Arduino: **OpenPLC es opcional y no es necesario para programar JWPLC Basic desde Arduino IDE**.

Alpha5 no asume OpenPLC integrado al package. El trabajo específico de integración OpenPLC continúa en ciclos dedicados.

Documentación histórica de integración:

- [`docs/alpha32_openplc_integration/`](docs/alpha32_openplc_integration/)

---

## Validación de Alpha5

El cierre técnico de Alpha5 incluye pruebas automáticas, compilación desde Arduino CLI y Arduino IDE y gates físicos dirigidos sobre las áreas funcionalmente modificadas.

| Área | Resultado |
|---|---|
| Auditoría global de 12 archives | PASS |
| Dependencias `jwplc...` bloqueantes | 0 |
| Bridge GPIO genérico | PASS |
| JW_MatrixButtons precompilada | PASS |
| Botonera física de 6 teclas | PASS |
| microSD física | PASS |
| TFT pre-setup | PASS |
| Ethernet + TFT + FRAM + SD | PASS |
| Ethernet sin Link | PASS |
| Ethernet con IP estática | PASS |
| Ethernet con DHCP | PASS |
| Arduino CLI | PASS |
| Arduino IDE 2.x | PASS |
| Gate integral compile-only | PASS |
| CI automático del package | PASS |

Las funciones no modificadas conservan además la evidencia física integral de Alpha4 para RTC, 8 DI, 8 DO, RS-485 y Modbus RTU.

El gate físico integral completo no se repitió sobre el último HEAD documental de Alpha5 por no disponer de hardware durante ese cierre. No se clasificó como fallo porque las áreas funcionalmente modificadas sí cuentan con gates físicos específicos y el gate integral fue recompilado sobre el estado final.

La regresión física integral continúa recomendada durante la validación del package publicado.

Para el detalle:

- [`ALPHA5_CLOSURE_CHECKLIST.md`](docs/v2.1.0-alpha.5/ALPHA5_CLOSURE_CHECKLIST.md)
- [`PHYSICAL_COMPATIBILITY_VALIDATION_20260823.md`](docs/v2.1.0-alpha.5/PHYSICAL_COMPATIBILITY_VALIDATION_20260823.md)
- [`SPI_STARTUP_TFT_PRESETUP_20260824.md`](docs/v2.1.0-alpha.5/SPI_STARTUP_TFT_PRESETUP_20260824.md)

---

## Detalles técnicos de v2.1.0-alpha.5

La información siguiente se conserva para desarrolladores y mantenedores del package. No es necesaria para utilizar normalmente un JWPLC Basic.

<details>
<summary><strong>Configuración de flash y particionado</strong></summary>

### JWPLC Basic

| Parámetro | Valor actual validado |
|---|---|
| MCU | ESP32 |
| CPU | 240 MHz |
| Flash | 4 MB |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Boot base | QIO |
| Bootloader address | `0x1000` |
| Partition scheme | `jwplc_max_app_4mb` |
| Máximo de aplicación | 4,063,232 bytes / 3.875 MiB |
| Upload speed | 921600 |

`jwplc_max_app_4mb` conserva NVS, `otadata` y 64 KiB para coredump. SPIFFS no forma parte de este perfil.

La presencia de `otadata` no implica que exista una política OTA final definida.

El perfil anterior es la **configuración actualmente validada**. FlashFreq 40 MHz no se declara todavía como configuración universal definitiva para futuras revisiones del producto.

`JWPLC Basic Core` conserva su perfil propio de validación desde fuente.

</details>

<details>
<summary><strong>Herramientas instaladas por Boards Manager</strong></summary>

Alpha5 conserva cinco `toolsDependencies` externas:

| Tool | Versión |
|---|---|
| `esp-x32` | `2601` |
| `esptool_py` | `5.2.0` |
| `mkspiffs` | `0.2.3` |
| `mklittlefs` | `4.0.2-db0513a` |
| `esp32-libs` | `3.3.8` |

Estas herramientas se descargan automáticamente durante la instalación y son distintas de las librerías que viajan dentro del ZIP de JWPLC.

</details>

<details>
<summary><strong>Librerías precompiladas y componentes de build</strong></summary>

Alpha5 distribuye el core de JWPLC Basic y un conjunto de librerías precompiladas cuya compatibilidad fue auditada explícitamente.

Entre los componentes precompilados validados se encuentran:

- `JWPLC_ModbusRTU`;
- `JW_FRAM`;
- `JW_SD`;
- `JW_MatrixButtons`;
- Adafruit ST7735/ST7789;
- Adafruit GFX;
- Adafruit BusIO;
- `FS`;
- `SD`;
- `Wire`;
- `SPI`;
- backend Ethernet W5x00.

Permanecen deliberadamente desde fuente:

- `JWPLC_Display`;
- `JW_RTC`.

El objetivo es evitar bridges SPI/I2C genéricos o dependencias ocultas del runtime únicamente para recuperar velocidad.

El core precompilado se distribuye en:

```txt
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

Documentación técnica:

- [`PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`](docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md)
- [`CORE_PRECOMPILED_STUB_NORMALIZATION_20260824.md`](docs/v2.1.0-alpha.5/CORE_PRECOMPILED_STUB_NORMALIZATION_20260824.md)
- [`CORE_PRECOMPILED_REPRODUCIBILITY_20260824.md`](docs/v2.1.0-alpha.5/CORE_PRECOMPILED_REPRODUCIBILITY_20260824.md)

</details>

<details>
<summary><strong>SPI compartido</strong></summary>

TFT, Ethernet W5500, microSD y FRAM comparten el bus SPI del JWPLC Basic.

El runtime conserva un mecanismo de exclusión para evitar que dos periféricos utilicen simultáneamente el bus.

En Alpha5 se validó la inicialización temprana del TFT y la coexistencia del bus con Ethernet, FRAM y microSD. La frecuencia configurada de microSD queda unificada en 20 MHz.

Para interfaces gráficas con refresco frecuente se recomienda leer periféricos SPI fuera del callback de dibujo y mostrar en pantalla valores ya almacenados en variables simples.

</details>

<details>
<summary><strong>App-only, bootloader y OTA</strong></summary>

### App-only

Se mantiene como herramienta auxiliar de desarrollo, pero no se adopta como modo público de upload por defecto. La carga completa continúa siendo la ruta normal y segura en Arduino IDE.

### Bootloader

No se adopta ni publica un `bootloader.bin` precompilado como artefacto definitivo. La generación normal continúa a partir del ELF del SDK.

### OTA

Alpha5 no define una política OTA final. La presencia de `otadata` en la tabla de particiones no implica una política OTA de producto.

</details>

<details>
<summary><strong>Automatización de PR y publicación</strong></summary>

Alpha5 incorpora un smoke CI automático para PR hacia `release/v2.1.x`.

El proceso de publicación dispone además de un workflow que genera el ZIP, calcula tamaño y SHA-256, actualiza los package indexes, crea el GitHub PreRelease y adjunta el package.

La publicación puede dispararse mediante el marcador de versión del propio README:

```html
<!-- JWPLC_RELEASE_VERSION: X.Y.Z-alpha.N -->
```

Una edición del README que no cambie ese marcador no crea una nueva release.

</details>

---

## Para desarrolladores del package

El ciclo activo de desarrollo se encuentra en:

```txt
JWPLC/2.1.0
```

La fuente histórica/estable de `v2.0.0` se conserva en:

```txt
JWPLC/JWPLC-2.0.0
```

Las herramientas de benchmark, auditoría y validación están en:

```txt
tools/build-speed-benchmark/
```

Los documentos de cierre de Alpha5 están en:

```txt
docs/v2.1.0-alpha.5/
```

Para desarrollo local puede mantenerse una instalación separada `jwplc_local` enlazada al repositorio, evitando modificar el package administrado por Boards Manager.

---

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.
