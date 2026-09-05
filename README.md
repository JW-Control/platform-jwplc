# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.10 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es mantener una experiencia cercana a Arduino, pero con las E/S industriales y periféricos del JWPLC integrados al runtime del package: TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485, Modbus RTU y TCA/I/O.

---

## Índices de Boards Manager

Estos son los archivos que normalmente se necesitan primero al instalar el package:

| Canal | Archivo | Versión actual | Uso |
|---|---|---:|---|
| **Dev / PreRelease** | [`package_jwplc_index_dev.json`](JWPLC/package_jwplc_index_dev.json) | `2.1.0-alpha.10` | Talleres, validación y desarrollo de la rama 2.1.x. |
| **Estable** | [`package_jwplc_index.json`](JWPLC/package_jwplc_index.json) | `2.0.0` | Proyectos que requieren la release estable publicada. |

URL dev / PreRelease:

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

> Para un taller o validación de una alpha concreta, usar el índice **dev** únicamente cuando así se indique. Para usuarios finales, el canal estable sigue siendo `2.0.0` hasta que una nueva release estable sea publicada.

---

## Estado actual

| Canal / ciclo | Estado |
|---|---|
| `v2.0.0` | Release estable pública. |
| `v2.1.0-alpha.10` | PreRelease publicada y validada desde Boards Manager. |
| Alpha11 | Siguiente ciclo de desarrollo; no forma parte de Alpha10. |

Artefacto Alpha10 publicado:

```text
TAG=v2.1.0-alpha.10
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=24464282
SHA256=5ca5a71d6de0ddd25c81442d7ea4f840ad48603dd024afcd2925235dc4d1b0bf
PACKAGE_ROOT=2.1.0/
```

Alpha10 fue instalado desde el índice dev publicado, compilado y subido a hardware real con resultado PASS.

---

## Resumen rápido

**JWPLC Basic** es una plataforma industrial basada en ESP32. El package permite programarla con sintaxis Arduino sin tener que manejar directamente expansores, buses internos o pines físicos para las funciones normales del PLC.

El perfil completo integra:

- 8 entradas digitales industriales;
- 8 salidas digitales por relé;
- TCA6424A / I/O industrial;
- TFT ST7789;
- botonera frontal de 6 teclas;
- RTC;
- FRAM de 8 KiB;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- arbitraje del bus SPI compartido.

Además, el package conserva las capacidades del core ESP32 necesarias para aplicaciones como Wi-Fi, Bluetooth y ESP-NOW. Estas capacidades no deben confundirse con periféricos JWPLC autoinicializados.

Ejemplo de E/S:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

---

## Enfoque del package

El package JWPLC se mantiene orientado al producto y evita exponer combinaciones de hardware que no hayan sido necesarias para un JWPLC real.

| Placa | FQBN | Uso recomendado |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Desarrollo ESP32 genérico dentro del package JWPLC. |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Hardware completo JWPLC Basic. |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Validación del core y pruebas esenciales. |

FQBN recomendado para el hardware completo:

```text
jwplc:esp32:jwplcbasic
```

No se retiran periféricos del autoload normal sólo para reducir tiempos de compilación.

---

## Modelo de librerías: package-managed

Desde el cierre de Alpha10, el flujo soportado para las librerías propias JW/JWPLC es:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

Esto significa que una instalación normal requiere instalar **el package JWPLC**, no instalar manualmente copias adicionales de `JW_*` o `JWPLC_*` en el sketchbook de Arduino.

La variante inicial de Alpha10 había añadido un guard específico para una copia manual antigua de `JWPLC_Ethernet`. El candidato final lo retiró para recuperar el comportamiento de discovery del package:

```text
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_Ethernet_VERSION=1.0.0
```

Se mantienen las protecciones bundled de Adafruit GFX, BusIO y ST77xx porque son dependencias externas vendorizadas/precompiladas y pueden coexistir legítimamente con versiones instaladas desde Library Manager.

> Si existe una copia manual antigua de una librería JW/JWPLC en el sketchbook, la corrección soportada es retirar o actualizar esa copia. No se añade coste permanente al autoload para defender overrides manuales fuera del package.

---

## Compatibilidad de periféricos

| Periférico / API | ESP32 Board | JWPLC Basic | JWPLC Basic Core |
|---|---:|---:|---:|
| `pinMode()` / `digitalRead()` / `digitalWrite()` sobre I/O industrial | No automático | Sí | Sí |
| TCA6424A integrado | No automático | Sí | Sí |
| `JWPLC_Display` | No automático | Sí | Sí |
| `JWPLC_Buttons` | No automático | Sí | Sí |
| `JWPLC_RTC` / `JWPLC_Time` | No automático | Sí | Sí |
| `JWPLC_FRAM` | No automático | Sí | Disabled |
| `JWPLC_SD` | No automático | Sí | Disabled |
| `JWPLC_Ethernet` | No automático | Sí | Disabled |
| `JWPLC_RS485` | No automático | Sí | Sí |
| `JWPLC_ModbusRTU` | No automático | Sí | Sí |

En `JWPLC Basic Core`, estados como `Ethernet disabled`, SD deshabilitada o FRAM con tamaño 0 son esperados cuando esos periféricos no forman parte del perfil compilado.

---

## APIs globales del ecosistema JWPLC

El perfil `JWPLC Basic` expone objetos y helpers de alto nivel:

```cpp
JWPLC_Display
JWPLC_IO
JWPLC_Time
JWPLC_Ethernet
JWPLC_RTC
JWPLC_FRAM
JWPLC_SD
JWPLC_Buttons
JWPLC_RS485
JWPLC_ModbusRTU
```

El usuario no necesita repetir la inicialización interna de los periféricos que pertenecen al autoload del JWPLC.

---

## Librerías incluidas

El árbol `JWPLC/2.1.0/libraries/` contiene las librerías del ecosistema JWPLC y las dependencias necesarias del core ESP32.

### Librerías JWPLC del package

| Librería | Función principal | Documentación |
|---|---|---|
| `JWPLC_GlobalPeripherals` | Integración del autoload, objetos globales, snapshots de I/O/RTC y coordinación de periféricos. | [README](JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/README.md) |
| `JWPLC_Display` | TFT ST7789, IDLE/USER, indicadores y HMI declarativa. | [README](JWPLC/2.1.0/libraries/JWPLC_Display/README.md) |
| `JWPLC_Ethernet` | W5500, DHCP/static IP, diagnóstico, recovery y servicio cooperativo. | [README](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md) |
| `JWPLC_RS485` | Transporte RS-485 del JWPLC Basic sobre el UART industrial. | [README](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md) |
| `JWPLC_ModbusRTU` | Modbus RTU Master/Slave y Remote I/O sobre `JWPLC_RS485`. | [README](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md) |
| `JWPLC_LogicRuntime` | Runtime lógico experimental, separado del autoload Arduino normal. | [Carpeta](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime/) |
| `JWPLC_LogicRuntime_UI` | Capa UI experimental asociada a `JWPLC_LogicRuntime`. | [Carpeta](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime_UI/) |

### Librerías JW distribuidas dentro del package

| Librería | Función principal | Documentación |
|---|---|---|
| `JW_FRAM` | FRAM SPI con API de persistencia tipo EEPROM. | [README](JWPLC/2.1.0/libraries/JW_FRAM/README.md) |
| `JW_RTC` | RTC, fecha/hora y utilidades de tiempo. | [README](JWPLC/2.1.0/libraries/JW_RTC/README.md) |
| `JW_SD` | Wrapper de microSD preparado para el SPI compartido del JWPLC. | [README](JWPLC/2.1.0/libraries/JW_SD/README.md) |
| `JW_MatrixButtons` | Lectura de botonera matricial, debounce y eventos. | [README](JWPLC/2.1.0/libraries/JW_MatrixButtons/README.md) |

### Dependencias y librerías del core

El package también contiene librerías estándar o de terceros necesarias para el entorno ESP32, entre ellas:

```text
Adafruit_BusIO
Adafruit_GFX_Library
Adafruit_ST7735_and_ST7789_Library
Ethernet
FS
SD
SPI
Wire
WiFi
BluetoothSerial
BLE
ESP_NOW
```

Que una librería exista dentro del core no significa que una función de producto JWPLC esté definida. Por ejemplo, `ArduinoOTA` forma parte del ecosistema ESP32, pero la estrategia OTA final de JWPLC sigue **no definida**.

---

## I/O industrial nativo

Las entradas y salidas se usan con nombres lógicos:

```text
I0_0 ... I0_7
Q0_0 ... Q0_7
```

Uso pin a pin:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

También existen operaciones por bloque:

```cpp
uint32_t inputs = digitalReadBlock(I0_X);
uint32_t outputs = JWPLC_readOutputs();

JWPLC_writeOutputs(0x0F);
digitalWriteBlock(Q0_X, 0xAA);
```

Y una vista cacheada mantenida por el runtime:

```cpp
uint8_t inputs = JWPLC_IO.inputs();
uint8_t outputs = JWPLC_IO.outputs();

bool i0 = JWPLC_IO.input(0);
bool q0 = JWPLC_IO.output(0);
```

Las APIs cacheadas no fuerzan una nueva transacción I2C en cada consulta.

---

## Botonera integrada

Objeto global:

```cpp
JWPLC_Buttons
```

IDs:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

Ejemplo:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    Serial.println("OK");
}
```

El runtime mantiene el scan de la botonera. En uso normal no se necesita llamar manualmente `JWPLC_Buttons.update()` ni crear otro task de scan.

---

## Display, IDLE y HMI

`JWPLC_Display` gestiona TFT, pantalla IDLE, pantalla USER, diagnósticos, navegación y coordinación SPI.

El comportamiento seguro por defecto mantiene deshabilitado el wake automático hacia USER:

```text
IDLE_WAKE_DISABLED
```

Entrada explícita a USER:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    JWPLC_Display.enterUserUI();
}
```

Wake automático opcional:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
```

HMI declarativa:

```text
JWPLC_UI_FIELD_VALUE
JWPLC_UI_FIELD_TEXT
JWPLC_UI_FIELD_BOOL
JWPLC_UI_FIELD_BAR
```

Operaciones principales:

```cpp
JWPLC_Display.setFields(fields, count);
JWPLC_Display.setValue(fieldId, value);
JWPLC_Display.setText(fieldId, "READY");
JWPLC_Display.setBool(fieldId, true);
JWPLC_Display.setBar(fieldId, 75.0f);
JWPLC_Display.setUserPage(0);
JWPLC_Display.requestUserRefresh();
```

Para dibujo directo con Adafruit:

```cpp
#include <JWPLC_Display.h>
auto &tft = JWPLC_Display.tft();
```

Documentación: [`JWPLC_Display`](JWPLC/2.1.0/libraries/JWPLC_Display/README.md)

---

## RTC y tiempo cacheado

Además de `JWPLC_RTC`, el runtime expone `JWPLC_Time` para consultar el último snapshot sin forzar una nueva lectura física:

```cpp
JWPLC_Time.present();
JWPLC_Time.valid();
JWPLC_Time.lostPower();
JWPLC_Time.hour();
JWPLC_Time.minute();
JWPLC_Time.second();
JWPLC_Time.day();
JWPLC_Time.month();
JWPLC_Time.year();
JWPLC_Time.dayOfWeek();
```

---

## FRAM y microSD

`JWPLC_FRAM` proporciona persistencia rápida para contadores, parámetros, setpoints y estados.

`JWPLC_SD` permite trabajar con logs, recetas, configuraciones y exportación de datos.

TFT, W5500, FRAM y microSD comparten SPI. El package mantiene el arbitraje interno necesario para que estos periféricos coexistan en el flujo normal.

---

## Ethernet W5500

`JWPLC_Ethernet` mantiene el runtime cooperativo/no bloqueante consolidado desde Alpha6 y la contención SPI validada en ciclos posteriores.

Incluye:

- detección del W5500;
- estado de link RJ45;
- DHCP cooperativo;
- IP estática;
- recuperación sin reset;
- mantenimiento DHCP;
- diagnóstico de hardware/link/IP/SPI;
- coexistencia con TFT, FRAM y microSD.

El autoload utiliza internamente:

```cpp
JWPLC_Ethernet.service();
```

Las APIs síncronas se conservan por compatibilidad y commissioning.

Documentación: [`JWPLC_Ethernet`](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md)

---

## RS-485 y Modbus RTU

`JWPLC_RS485` expone el transporte industrial. La aplicación puede definir baudrate y formato:

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

`JWPLC_ModbusRTU` incluye operación Master/Slave y servicio cooperativo.

Ejemplo de servicio:

```cpp
void loop()
{
    JWPLC_ModbusRTU.task();
}
```

Alpha7 validó multidrop, Remote I/O y recuperación sin reset. Alpha9 reutilizó esta base para cerrar el recorrido OpenPLC Backplane con FC01/FC02/FC15 en hardware real.

Documentación:

- [`JWPLC_RS485`](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md)
- [`JWPLC_ModbusRTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md)
- [`JWPLC Remote I/O Slave RTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/examples/JWPLC_RemoteIO_Slave_RTU/README.md)

---

## Build speed y Alpha10

Alpha10 no elimina periféricos. Su cambio principal es retirar el coste de discovery añadido por guards JW/JWPLC que no forman parte del flujo package-managed soportado.

Benchmark histórico del mismo host:

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers JW/JWPLC | 22.094 s | base |
| Ethernet únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Tres réplicas del candidato final mantuvieron:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
```

Por variación del host no se reclama un porcentaje exacto de recuperación para el candidato final.

El empaquetado publicado también queda validado con una única raíz de Boards Manager:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

---

## OpenPLC / Backplane

OpenPLC continúa siendo una integración **externa/opcional** respecto al runtime Arduino del package.

El cierre Alpha9 validó físicamente:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

Ruta validada:

```text
Slave DI -> FC02 -> OpenPLC/Ladder -> FC15 -> Slave DO -> FC01 -> feedback
```

No asumir todavía:

```text
OpenPLC integrado al autoload Arduino = NO
Backplane baudrate configurable por UI = NO
Backplane serial format configurable   = NO
HMI Arduino expuesta a Ladder          = NO
```

La configuración RTU del Backplane, referencias tipadas de timers y la integración futura de HMI/Ladder permanecen como trabajo posterior a Alpha10.

---

## Decisiones de configuración vigentes

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo mientras la configuración final universal siga pendiente.

---

## Documentación de Alpha10

El detalle técnico y de publicación está en:

- [`ALPHA10_BUILD_BENCHMARK.md`](docs/v2.1.0-alpha.10/ALPHA10_BUILD_BENCHMARK.md)
- [`ALPHA10_PROTECTION_AUDIT.md`](docs/v2.1.0-alpha.10/ALPHA10_PROTECTION_AUDIT.md)
- [`ALPHA10_TECHNICAL_CLOSURE.md`](docs/v2.1.0-alpha.10/ALPHA10_TECHNICAL_CLOSURE.md)
- [`ALPHA10_CLOSURE_CHECKLIST.md`](docs/v2.1.0-alpha.10/ALPHA10_CLOSURE_CHECKLIST.md)
- [`ALPHA10_TO_ALPHA11_HANDOFF.md`](docs/v2.1.0-alpha.10/ALPHA10_TO_ALPHA11_HANDOFF.md)
- [`PRE_RELEASE.md`](docs/v2.1.0-alpha.10/PRE_RELEASE.md)

Estado del package publicado:

```text
ALPHA10_PUBLISHED_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
ALPHA10_RELEASE_PUBLICATION=PASS
```

---

## Regla práctica para usuarios

Para programar un JWPLC Basic normal:

1. Agrega el índice correspondiente a Arduino IDE.
2. Instala `JW Control ESP32 Boards`.
3. Selecciona `JWPLC Basic`.
4. Programa usando las APIs JWPLC incluidas en el package.

No es necesario instalar manualmente copias adicionales de las librerías JW/JWPLC que ya vienen dentro del package.
