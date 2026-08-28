# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.6 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es que el hardware pueda utilizarse con una experiencia cercana a Arduino sin perder las funciones propias del controlador: E/S industriales, TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485 y Modbus RTU integrados al runtime del package.

## Estado de versiones

| Canal | Versión | Estado |
|---|---|---|
| Público / estable | `v2.0.0` | Recomendado para proyectos estables. |
| Dev publicada | `v2.1.0-alpha.5` | Última PreRelease publicada antes del trabajo Alpha6. |
| Rama actual | `v2.1.0-alpha.6` | En cierre técnico/documental; todavía no debe asumirse publicada hasta completar release. |

Alpha6 mantiene las optimizaciones de compilación de Alpha4/Alpha5 y se concentra en robustecer el runtime de comunicaciones y diagnóstico sin retirar periféricos del autoload normal.

## Qué incluye JWPLC Basic

El perfil completo mantiene:

- 8 entradas digitales industriales;
- 8 salidas digitales por relé;
- TFT ST7789;
- botonera frontal de 6 teclas;
- RTC;
- FRAM de 8 KiB;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- TCA / I/O industrial;
- arbitraje del bus SPI compartido.

APIs principales expuestas por el ecosistema:

```cpp
JWPLC_Display
JWPLC_Ethernet
JWPLC_RTC
JWPLC_FRAM
JWPLC_SD
JWPLC_Buttons
JWPLC_RS485
JWPLC_ModbusRTU
```

Las librerías lógicas experimentales se incluyen de forma separada:

```text
JWPLC_LogicRuntime
JWPLC_LogicRuntime_UI
```

## E/S industriales

Las E/S físicas se usan con nombres lógicos:

```text
I0_0 ... I0_7
Q0_0 ... Q0_7
```

Ejemplo Arduino:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

También existen operaciones por banco:

```cpp
uint32_t inputs = JWPLC_readInputs();
uint32_t outputs = JWPLC_readOutputs();
JWPLC_writeOutputs(0x0000000F);
```

## Display e indicadores IDLE

`JWPLC_Display` gestiona la TFT integrada, las pantallas `IDLE`/`USER`, callbacks gráficos y la coexistencia SPI.

El IDLE muestra:

```text
PWR
RUN
ERR
BUS
ETH
```

Alpha6 diferencia claramente sus responsabilidades:

- `ERR`: error definido por la aplicación, con código alfanumérico de hasta 4 caracteres;
- `BUS`: diagnóstico RS-485/Modbus RTU con código corto;
- `ETH`: diagnóstico Ethernet con código corto.

Ejemplos:

```cpp
JWPLC_Display.setErrCode("A01");
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

Códigos BUS actuales:

```text
--- DIS INI SER SID MAP TMO CRC EXC RSP OVF FUN
```

Códigos ETH actuales:

```text
--- DIS INI PHY LNK DHC HW IP SPI
```

Documentación: [`JWPLC_Display`](JWPLC/2.1.0/libraries/JWPLC_Display/README.md)

## Ethernet W5500 — Alpha6

`JWPLC_Ethernet` usa un runtime cooperativo/no bloqueante para:

- sondeo del W5500;
- link RJ45;
- DHCP inicial;
- IP estática;
- recuperación tras desconectar/reconectar;
- reintento sin reset;
- mantenimiento DHCP T1 renew / T2 rebind;
- coexistencia SPI con TFT, FRAM y microSD.

El autoload utiliza:

```cpp
JWPLC_Ethernet.service();
```

Las APIs síncronas `begin()` y `maintain()` se conservan por compatibilidad, pero no son la base del autoload Alpha6.

Estados del runtime:

```text
NOT_STARTED
PROBING
PHY_READY
LINK_OFF
DHCP_PENDING
READY
ERROR
```

En los gates físicos se validaron DHCP, IP estática, servidor HTTP, stress SPI, router→red sin DHCP→router y mantenimiento T1/T2 conservando un lease válido cuando corresponde.

Documentación: [`JWPLC_Ethernet`](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md)

## RS-485 y Modbus RTU

`JWPLC_RS485` expone el transporte industrial y telemetría de actividad. No se autoinicia con parámetros arbitrarios; la aplicación o la capa Modbus define baudrate/configuración.

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

`JWPLC_ModbusRTU` implementa actualmente:

### Slave

| FC | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

### Master

```cpp
readHoldingRegisters();
writeSingleRegister();
```

La integración `BUS` muestra actividad y errores como `TMO`, `CRC`, `EXC` o `RSP` sin apropiarse de `ERR`.

El gate físico Alpha6 verificó timeout rojo `TMO` con peer ausente y recuperación a lecturas correctas/actividad verde al iniciar el segundo JWPLC sin reset del master.

Documentación:

- [`JWPLC_RS485`](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md)
- [`JWPLC_ModbusRTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md)

## Periféricos globales

`JWPLC_GlobalPeripherals` es la capa de unión del package. Expone objetos globales, IDs de botonera y helpers comunes; no sustituye a los drivers individuales.

```text
JWPLC_RTC
JWPLC_FRAM
JWPLC_Buttons
JWPLC_SD
JWPLC_Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
```

Documentación: [`JWPLC_GlobalPeripherals`](JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/README.md)

## RTC, FRAM, microSD y botonera

Los drivers cuyo nombre comienza con `JW_` se mantienen como librerías reutilizables en el repositorio **JW_Libraries**. Este repositorio integra sus versiones dentro del package, pero su README fuente debe actualizarse en el repositorio correspondiente para evitar documentación duplicada.

Dentro de `platform-jwplc`, `JWPLC_GlobalPeripherals` realiza la integración con el runtime y expone los objetos globales usados por el sketch.

## Logic Runtime experimental

El package contiene una línea de investigación propia para lógica tipo PLC/FBD que **no equivale a OpenPLC integrado**.

### `JWPLC_LogicRuntime`

Mantiene dos fronteras:

- runtime v1 con persistencia/FRAM e integración de I/O;
- motor v2 con contrato explícito RAM-only usado por el editor FBD experimental.

El motor v2 todavía no debe asumirse con persistencia FRAM, retentividad o salidas físicas finales.

### `JWPLC_LogicRuntime_UI`

Conecta runtime/motor con TFT y botonera. El branch actual incluye evolución del mapa FBD, edición transaccional en RAM, editor TON, mini mapa, refresco regional y consolidación experimental de renderer.

Su metadata continúa declarando `0.5.8`; el trabajo posterior contenido en el branch no debe confundirse con una nueva versión publicada de la librería.

Documentación:

- [`JWPLC_LogicRuntime`](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime/README.md)
- [`JWPLC_LogicRuntime_UI`](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime_UI/README.md)

## Librerías `JWPLC_` del package

| Librería | Rol | Estado resumido |
|---|---|---|
| `JWPLC_Display` | TFT, IDLE/USER, ERR/BUS/ETH | Producción Alpha6; archive precompilado validado. |
| `JWPLC_Ethernet` | W5500, DHCP/IP, link, recuperación | Runtime cooperativo Alpha6 validado. |
| `JWPLC_RS485` | Transporte RS-485 | API estable dentro del ciclo 2.1.0. |
| `JWPLC_ModbusRTU` | Modbus RTU base master/slave | Alcance funcional acotado y validado. |
| `JWPLC_GlobalPeripherals` | Objetos globales/autoload | Capa interna de integración. |
| `JWPLC_LogicRuntime` | Motor lógico v1/v2 | Experimental; v1 persistente + v2 RAM-only. |
| `JWPLC_LogicRuntime_UI` | UI/Editor FBD | Experimental; migración/consolidación en curso. |

## Precompilación y tiempos de build

El ciclo 2.1.0 reduce compilaciones sin retirar funcionalidades. Alpha5 recuperó gran parte de las TUs precompiladas y Alpha6 vuelve a dejar `JWPLC_Display` precompilado después de cerrar sus cambios funcionales.

Para Display, el archive final Alpha6 se regeneró desde los dos objetos fuente actuales y se validó con:

- miembros del archive byte-idénticos a los `.o` fuente;
- cero compilaciones source de Display en modo `precompiled=full`;
- mismo conjunto de símbolos;
- misma RAM;
- diferencia de flash explicada por padding del linker.

La tabla final de tiempos de Alpha6 y la conclusión global de build deben registrarse en la documentación de cierre de la release, no fijarse aquí mientras el alpha siga en proceso.

## Instalación

### Canal estable

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Versión estable pública actual:

```text
2.0.0
```

### Canal dev / PreRelease

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Las PreRelease deben utilizarse únicamente cuando la versión correspondiente ya haya sido publicada en ese índice. La rama Alpha6 actual no debe asumirse disponible por Boards Manager hasta completar su release.

## Placas

| Placa | FQBN | Uso |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Perfil ESP32 genérico del package. |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Perfil completo del JWPLC Basic. |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Perfil de control/validación desde fuente. |

Para desarrollo local se utiliza también el namespace `jwplc_local` según el flujo de validación del repositorio.

## Principios del ciclo 2.1.0

- estabilidad primero;
- compatibilidad Arduino IDE;
- no romper APIs ya validadas;
- no retirar periféricos del autoload normal sólo para ganar velocidad;
- registrar decisiones y resultados físicos;
- no asumir OpenPLC integrado;
- no asumir OTA definida;
- no fijar FlashFreq o bootloader definitivo antes de cerrar su configuración.

## Estado Alpha6

A la fecha de este README ya se han cerrado técnicamente los gates principales de:

- `ERR` alfanumérico de aplicación;
- diagnóstico automático `BUS`;
- diagnóstico automático `ETH`;
- Ethernet cooperativo y recuperación de link;
- DHCP T1/T2 cooperativo;
- build normal sin hooks de prueba DHCP;
- archive final precompilado de `JWPLC_Display`.

Antes de publicar Alpha6 todavía corresponde completar el cierre documental/release y ejecutar el gate final de producción sobre el HEAD definitivo.
