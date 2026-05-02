# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, optimizado para el ecosistema **JWPLC Basic**.

El objetivo del package es ofrecer una experiencia industrial más directa que el core ESP32 genérico: menos variantes innecesarias, APIs de alto nivel y periféricos integrados al runtime del JWPLC.

---

## Enfoque del package

A partir de la etapa `alpha27`, el package vuelve a su enfoque compacto:

- **ESP32 Board** para proyectos ESP32 genéricos.
- **JWPLC Basic** para el hardware completo con periféricos integrados.
- **JWPLC Basic Core** para validación/core esencial sin periféricos opcionales.

No se mantiene soporte visible para ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2 u otras familias dentro del package JWPLC mientras no sean requeridas por un producto real. Esto reduce peso, simplifica mantenimiento y evita confusión en Arduino IDE.

---

## Instalación

En Arduino IDE, ingresa este enlace en **Archivo > Preferencias > Gestor de URLs adicionales de tarjetas**:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Luego abre **Herramientas > Placa > Gestor de tarjetas**, busca `JW Control ESP32 Boards` e instala la versión disponible.

---

## Placas objetivo

| Placa | Uso recomendado | Periféricos esperados |
|---|---|---|
| ESP32 Board | Desarrollo ESP32 genérico dentro del package JWPLC | Sin periféricos JWPLC automáticos. |
| JWPLC Basic | Hardware completo JWPLC Basic | I/O industrial por TCA6424A, Display, botonera, RTC, FRAM, microSD, Ethernet, RS-485 y Modbus RTU base. |
| JWPLC Basic Core | Validación del core y pruebas sin periféricos opcionales | I/O industrial según variante/configuración; periféricos opcionales pueden reportar disabled. |

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

> Nota: en **JWPLC Basic Core**, mensajes como `Ethernet disabled`, `SD disabled` o `FRAM size: 0` son esperados si esa variante fue compilada sin esos periféricos.

---

## I/O industrial nativo por TCA6424A

Una de las bases del package JWPLC es que el expansor **TCA6424A** queda integrado al core para que las entradas y salidas industriales se usen con funciones estándar de Arduino:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

bool state = digitalRead(I0_0);
digitalWrite(Q0_0, state);
```

El usuario no necesita usar manualmente Wire/I2C ni escribir código directo para el TCA6424A.

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

---

## APIs globales del ecosistema JWPLC

En JWPLC Basic, el package expone objetos globales para trabajar con sintaxis directa:

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

---

## Ethernet integrado

Desde `2.0.0-alpha.26`, Ethernet W5500 queda integrado al runtime del JWPLC Basic.

En uso normal no es necesario llamar:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

El runtime se encarga de inicializar Ethernet automáticamente, evitar bloqueos largos cuando no hay RJ45 conectado, reintentar al conectar RJ45 después del arranque, mantener DHCP, proteger el bus SPI compartido y actualizar el indicador `ETH` en pantalla IDLE.

### LED ETH en IDLE

| Condición | LED ETH |
|---|---|
| Ethernet disabled / Basic Core | Apagado |
| RJ45 desconectado / Link OFF | Apagado |
| Ethernet OK | Verde |
| Falla real de Ethernet | Rojo |

---

## RS-485 integrado

Desde `2.0.0-alpha.28`, el JWPLC Basic incluye API nativa para RS-485 usando `Serial2`.

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

Desde `2.0.0-alpha.28`, el package incluye base Modbus RTU sobre RS-485.

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

---

## Reglas de coexistencia SPI

El JWPLC Basic comparte SPI entre TFT ST7789, Ethernet W5500, FRAM y microSD.

Regla recomendada para sketches avanzados:

1. Consultar periféricos SPI desde `loop()`.
2. Guardar resultados en variables simples.
3. En callbacks gráficos del display, dibujar solo variables cacheadas.

Evita consultar `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` directamente dentro de callbacks de dibujo.

---

## Ejemplos recomendados para validar instalación

### I/O industrial

- `DigitalIO_InputMonitor`
- `DigitalIO_OutputSequence`
- `DigitalIO_InputToOutput`
- `DigitalIO_AllStatus_Serial`

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

## Checklist rápido antes de una beta

- Instalar el package desde `package_jwplc_index.json`.
- Seleccionar **ESP32 Board** y compilar sketch vacío.
- Seleccionar **JWPLC Basic** y compilar sketch vacío.
- Seleccionar **JWPLC Basic Core** y compilar sketch vacío.
- Probar `pinMode()`, `digitalRead()` y `digitalWrite()` con `I0_x` / `Q0_x`.
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
