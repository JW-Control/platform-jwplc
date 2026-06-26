# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, optimizado para el ecosistema **JWPLC Basic**.

El objetivo del package es ofrecer una experiencia industrial más directa que el core ESP32 genérico: menos variantes innecesarias, APIs de alto nivel y periféricos integrados al runtime del JWPLC.

---

## Estado de v2.0.0

`v2.0.0` corresponde a la **release estable inicial** del package Arduino **JW Control ESP32 Boards** para **JWPLC Basic**.

Objetivo:

```txt
Publicar una versión estable inicial, instalable desde Boards Manager, validada con Arduino IDE, Arduino CLI y hardware real.
```

Esta release toma como base el trabajo validado en:

```txt
v2.0.0-alpha.31
v2.0.0-beta.1
```

Durante este ciclo, `alpha31` actuó como validación técnica integral del package instalado desde cero, y `beta1` fue publicada como etapa de package validation antes de la estable.

A partir de `v2.0.0`, el flujo recomendado puede simplificarse en ciclos futuros:

```txt
alphas técnicas -> última alpha de verificación -> release estable
```

Beta queda como etapa opcional cuando se requiera validación pública o de terceros. RC queda como etapa opcional si se necesita un candidato final adicional.

Queda fuera de `v2.0.0`:

- OpenPLC como runtime integrado directamente dentro del package Arduino;
- definición final de OTA;
- publicación de `bootloader.bin` como definitivo;
- precompilación de librerías internas;
- cambios de arquitectura multicore;
- eliminación de periféricos del autoload normal solo por velocidad;
- partición recovery;
- soporte formal para hardware de 8 MB/16 MB;
- coredump como feature documentada de usuario.

> Nota: a partir de la etapa `alpha32-openplc-integration`, OpenPLC Editor v4 queda validado como **integración externa/opcional mediante patch**, sin modificar el package Arduino estable `v2.0.0`.

---


## Estado de desarrollo v2.1.0-alpha.2

`v2.1.0-alpha.2` corresponde a una **pre-release técnica** enfocada en mejoras y correcciones de `JWPLC_Display`, refresco TFT y panel de estado `IDLE`.

Esta alpha no reemplaza todavía a `v2.0.0` como canal estable público. Su objetivo es validar sobre hardware real los cambios de HMI antes de preparar una publicación estable posterior.

Objetivo:

```txt
Mejorar la experiencia de uso del TFT integrado, permitir configuración directa desde setup() y mostrar estados más claros para ETH/BUS en la pantalla IDLE.
```

Alcance principal:

- `setUserRefreshPeriodMs()` ahora controla realmente la frecuencia de `jwplcUserDisplayRefreshCallback()` en modo `USER`.
- Las configuraciones principales de `JWPLC_Display` pueden declararse desde `setup()`, sin esperar `isReady()` en `loop()`.
- Los indicadores `RUN`, `ERR`, `BUS` y `ETH` ya no son pisados por la inicialización interna de la TFT.
- `BUS` puede funcionar en modo automático usando actividad real de `JWPLC_RS485`.
- `ETH` y `BUS` usan estados visuales más claros: gris, apagado, verde y rojo.
- Se agregan códigos internos de prueba en `JWPLC/Test_Codes` para conservar histórico de validación.
- Se validó Modbus RTU Master/Slave entre dos JWPLC Basic usando el puerto RS-485 físico.

Estados visuales definidos para `BUS` y `ETH`:

| Estado visual | Significado general |
|---|---|
| Gris | Periférico no disponible, deshabilitado o no iniciado. |
| Apagado / negro | Periférico disponible, pero sin actividad o sin link activo. |
| Verde | Estado OK o actividad reciente. |
| Rojo | Error real detectado. |

Queda fuera de `v2.1.0-alpha.2`:

- OpenPLC como runtime integrado obligatorio.
- OTA.
- definición final de Flash Frequency como decisión de producto.
- publicación de `bootloader.bin` precompilado como definitivo.
- cambios de arquitectura multicore.
- nuevas mejoras visuales de TFT no necesarias para cerrar esta alpha.

> Cualquier mejora adicional de UX del display debería moverse a una alpha posterior para mantener `v2.1.0-alpha.2` acotada y verificable.

## Resumen rápido

**JWPLC Basic** es una plataforma industrial basada en **ESP32-WROOM-32E**. Este package permite programarla desde Arduino IDE manteniendo una sintaxis familiar, pero con periféricos industriales ya integrados:

- I/O industrial por TCA6424A.
- Display TFT ST7789.
- Botonera tipo LOGO!.
- RTC.
- FRAM.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU base.

La idea es que el usuario pueda escribir código tipo Arduino:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

bool state = digitalRead(I0_0);
digitalWrite(Q0_0, state);
```

sin tener que manejar directamente expansores I2C, buses SPI, pines internos, inicialización de periféricos o detalles del hardware.

---

## Enfoque del package

El package JWPLC mantiene un enfoque compacto y orientado a producto:

| Placa | Uso recomendado | Periféricos esperados |
|---|---|---|
| ESP32 Board | Desarrollo ESP32 genérico dentro del package JWPLC. | Sin periféricos JWPLC automáticos. |
| JWPLC Basic | Hardware completo JWPLC Basic. | I/O industrial, Display, botonera, RTC, FRAM, microSD, Ethernet, RS-485 y Modbus RTU base. |
| JWPLC Basic Core | Validación del core y pruebas esenciales sin periféricos opcionales. | I/O industrial y periféricos esenciales; FRAM, SD y Ethernet pueden reportar disabled. |

No se mantiene soporte visible para ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2 u otras familias dentro del package JWPLC mientras no sean requeridas por un producto real.

Esto reduce peso, simplifica mantenimiento y evita confusión dentro de Arduino IDE.

---

## Comparativa de tamaño del package

Una de las metas del package JWPLC es reducir el tamaño frente al package ESP32 genérico, manteniendo solo lo necesario para el ecosistema JWPLC Basic.

Medición local en Windows:

| Package | Tamaño | Tamaño en disco | Archivos | Carpetas |
|---|---:|---:|---:|---:|
| `jwplc` | 1.57 GB | 1.58 GB | 9,371 | 1,951 |
| `esp32` oficial | 5.72 GB | 5.80 GB | 44,264 | 11,327 |

Reducción aproximada:

| Métrica | Reducción |
|---|---:|
| Tamaño | 72.6 % |
| Cantidad de archivos | 78.8 % |
| Cantidad de carpetas | 82.8 % |

En la práctica, el package JWPLC ocupa alrededor del **27.4 %** del tamaño del package ESP32 oficial medido, es decir, es aproximadamente **3.6 veces más compacto**.

---

## Instalación

### Canal público recomendado

Para usuarios finales, usar el índice público:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Este canal está pensado para mostrar versiones estables y versiones públicas seleccionadas, evitando que el usuario final vea todas las alphas históricas.

En Arduino IDE, ingresa este enlace en:

```txt
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

Luego abre:

```txt
Herramientas > Placa > Gestor de tarjetas
```

Busca:

```txt
JW Control ESP32 Boards
```

e instala:

```txt
2.0.0
```

### Canal dev / interno

Para validación interna con alphas y betas históricas, usar:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Este canal no se recomienda para usuarios finales.

Versiones esperadas en este canal durante el ciclo `2.1.0`:

```txt
2.1.0-alpha.1
2.1.0-alpha.2
```

Para pruebas locales de PR o desarrollo activo se recomienda usar un package local separado, por ejemplo:

```txt
jwplc_local:esp32:jwplcbasic
```

y enlazarlo por junction/symlink hacia:

```txt
JWPLC/2.1.0
```

Esto permite probar cambios del repo local sin alterar la instalación estable `jwplc:esp32` instalada desde Boards Manager.

---

## Selección de placa

Después de instalar el package, selecciona una de las placas disponibles:

| Placa | FQBN | Uso recomendado |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Desarrollo ESP32 genérico dentro del package JWPLC. |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Hardware completo JWPLC Basic. |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Validación del core y pruebas sin periféricos opcionales. |

FQBN recomendado para JWPLC Basic:

```txt
jwplc:esp32:jwplcbasic
```

---

## Configuración estable de JWPLC Basic

Desde alpha30, `JWPLC Basic` y `JWPLC Basic Core` usan una configuración fija para reducir combinaciones no validadas y estabilizar el FQBN.

| Parámetro | Valor |
|---|---|
| CPU | 240 MHz |
| Flash size | 4 MB |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Bootloader base | QIO |
| Partition scheme | `huge_app` |
| Upload speed | 921600 |
| LoopCore | default del core |
| EventsCore | default del core |

La partición `huge_app` deja 3 MB para aplicación:

```txt
Maximum is 3145728 bytes.
```

### Decisiones asociadas

- OTA no está integrado todavía.
- OpenPLC no forma parte obligatoria del package Arduino; queda validado como integración externa/opcional mediante patch para OpenPLC Editor v4.
- No se publica `bootloader.bin` precompilado como definitivo.
- No se eliminan periféricos del autoload normal por velocidad.
- No se precompilan librerías internas en `v2.0.0`.
- `platform.txt` se mantiene sin cambios salvo bloqueante real.
- `app-only` queda documentado como herramienta útil, no como solución principal de compilación.

---

## Compatibilidad de periféricos

| Periférico / API | ESP32 Board | JWPLC Basic | JWPLC Basic Core |
|---|---:|---:|---:|
| `pinMode()` / `digitalRead()` / `digitalWrite()` sobre I/O industrial | No automático | Sí | Sí |
| TCA6424A integrado al core | No automático | Sí | Sí |
| `JWPLC_Display` | No automático | Sí | Sí |
| `JWPLC_Buttons` | No automático | Sí | Sí |
| `JWPLC_RTC` | No automático | Sí | Sí |
| `JWPLC_FRAM` | No automático | Sí | Disabled |
| `JWPLC_SD` | No automático | Sí | Disabled |
| `JWPLC_Ethernet` | No automático | Sí | Disabled |
| `JWPLC_RS485` | No automático | Sí | Sí |
| `JWPLC_ModbusRTU` | No automático | Sí | Sí |

> En **JWPLC Basic Core**, mensajes como `Ethernet disabled`, `SD disabled` o `FRAM size: 0` son esperados si esa variante fue compilada sin esos periféricos.

---

## I/O industrial nativo por TCA6424A

Una de las bases del package JWPLC es que el expansor **TCA6424A** queda integrado al core para que las entradas y salidas industriales se usen con funciones estándar de Arduino.

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

bool state = digitalRead(I0_0);
digitalWrite(Q0_0, state);
```

El usuario no necesita usar manualmente `Wire`, I2C ni escribir código directo para el TCA6424A.

### Entradas digitales

| Nombre JWPLC | Uso |
|---|---|
| `I0_0` | Entrada digital 0 |
| `I0_1` | Entrada digital 1 |
| `I0_2` | Entrada digital 2 |
| `I0_3` | Entrada digital 3 |
| `I0_4` | Entrada digital 4 |
| `I0_5` | Entrada digital 5 |
| `I0_6` | Entrada digital 6 |
| `I0_7` | Entrada digital 7 |

### Salidas digitales tipo relé

| Nombre JWPLC | Uso |
|---|---|
| `Q0_0` | Salida digital / relé 0 |
| `Q0_1` | Salida digital / relé 1 |
| `Q0_2` | Salida digital / relé 2 |
| `Q0_3` | Salida digital / relé 3 |
| `Q0_4` | Salida digital / relé 4 |
| `Q0_5` | Salida digital / relé 5 |
| `Q0_6` | Salida digital / relé 6 |
| `Q0_7` | Salida digital / relé 7 |

### I/O por bloque

Además del uso pin a pin, alpha29 incorporó APIs de lectura/escritura por bloque:

```cpp
uint32_t inputs = digitalReadBlock(I0_X);
uint32_t outputs = JWPLC_readOutputs();

JWPLC_writeOutputs(0x0000000F);
digitalWriteBlock(Q0_X, 0x000000AA);
```

APIs disponibles:

```cpp
digitalReadBlock(I0_X);
digitalWriteBlock(Q0_X, bitmap);

JWPLC_readInputs();
JWPLC_readOutputs();
JWPLC_writeOutputs(bitmap);
```

Esto permite trabajar con mapas de bits para integración con lógica PLC, Modbus u otros runtimes.

---

## APIs globales del ecosistema JWPLC

En `JWPLC Basic`, el package expone objetos globales para trabajar con sintaxis directa:

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

La idea es que el usuario final no tenga que repetir inicializaciones internas ni recordar pines, buses SPI, buses I2C o detalles de hardware.

---

## Librerías incluidas

El package incluye librerías internas del ecosistema JWPLC y snapshots estables de librerías JW distribuibles.

### Librerías internas del package JWPLC

| Librería | Descripción | README |
|---|---|---|
| `JWPLC_Display` | Manejo de TFT ST7789, pantallas IDLE/USER, callbacks de HMI e indicadores visuales. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/2.1.0/libraries/JWPLC_Display/README.md) |
| `JWPLC_Ethernet` | Integración del W5500, DHCP/static IP, reconexión, estado de link y coexistencia SPI. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md) |
| `JWPLC_RS485` | API nativa para usar el puerto RS-485 físico del JWPLC Basic sobre `Serial2`. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/2.1.0/libraries/JWPLC_RS485/README.md) |
| `JWPLC_ModbusRTU` | Modbus RTU base sobre `JWPLC_RS485`, con soporte inicial master/slave. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md) |

> `JWPLC_GlobalPeripherals` no tiene README independiente por ahora. Su función principal es agrupar y exponer los objetos globales del ecosistema JWPLC.

### Librerías JW externas / distribuibles

Estas librerías tienen repositorio propio y el README apunta al branch `main` oficial, que es la referencia pública para consulta y descarga independiente.

| Librería | Descripción | README |
|---|---|---|
| `JW_FRAM` | Librería SPI FRAM con API tipo EEPROM para guardar variables, estructuras, strings y configuraciones. | [Ver README](https://github.com/JW-Control/JW_FRAM/blob/main/README.md) |
| `JW_RTC` | Librería para RTC DS3232M / DS3232 con manejo de fecha/hora, timestamp Unix, temperatura, alarmas y NVRAM. | [Ver README](https://github.com/JW-Control/JW_RTC/blob/main/README.md) |
| `JW_SD` | Wrapper para microSD basado en `SD`, pensado para trabajar de forma segura con bus SPI compartido. | [Ver README](https://github.com/JW-Control/JW_SD/blob/main/README.md) |
| `JW_MatrixButtons` | Librería para lectura de botonera matricial con debounce, eventos, repeat y helpers de navegación HMI/PLC. | [Ver README](https://github.com/JW-Control/JW_MatrixButtons/blob/main/README.md) |
| `JW_DWIN_RS485` | Librería complementaria para comunicación con pantallas DWIN por RS-485 / Modbus RTU. No forma parte del runtime base de JWPLC Basic v2.0.0. | [Ver README](https://github.com/JW-Control/JW_DWIN_RS485/blob/main/README.md) |

> En `JWPLC Basic`, varias de estas librerías se usan a través de objetos globales ya integrados al runtime. Para uso normal del PLC, el usuario no necesita inicializar manualmente todos los periféricos ni recordar pines internos del hardware.

---

## Display integrado

`JWPLC_Display` se inicializa automáticamente en placas compatibles y muestra una pantalla `IDLE` con estado general del equipo.

La API recomendada usa estilo objeto con punto:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setUserRefreshPeriodMs(100);
JWPLC_Display.setRunLed(true);
```

### Configuración desde `setup()`

Desde `v2.1.0-alpha.2`, las configuraciones principales del display pueden declararse directamente desde `setup()`.

Ya no es necesario crear una lógica en `loop()` del tipo `if (JWPLC_Display.isReady())` solo para configurar comportamiento del display.

Ejemplo recomendado:

```cpp
#include <Arduino.h>
#include <JWPLC_Display.h>

void setup()
{
    Serial.begin(115200);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setIdleRefreshPeriodMs(1000);
    JWPLC_Display.setUserRefreshPeriodMs(100);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
}

void loop()
{
    // Lógica normal del sketch.
}
```

APIs validadas como configuración desde `setup()`:

```cpp
JWPLC_Display.setIdleWakeMode(...);
JWPLC_Display.setIdleWakeButton(...);

JWPLC_Display.setIdleReturnMode(...);
JWPLC_Display.setIdleReturnButton(...);
JWPLC_Display.setIdleTimeoutMs(...);

JWPLC_Display.setIdleRefreshPeriodMs(...);
JWPLC_Display.setUserRefreshPeriodMs(...);

JWPLC_Display.setRunLed(...);
JWPLC_Display.setErrLed(...);

JWPLC_Display.setBusLed(...);
JWPLC_Display.setBusLedAuto(...);

JWPLC_Display.setEthLed(...);
JWPLC_Display.setEthLedAuto(...);
```

`isReady()` sigue siendo útil para saber cuándo la TFT ya está disponible para dibujo directo, pero no es necesario para definir la configuración básica del display.

### IDLE y USER

`IDLE` es la pantalla automática base del JWPLC. Muestra:

- título del equipo;
- indicadores laterales `PWR`, `RUN`, `ERR`, `BUS`, `ETH`;
- entradas digitales;
- salidas digitales;
- RTC cuando está disponible.

`USER` es la pantalla personalizada del sketch. Se entra a `USER` cuando:

- se presiona una tecla de la botonera según el modo configurado;
- el sketch llama manualmente a `JWPLC_Display.enterUserUI()`.

Modos de entrada desde `IDLE` a `USER`:

```cpp
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

Modos de retorno desde `USER` a `IDLE`:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_BUTTON_ONLY
IDLE_RETURN_DISABLED
```

Ejemplo:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
```

### Refresco de pantalla USER

`setUserRefreshPeriodMs(ms)` define cada cuánto se llama:

```cpp
jwplcUserDisplayRefreshCallback(...)
```

en modo `USER`.

Desde `v2.1.0-alpha.2`, el core respeta este periodo también cuando no hay eventos de botones.

Ejemplos validados:

```cpp
JWPLC_Display.setUserRefreshPeriodMs(100); // 10 FPS
JWPLC_Display.setUserRefreshPeriodMs(20);  // prueba rápida de refresco
```

El runtime limita el valor a un rango seguro para evitar valores nulos o demasiado agresivos.

### Acceso directo a la TFT

Para dibujar directamente con Adafruit ST7789, incluye:

```cpp
#include <JWPLC_Display.h>
```

y usa:

```cpp
auto &tft = JWPLC_Display.tft();
```

También existe el alias:

```cpp
auto &display = JWPLC_Display.display();
```

El acceso directo a la TFT debe hacerse cuando la pantalla ya esté inicializada. Lo más ordenado es dibujar desde los callbacks:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(ST77XX_BLACK);
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    auto &tft = JWPLC_Display.tft();
    // Redibujar solo áreas pequeñas.
}
```

### Indicadores laterales IDLE

Los indicadores laterales resumen el estado del equipo:

| LED | Uso |
|---|---|
| `PWR` | Encendido del sistema. |
| `RUN` | Estado de ejecución definido por el usuario/runtime. |
| `ERR` | Estado de error definido por el usuario/runtime. |
| `BUS` | Estado o actividad del bus RS-485/Modbus RTU. |
| `ETH` | Estado de Ethernet W5500. |

Desde `v2.1.0-alpha.2`, `BUS` y `ETH` usan estados visuales de cuatro niveles:

| Color / estado | Significado |
|---|---|
| Gris | Periférico no disponible, deshabilitado o no iniciado. |
| Negro / apagado | Periférico disponible, pero sin actividad o sin link activo. |
| Verde | Estado OK o actividad reciente. |
| Rojo | Error real detectado. |

### LED BUS

Modo manual:

```cpp
JWPLC_Display.setBusLed(true);
JWPLC_Display.setBusLed(false);
```

`setBusLed(...)` desactiva el modo automático y fuerza el estado manual.

Modo automático:

```cpp
JWPLC_Display.setBusLedAuto(true);
```

En modo automático, `BUS` depende de `JWPLC_RS485`:

| Condición | LED BUS |
|---|---|
| RS-485 no disponible o no iniciado | Gris |
| RS-485 iniciado, sin tráfico reciente | Apagado |
| TX/RX reciente por `JWPLC_RS485` o `JWPLC_ModbusRTU` | Verde |
| Error RS-485 o error Modbus RTU reciente | Rojo |

Este indicador no inicializa RS-485 por sí mismo. Para que `BUS` salga de gris, el sketch debe iniciar `JWPLC_RS485` o `JWPLC_ModbusRTU`.

### LED ETH

Modo manual:

```cpp
JWPLC_Display.setEthLed(true);
JWPLC_Display.setEthLed(false);
```

`setEthLed(...)` desactiva el modo automático y fuerza el estado manual.

Modo automático:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

En modo automático:

| Condición | LED ETH |
|---|---|
| Ethernet no disponible o deshabilitado por variante | Gris |
| Ethernet disponible, pero no iniciado / link off | Apagado |
| Ethernet OK | Verde |
| Falla real de Ethernet | Rojo |

En `JWPLC Basic Core`, donde Ethernet puede estar deshabilitado por configuración, `ETH` debe mostrarse en gris.

### Recomendación para callbacks gráficos

En callbacks del display, evita hacer operaciones pesadas o consultas directas a periféricos SPI. Lo recomendado es:

1. Leer Ethernet, SD o FRAM desde `loop()`.
2. Guardar resultados en variables simples.
3. Dibujar en pantalla usando esas variables ya cacheadas.

Evita consultar `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` directamente dentro de callbacks de dibujo repetitivos.

## Botonera integrada

`JWPLC_Buttons` permite usar la botonera frontal del JWPLC Basic sin escanear manualmente la matriz.

Uso típico:

```cpp
if (JWPLC_Buttons.upPressed()) {
    // acción
}

if (JWPLC_Buttons.okPressed()) {
    // confirmar
}
```

La botonera se integra con el flujo de pantallas del display, permitiendo interfaces tipo menú, navegación o edición de parámetros.

---

## RTC integrado

`JWPLC_RTC` permite usar el RTC integrado del JWPLC Basic para fecha y hora del sistema.

Uso típico:

```cpp
auto now = JWPLC_RTC.now();
```

Aplicaciones:

- Registro de eventos.
- Fechado de logs.
- Sincronización de pantallas.
- Tiempos de proceso.
- Diagnóstico básico.

---

## FRAM integrada

`JWPLC_FRAM` permite almacenar datos persistentes de forma más robusta que EEPROM emulada.

Aplicaciones típicas:

- Contadores.
- Último estado de máquina.
- Parámetros de usuario.
- Setpoints.
- Flags de diagnóstico.

Ejemplo conceptual:

```cpp
uint32_t starts = 0;

JWPLC_FRAM.get(0, starts);
starts++;
JWPLC_FRAM.put(0, starts);
```

En `JWPLC Basic Core`, FRAM puede estar deshabilitada y reportar tamaño 0.

---

## microSD integrada

`JWPLC_SD` permite trabajar con archivos en microSD.

Aplicaciones típicas:

- Logs.
- Recetas.
- Archivos de configuración.
- Recursos para HMI.
- Exportación de datos.

Notas prácticas:

- Se recomienda FAT32.
- Para tarjetas grandes, usar partición MBR.
- microSD comparte SPI con TFT, W5500 y FRAM.

---

## Ethernet integrado

Ethernet W5500 queda integrado al runtime del JWPLC Basic.

En uso normal no es necesario llamar manualmente:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

El runtime se encarga de:

- Inicializar Ethernet automáticamente en variantes compatibles.
- Evitar bloqueos largos cuando no hay RJ45 conectado.
- Reintentar al conectar RJ45 después del arranque.
- Mantener DHCP.
- Proteger el bus SPI compartido.
- Actualizar el indicador `ETH` en pantalla IDLE.

### LED ETH en IDLE

Desde `v2.1.0-alpha.2`, `ETH` usa cuatro estados visuales:

| Condición | LED ETH |
|---|---|
| Ethernet disabled / Basic Core / variante sin W5500 | Gris |
| Ethernet disponible, pero no iniciado | Apagado |
| RJ45 desconectado / Link OFF | Apagado |
| Ethernet OK | Verde |
| Falla real de Ethernet | Rojo |

El color gris se reserva para indicar que el periférico no está disponible o está deshabilitado por configuración de placa. Esto evita confundir “no existe/no está habilitado” con “existe, pero está sin link”.

### Control manual / automático

El indicador puede controlarse manualmente:

```cpp
JWPLC_Display.setEthLed(true);
JWPLC_Display.setEthLed(false);
```

o dejarse en modo automático:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

Cuando se usa `setEthLed(...)`, el modo automático se desactiva para respetar el valor definido por el sketch.

## RS-485 integrado

El JWPLC Basic incluye API nativa para RS-485 usando `Serial2`.

```cpp
JWPLC_RS485.begin(); // 115200, SERIAL_8N1
```

| Señal | ESP32 |
|---|---:|
| RX2 | IO16 |
| TX2 | IO17 |

El JWPLC Basic usa transceptor MAX13487 con auto-direccionamiento, por lo que no se requiere controlar manualmente DE/RE.

RS-485 no se inicializa automáticamente. El usuario debe llamar a `begin()` porque el mismo `Serial2` puede usarse como UART RS-485 genérico o como Modbus RTU.

### Actividad de bus

Desde `v2.1.0-alpha.2`, `JWPLC_RS485` registra actividad reciente de recepción/transmisión:

```cpp
JWPLC_RS485.lastActivityMs();
JWPLC_RS485.lastRxActivityMs();
JWPLC_RS485.lastTxActivityMs();
JWPLC_RS485.hasRecentActivity(800);
```

Estas APIs permiten que `JWPLC_Display` actualice automáticamente el LED `BUS` en la pantalla IDLE.

La actividad se detecta cuando el sketch usa:

```cpp
JWPLC_RS485.available();
JWPLC_RS485.read();
JWPLC_RS485.write(...);
```

o cuando se usa `JWPLC_ModbusRTU`, porque Modbus RTU trabaja sobre `JWPLC_RS485`.

> Si un sketch usa `Serial2` directamente sin pasar por `JWPLC_RS485`, el indicador automático `BUS` no puede detectar esa actividad. Para mantener integración con el panel IDLE, usar `JWPLC_RS485` o `JWPLC_ModbusRTU`.

### LED BUS en IDLE

En modo automático:

```cpp
JWPLC_Display.setBusLedAuto(true);
```

| Condición | LED BUS |
|---|---|
| RS-485 no disponible o no iniciado | Gris |
| RS-485 iniciado, sin tráfico reciente | Apagado |
| TX/RX reciente por RS-485 | Verde |
| Error RS-485 / Modbus RTU reciente | Rojo |

### Uso directo

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
JWPLC_RS485.println("Hola RS485");
```

Para recibir:

```cpp
while (JWPLC_RS485.available() > 0)
{
    int value = JWPLC_RS485.read();
    if (value >= 0)
    {
        Serial.write((uint8_t)value);
    }
}
```

## Modbus RTU base

El package incluye base Modbus RTU sobre RS-485.

```cpp
#include <JWPLC_ModbusRTU.h>

JWPLC_ModbusRTU.begin(); // Slave ID 1, 19200, SERIAL_8E1
```

`JWPLC_ModbusRTU.h` incluye internamente `JWPLC_RS485.h`, por lo que no es necesario incluir ambas librerías cuando el sketch solo usa Modbus RTU.

`JWPLC_ModbusRTU.begin(...)` inicializa el puerto RS-485 con la configuración indicada. No debe llamarse adicionalmente `JWPLC_RS485.begin(...)` en el mismo sketch, salvo que se esté cerrando/reconfigurando el bus de forma intencional.

Funciones soportadas en modo slave:

| Código | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

Funciones soportadas en modo master:

| API | Función |
|---|---|
| `readHoldingRegisters()` | Lectura de holding registers |
| `writeSingleRegister()` | Escritura de un holding register |

### Validación Master/Slave

En `v2.1.0-alpha.2` se validó comunicación Modbus RTU entre dos JWPLC Basic:

```txt
JWPLC Basic Master  <->  RS-485  <->  JWPLC Basic Slave
```

Resultado validado:

- Master lee holding registers del Slave.
- Master escribe un registro del Slave.
- Slave actualiza registros internos.
- LED `BUS` parpadea en ambos equipos durante tráfico real.
- El mismo test sirve para validar el modo automático del LED `BUS`.

Los códigos internos de prueba se conservan en:

```txt
JWPLC/Test_Codes/alpha2_ModbusRTU_BusLedAuto_Master/
JWPLC/Test_Codes/alpha2_ModbusRTU_BusLedAuto_Slave/
```

## Compatibilidad con OpenPLC Editor v4

A partir de la etapa `alpha32-openplc-integration`, el **JWPLC Basic v2.0.0** fue validado como target externo para **OpenPLC Editor v4**.

Esta integración no modifica el package Arduino estable `platform-jwplc v2.0.0`. Se entrega como un patch externo para OpenPLC Editor, ubicado en:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

### Estado validado

- OpenPLC Editor reconoce `JWPLC BASIC [2.0.0]`.
- Compilación desde OpenPLC usando Arduino CLI.
- Subida por USB al JWPLC Basic.
- Debugger de OpenPLC operativo.
- Pin Mapping compatible con `I0_0..I0_7` y `Q0_0..Q0_7`.
- Lectura de entradas digitales.
- Activación de salidas digitales.
- Concordancia entre OpenPLC, E/S físicas y TFT.
- Modbus TCP por Ethernet W5500.
- DHCP y puerto TCP 502 validados.
- Pruebas con ModbusTool como master TCP.

### Limitación conocida

Modbus RTU y Modbus TCP fueron validados de forma independiente:

```txt
RTU solo: funcional.
TCP solo: funcional.
RTU + TCP simultáneo: TCP funciona; RTU queda pendiente de revisión.
```

### Instalación del patch OpenPLC

La guía y los scripts de instalación están en:

```txt
docs/alpha32_openplc_integration/
```

Para instalación rápida en Windows:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
```

El instalador crea un backup automático y copia los archivos modificados sobre la carpeta local de OpenPLC Editor.

### Mapa Modbus TCP validado

| OpenPLC | Modbus TCP | JWPLC Basic |
|---|---:|---|
| `%IX0.0` | Discrete Input 0 | `I0_0` |
| `%IX0.1` | Discrete Input 1 | `I0_1` |
| `%IX0.2` | Discrete Input 2 | `I0_2` |
| `%IX0.3` | Discrete Input 3 | `I0_3` |
| `%IX0.4` | Discrete Input 4 | `I0_4` |
| `%IX0.5` | Discrete Input 5 | `I0_5` |
| `%IX0.6` | Discrete Input 6 | `I0_6` |
| `%IX0.7` | Discrete Input 7 | `I0_7` |
| `%QX0.0` | Coil 0 | `Q0_0` |
| `%QX0.1` | Coil 1 | `Q0_1` |
| `%QX0.2` | Coil 2 | `Q0_2` |
| `%QX0.3` | Coil 3 | `Q0_3` |
| `%QX0.4` | Coil 4 | `Q0_4` |
| `%QX0.5` | Coil 5 | `Q0_5` |
| `%QX0.6` | Coil 6 | `Q0_6` |
| `%QX0.7` | Coil 7 | `Q0_7` |

Ver documentación completa en:

```txt
docs/alpha32_openplc_integration/openplc-io-map.md
docs/alpha32_openplc_integration/openplc-validation-report.md
docs/alpha32_openplc_integration/openplc-modbus-test-guide.md
```

> OpenPLC queda como integración externa/opcional. El uso normal del JWPLC Basic desde Arduino IDE sigue funcionando sin requerir OpenPLC.

---

## Reglas de coexistencia SPI

El JWPLC Basic comparte SPI entre:

- TFT ST7789.
- Ethernet W5500.
- FRAM.
- microSD.

Regla recomendada para sketches avanzados:

1. Consultar periféricos SPI desde `loop()`.
2. Guardar resultados en variables simples.
3. En callbacks gráficos del display, dibujar solo variables cacheadas.

Evita consultar `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` directamente dentro de callbacks de dibujo.

---

## Tiempos de compilación y caché

Desde alpha30 se documentó el comportamiento real de compilación:

| Prueba | Tiempo |
|---|---:|
| Build limpio con configuración fija | 02:01.014 |
| Build incremental del mismo sketch | 00:31.861 |
| Segundo sketch usando cache de core | 01:58.993 |
| Build incremental del segundo sketch | 00:31.296 |

Conclusiones:

- La primera compilación de un sketch sigue siendo la más pesada.
- Las compilaciones posteriores del mismo sketch son mucho más rápidas.
- El `bootloader.bin` precompilado no aportó una mejora significativa.
- La precarga global del package no se considera suficiente para alpha30.
- La optimización principal fue fijar la configuración de placa y reducir combinaciones.

---

## App-only

El flujo app-only permite subir únicamente la aplicación a `0x10000`, sin regrabar bootloader, particiones ni `boot_app0.bin`.

Resultado medido:

| Prueba | Tiempo |
|---|---:|
| App-only aislado | 00:06.352 |

Conclusión:

`app-only` es útil para desarrollo cuando la placa ya tiene bootloader y particiones correctas, pero no resuelve el tiempo principal de compilación.

---

## Bootloader precompilado

Se evaluó publicar un `bootloader.bin` precompilado en la variante `jwplcbasic`.

Resultado:

- Build limpio mejoró aproximadamente 2.9 s.
- Build incremental empeoró aproximadamente 2.5 s.

Decisión:

No se publica `bootloader.bin` precompilado como definitivo. Se mantiene la generación automática desde:

```txt
bootloader_qio_40m.elf
```

---

## OTA

OTA no está integrado todavía en JWPLC Basic v2.0.0.

La configuración actual usa `huge_app` para priorizar espacio de aplicación en hardware de 4 MB.

Pendientes futuros:

- Evaluar hardware de 8 MB/16 MB.
- Evaluar partición recovery.
- Evaluar interfaz de actualización desde display.
- Evaluar actualización desde microSD o Ethernet.

---

## Coredump

La partición `huge_app` conserva una partición `coredump`, que puede ser útil para diagnóstico de fallos graves del ESP32.

Uso futuro esperado:

- Diagnóstico de `Guru Meditation`.
- Análisis de backtrace.
- Identificación de tarea que falló.
- Depuración de `loopTask`, `jwplcSystemTask` u otros módulos.

El uso formal de coredump queda pendiente de validación y documentación en una versión posterior.

---

## Validación realizada

`v2.0.0` recoge las validaciones realizadas en `alpha31` y `beta1`.

| Área | Resultado |
|---|---|
| Instalación limpia desde Boards Manager | OK |
| Instalación y manejo desde otra PC | OK |
| Arduino IDE | OK |
| Arduino CLI | OK |
| `ESP32 Board` | OK |
| `JWPLC Basic` | OK |
| `JWPLC Basic Core` | OK |
| Sketch vacío | OK |
| Sketch Serial mínimo | OK |
| Subida por USB | OK |
| Monitor serial | OK |
| Librerías bundled desde package | OK |
| I/O TCA6424A | OK |
| Display | OK |
| Botonera | OK |
| RTC | OK |
| FRAM | OK |
| microSD | OK |
| Ethernet W5500 | OK |
| RS-485 | OK |
| Modbus RTU | OK |
| Coexistencia SPI Ethernet + SD + FRAM + Display | OK |

### Validación adicional OpenPLC alpha32

| Área | Resultado |
|---|---|
| OpenPLC Editor v4 reconoce `JWPLC BASIC [2.0.0]` | OK |
| Compilación desde OpenPLC usando Arduino CLI | OK |
| Subida por USB desde OpenPLC | OK |
| Debugger OpenPLC | OK |
| Pin Mapping `I0_x` / `Q0_x` | OK |
| Lectura de entradas digitales | OK |
| Activación de salidas digitales | OK |
| Concordancia con TFT | OK |
| Modbus TCP por W5500 | OK |
| DHCP | OK |
| Puerto TCP 502 | OK |
| FC01 Read Coils | OK |
| FC02 Read Discrete Inputs | OK |
| RTU solo | OK |
| TCP solo | OK |
| RTU + TCP simultáneo | Parcial: TCP OK, RTU pendiente |

### Validación adicional v2.1.0-alpha.2

| Área | Resultado |
|---|---|
| Package local `jwplc_local:esp32:jwplcbasic` apuntando a `JWPLC/2.1.0` | OK |
| `setUserRefreshPeriodMs(100)` con refresco USER a 10 FPS | OK |
| `setUserRefreshPeriodMs(20)` como prueba rápida de refresco | OK |
| Configuración de `JWPLC_Display` desde `setup()` | OK |
| `RUN`, `ERR`, `BUS`, `ETH` manuales desde `setup()` | OK |
| `ETH` automático con encendido/apagado según estado real | OK |
| `BUS` automático por actividad `JWPLC_RS485` | OK |
| `BUS` y `ETH` con estado gris para periférico no disponible/no iniciado | OK |
| Modbus RTU Master/Slave entre dos JWPLC Basic | OK |
| Parpadeo de `BUS` en ambos equipos con tráfico RS-485 real | OK |
| Test codes guardados en `JWPLC/Test_Codes` | OK |

---

## Ejemplos recomendados para validar instalación

Estos son los ejemplos principales usados y/o revisados para validación alpha31/beta1/v2.0.0.

### I/O industrial

- `DigitalIO_BlockRead`
- `DigitalIO_BlockMirror`

### RS-485

- `RS485_USB_Bridge`

### Modbus RTU

- `ModbusRTU_CRC_Test`
- `ModbusRTU_Master_Extern`
- `ModbusRTU_Master_ReadHoldingRegisters`
- `ModbusRTU_Master_WriteSingleRegister`
- `ModbusRTU_Slave_Extern`
- `ModbusRTU_Slave_HoldingRegisters`

### RTC

- `BasicRead`

### FRAM

- `Basic_RW`

### microSD

- `CardInfo`
- `BasicReadWrite`

### Display

- `Display_DotAPI_Minimal`
- `Display_UserUI_Callbacks`
- `Display_Idle_Return_Modes`
- `Display_FlappyBird`
- `Display_Tetris`

### Test codes internos v2.1.0-alpha.2

Estos códigos no son necesariamente ejemplos públicos finales; se conservan para histórico de validación del package:

- `JWPLC/Test_Codes/alpha2_JWPLC_Display_SetupLedTest`
- `JWPLC/Test_Codes/alpha2_JWPLC_Display_SetupLedAutoTest`
- `JWPLC/Test_Codes/alpha2_JWPLC_Display_BusLedAutoRS485Test`
- `JWPLC/Test_Codes/alpha2_ModbusRTU_BusLedAuto_Master`
- `JWPLC/Test_Codes/alpha2_ModbusRTU_BusLedAuto_Slave`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

### OpenPLC Editor v4

Los ejemplos/proyectos de OpenPLC se conservan como integración externa/documental en:

```txt
docs/alpha32_openplc_integration/
```

Pruebas mínimas recomendadas:

- Blink de `Q0_0` desde Ladder.
- Entrada `I0_0` controlando salida `Q0_0`.
- Lectura de coils por Modbus TCP.
- Lectura de discrete inputs por Modbus TCP.

---

## Checklist rápido de validación

- Instalar el package desde `package_jwplc_index.json`.
- Seleccionar **ESP32 Board** y compilar sketch vacío.
- Seleccionar **JWPLC Basic** y compilar sketch vacío.
- Seleccionar **JWPLC Basic Core** y compilar sketch vacío.
- Verificar que `JWPLC Basic` compile con partición `huge_app`.
- Verificar que el máximo de programa sea `3145728 bytes`.
- Probar `pinMode()`, `digitalRead()` y `digitalWrite()` con `I0_x` / `Q0_x`.
- Probar lectura/escritura por bloque.
- Probar Display IDLE.
- Probar retorno `USER -> IDLE` por timeout.
- Probar retorno `USER -> IDLE` por ESC.
- Probar Ethernet con RJ45 conectado y desconectado.
- Probar microSD insertada/retirada.
- Probar FRAM con lectura/escritura.
- Probar coexistencia Ethernet + SD + FRAM + Display.
- Probar RS-485 con bridge USB ↔ RS-485.
- Probar Modbus RTU como slave y master.
- Probar Arduino CLI con `jwplc:esp32:jwplcbasic`, `jwplc:esp32:jwplcbasiccore` y `jwplc:esp32:esp32`.

### Checklist adicional v2.1.0-alpha.2

- Probar con package local `jwplc_local:esp32:jwplcbasic`.
- Verificar que el log de compilación use `JWPLC/2.1.0`.
- Probar `setUserRefreshPeriodMs(100)` y confirmar refresco USER a 10 FPS.
- Probar `setUserRefreshPeriodMs(20)` como prueba rápida.
- Configurar `JWPLC_Display` desde `setup()` sin lógica adicional en `loop()`.
- Verificar LEDs manuales `RUN`, `ERR`, `BUS`, `ETH` desde `setup()`.
- Verificar `setEthLedAuto(true)` con RJ45 conectado/desconectado.
- Verificar `setBusLedAuto(true)` con tráfico RS-485.
- Probar Modbus RTU Master/Slave entre dos JWPLC Basic.
- Confirmar parpadeo de `BUS` en ambos equipos durante tráfico real.
- Confirmar `BUS` gris cuando RS-485/Modbus no está iniciado.
- Confirmar `ETH` gris en variante sin Ethernet o con Ethernet disabled.
- Confirmar que no se agregaron nuevas dependencias obligatorias.
- Confirmar que OpenPLC, OTA y bootloader precompilado no se asumen como parte de esta alpha.

### Checklist adicional OpenPLC

- Instalar OpenPLC Editor v4.
- Aplicar patch externo desde `docs/alpha32_openplc_integration/open-plc-editor/`.
- Seleccionar `JWPLC BASIC [2.0.0]` en OpenPLC Editor.
- Compilar proyecto mínimo.
- Subir por USB.
- Verificar Debugger.
- Validar Pin Mapping.
- Validar `I0_0..I0_7` y `Q0_0..Q0_7`.
- Habilitar Modbus TCP con DHCP.
- Validar puerto TCP `502`.
- Probar FC01 Read Coils.
- Probar FC02 Read Discrete Inputs.

---

## Estructura principal

```txt
JWPLC/
  package_jwplc_index.json
  package_jwplc_index_dev.json

  2.1.0/
    boards.txt
    platform.txt
    cores/
    variants/
    libraries/
      JWPLC_Display/
      JWPLC_Ethernet/
      JWPLC_RS485/
      JWPLC_ModbusRTU/
      ...

  JWPLC-2.0.0/
    boards.txt
    platform.txt
    cores/
    variants/
    libraries/

  Test_Codes/
    alpha2_JWPLC_Display_SetupLedTest/
    alpha2_JWPLC_Display_SetupLedAutoTest/
    alpha2_JWPLC_Display_BusLedAutoRS485Test/
    alpha2_ModbusRTU_BusLedAuto_Master/
    alpha2_ModbusRTU_BusLedAuto_Slave/
```

Notas:

- `JWPLC/2.1.0` es la carpeta activa para el ciclo de desarrollo `2.1.0`.
- `JWPLC/JWPLC-2.0.0` conserva la fuente histórica/estable de `v2.0.0`.
- `JWPLC/Test_Codes` conserva pruebas internas y sketches de validación que no necesariamente deben aparecer como ejemplos públicos de Arduino IDE.

Documentación y patch OpenPLC:

```txt
docs/
  alpha32_openplc_integration/
    README.md
    open-plc-editor/
    installer/
    openplc-integration-plan.md
    openplc-io-map.md
    openplc-architecture-review.md
    openplc-validation-report.md
    openplc-modbus-test-guide.md
    openplc-alpha32-checklist.md
```

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.
