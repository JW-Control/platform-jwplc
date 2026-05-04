# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, optimizado para el ecosistema **JWPLC Basic**.

El objetivo del package es ofrecer una experiencia industrial más directa que el core ESP32 genérico: menos variantes innecesarias, APIs de alto nivel y periféricos integrados al runtime del JWPLC.

---

## Resumen rápido

**JWPLC Basic** es una plataforma industrial basada en **ESP32 WROOM-32E**. Este package permite programarla desde Arduino IDE manteniendo una sintaxis familiar, pero con periféricos industriales ya integrados:

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

- **ESP32 Board** para pruebas ESP32 genéricas dentro del ecosistema JWPLC.
- **JWPLC Basic** para el hardware completo con periféricos integrados.
- **JWPLC Basic Core** para validación del core y pruebas esenciales sin periféricos opcionales.

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

En Arduino IDE, ingresa este enlace en:

```txt
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

URL:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Luego abre:

```txt
Herramientas > Placa > Gestor de tarjetas
```

Busca:

```txt
JW Control ESP32 Boards
```

e instala la versión disponible.

---

## Selección de placa

Después de instalar el package, selecciona una de las placas disponibles:

| Placa | Uso recomendado | Periféricos esperados |
|---|---|---|
| ESP32 Board | Desarrollo ESP32 genérico dentro del package JWPLC. | Sin periféricos JWPLC automáticos. |
| JWPLC Basic | Hardware completo JWPLC Basic. | I/O industrial, Display, botonera, RTC, FRAM, microSD, Ethernet, RS-485 y Modbus RTU base. |
| JWPLC Basic Core | Validación del core y pruebas sin periféricos opcionales. | I/O industrial y periféricos esenciales; FRAM, SD y Ethernet pueden reportar disabled. |

---

## Configuración actual de JWPLC Basic

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

FQBN recomendado:

```txt
jwplc:esp32:jwplcbasic
```

La partición `huge_app` deja 3 MB para aplicación:

```txt
El máximo es 3145728 bytes.
```

### Decisiones asociadas

- OTA no está integrado todavía.
- No se publica `bootloader.bin` precompilado como parte de alpha30.
- No se eliminan periféricos del autoload normal por velocidad.
- No se precompilan librerías internas en alpha30.
- `platform.txt` se mantiene sin cambios para esta optimización.

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

### Ejemplo: entrada a salida

```cpp
void setup()
{
    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);

    digitalWrite(Q0_0, LOW);
}

void loop()
{
    bool inputState = digitalRead(I0_0);
    digitalWrite(Q0_0, inputState ? HIGH : LOW);
}
```

### Ejemplo: espejo por bloque

```cpp
void setup()
{
}

void loop()
{
    uint32_t inputs = JWPLC_readInputs();
    JWPLC_writeOutputs(inputs);
}
```

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

## READMEs de librerías

Para documentación más detallada de cada componente, revisa los README específicos de las librerías del ecosistema JWPLC.

### Librerías internas del package JWPLC

| Librería | Descripción | README |
|---|---|---|
| `JWPLC_Display` | Manejo de TFT ST7789, pantallas IDLE/USER, callbacks de HMI e indicadores visuales. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/JWPLC-2.0.0/libraries/JWPLC_Display/README.md) |
| `JWPLC_Ethernet` | Integración del W5500, DHCP/static IP, reconexión, estado de link y coexistencia SPI. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/JWPLC-2.0.0/libraries/JWPLC_Ethernet/README.md) |
| `JWPLC_RS485` | API nativa para usar el puerto RS-485 físico del JWPLC Basic sobre `Serial2`. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/JWPLC-2.0.0/libraries/JWPLC_RS485/README.md) |
| `JWPLC_ModbusRTU` | Modbus RTU base sobre `JWPLC_RS485`, con soporte inicial master/slave. | [Ver README](https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/README.md) |

> `JWPLC_GlobalPeripherals` no tiene README independiente por ahora. Su función principal es agrupar y exponer los objetos globales del ecosistema JWPLC, como `JWPLC_Display`, `JWPLC_Ethernet`, `JWPLC_SD`, `JWPLC_FRAM`, `JWPLC_RTC`, `JWPLC_RS485` y `JWPLC_ModbusRTU`.

### Librerías JW externas / distribuibles

| Librería | Descripción | README |
|---|---|---|
| `JW_FRAM` | Librería SPI FRAM con API tipo EEPROM para guardar variables, estructuras, strings y configuraciones. | [Ver README](https://github.com/JW-Control/JW_FRAM/blob/main/README.md) |
| `JW_RTC` | Librería para RTC DS3232M / DS3232 con manejo de fecha/hora, timestamp Unix, temperatura, alarmas y NVRAM. | [Ver README](https://github.com/JW-Control/JW_RTC/blob/main/README.md) |
| `JW_SD` | Wrapper para microSD basado en `SD`, pensado para trabajar de forma segura con bus SPI compartido. | [Ver README](https://github.com/JW-Control/JW_SD/blob/main/README.md) |
| `JW_MatrixButtons` | Librería para lectura de botonera matricial con debounce, eventos, repeat y helpers de navegación HMI/PLC. | [Ver README](https://github.com/JW-Control/JW_MatrixButtons/blob/main/README.md) |
| `JW_DWIN_RS485` | Librería para comunicación con pantallas DWIN por RS-485 / Modbus RTU. | [Ver README](https://github.com/JW-Control/JW_DWIN_RS485/blob/main/README.md) |

> En `JWPLC Basic`, varias de estas librerías se usan a través de objetos globales ya integrados al runtime. Para uso normal del PLC, el usuario no necesita inicializar manualmente todos los periféricos ni recordar pines internos del hardware.

---

## Display integrado

`JWPLC_Display` se inicializa automáticamente en placas compatibles y muestra una pantalla `IDLE` con estado general del equipo.

La API recomendada usa estilo objeto con punto:

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
JWPLC_Display.setIdleTimeoutMs(8000);
JWPLC_Display.setRunLed(true);
```

Modos de retorno desde pantalla `USER` a `IDLE`:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
```

Para usar funciones de configuración, estado o LEDs del display no hace falta incluir librerías manualmente.

Para dibujar directamente sobre la TFT con métodos de Adafruit ST7789, sí se debe incluir:

```cpp
#include <JWPLC_Display.h>
```

Esto es necesario cuando se usa:

```cpp
auto &tft = JWPLC_Display.tft();
```

### Recomendación para callbacks gráficos

En callbacks del display, evita hacer operaciones pesadas o consultas directas a periféricos SPI. Lo recomendado es:

1. Leer Ethernet, SD o FRAM desde `loop()`.
2. Guardar resultados en variables simples.
3. Dibujar en pantalla usando esas variables ya cacheadas.

---

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

- Inicializar Ethernet automáticamente.
- Evitar bloqueos largos cuando no hay RJ45 conectado.
- Reintentar al conectar RJ45 después del arranque.
- Mantener DHCP.
- Proteger el bus SPI compartido.
- Actualizar el indicador `ETH` en pantalla IDLE.

### LED ETH en IDLE

| Condición | LED ETH |
|---|---|
| Ethernet disabled / Basic Core | Apagado |
| RJ45 desconectado / Link OFF | Apagado |
| Ethernet OK | Verde |
| Falla real de Ethernet | Rojo |

---

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

---

## Modbus RTU base

El package incluye base Modbus RTU sobre RS-485.

```cpp
JWPLC_ModbusRTU.begin(); // Slave ID 1, 19200, SERIAL_8E1
```

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

---

## Mapa preliminar para OpenPLC / Modbus

La integración OpenPLC queda para una alpha/beta posterior, pero el mapa preliminar recomendado es:

| Capa OpenPLC / Modbus | JWPLC Basic | Descripción |
|---|---|---|
| Discrete Inputs | `I0_0` a `I0_7` | Entradas digitales físicas |
| Coils | `Q0_0` a `Q0_7` | Salidas digitales tipo relé |
| Holding Registers | Variables internas | Setpoints, parámetros, comandos |
| Input Registers | Estados internos | Diagnóstico, contadores, mediciones |

Dirección base recomendada para Modbus:

| Dirección Modbus base 0 | Dirección estilo 40001/00001 | Señal JWPLC |
|---:|---:|---|
| Discrete Input 0 | 10001 | `I0_0` |
| Discrete Input 1 | 10002 | `I0_1` |
| Coil 0 | 00001 | `Q0_0` |
| Coil 1 | 00002 | `Q0_1` |
| Holding Register 0 | 40001 | Variable interna 0 |
| Input Register 0 | 30001 | Estado interno 0 |

> OpenPLC no está integrado todavía. Este mapa es una base preliminar para futuras pruebas.

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

No se publica `bootloader.bin` precompilado en alpha30. Se mantiene la generación automática desde:

```txt
bootloader_qio_40m.elf
```

---

## OTA

OTA no está integrado todavía en JWPLC Basic v2.0.0 alpha30.

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

El uso formal de coredump queda pendiente de validación y documentación en una alpha posterior.

---

## Ejemplos recomendados para validar instalación

### I/O industrial

- `DigitalIO_InputMonitor`
- `DigitalIO_OutputSequence`
- `DigitalIO_InputToOutput`
- `DigitalIO_AllStatus_Serial`
- `DigitalIO_BlockMirror`
- `DigitalIO_BlockRead`

### Display

- `Display_DotAPI_Minimal`
- `Display_Idle_Return_Modes`
- `Display_UserUI_Callbacks`
- `Display_Efficient_Redraw`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Auto_StaticIP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

### FRAM

- `FRAM_BootCounter`
- `FRAM_ReadWrite`
- `FRAM_ConfigStorage`

### microSD

- `SD_FileList`
- `SD_ReadWrite`
- `SD_DisplayBrowser`

### RTC

- `RTC_ReadTime`
- `RTC_SetTime`
- `RTC_DisplayClock`

### RS-485

- `RS485_Status`
- `RS485_Basic_Send`
- `RS485_Basic_Echo`
- `RS485_USB_Bridge`

### Modbus RTU

- `ModbusRTU_CRC_Test`
- `ModbusRTU_Slave_HoldingRegisters`
- `ModbusRTU_Master_ReadHoldingRegisters`
- `ModbusRTU_Master_WriteSingleRegister`

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
- Probar FRAM con contador de arranque.
- Probar coexistencia Ethernet + SD + FRAM + Display.
- Probar RS-485 con bridge USB ↔ RS-485.
- Probar Modbus RTU como slave y master.

---

## Estructura principal

```txt
JWPLC/
  package_jwplc_index.json
  JWPLC-2.0.0/
    boards.txt
    platform.txt
    cores/
    variants/
    libraries/
```

---

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.
