# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.7 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es que el hardware pueda utilizarse con una experiencia cercana a Arduino sin perder las funciones propias del controlador: E/S industriales, TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485 y Modbus RTU integrados al runtime del package.

## Estado de versiones

| Canal | Versión | Estado |
|---|---|---|
| Público / estable | `v2.0.0` | Recomendado para proyectos estables. |
| Dev publicada | `v2.1.0-alpha.7` | PreRelease validada y publicada. |
| Siguiente alpha | `v2.1.0-alpha.8` | Pendiente de iniciar formalmente. |

Alpha7 parte de Alpha6 sin retirar periféricos del autoload normal. Este cierre consolida Modbus RTU cooperativo y multidrop, Remote I/O, robustez Ethernet/W5500 ante contención SPI y la integración externa OpenPLC/VPP trabajada durante el alpha.

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

Alpha6 consolidó la separación de responsabilidades que se mantiene en Alpha7:

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

## Ethernet W5500 — Alpha6/Alpha7

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

Las APIs síncronas `begin()` y `maintain()` se conservan por compatibilidad, pero no son la base del autoload.

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

Alpha7 corrige además un falso `LINK_OFF` que podía producirse cuando el mutex SPI estaba ocupado temporalmente. Un `BUS_LOCK_TIMEOUT` ya no se interpreta como desconexión física: `READY` se conserva mientras el cable siga conectado.

Gate dirigido Alpha7:

```text
FORCED_BUS_LOCK_TIMEOUTS=12
FALSE_LINK_OFFS=0
READY_DROPS=0
FINAL_READY=YES
FINAL_LINK=ON
ALPHA7_ETH_SPI_CONTENTION=PASS
```

También se revalidó la recuperación Router -> laptop sin DHCP -> Router sin RESET.

Documentación: [`JWPLC_Ethernet`](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md)

## RS-485 y Modbus RTU

`JWPLC_RS485` expone el transporte industrial y telemetría de actividad. No se autoinicia con parámetros arbitrarios; la aplicación o la capa Modbus define baudrate/configuración.

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

`JWPLC_ModbusRTU` implementa actualmente Slave y Master sobre el transporte RS-485 del JWPLC Basic.

### Master cooperativo — API recomendada

```cpp
JWPLC_ModbusRTU.requestReadHoldingRegisters(...);
JWPLC_ModbusRTU.requestWriteSingleRegister(...);

void loop()
{
    JWPLC_ModbusRTU.task();
    // resto de la aplicación
}
```

Las solicitudes retornan de inmediato. La aplicación consulta `masterBusy()`, `masterDone()`, `masterSucceeded()` y `masterResult()` para seguir el estado de la transacción.

La llamada frecuente a `task()` forma parte del contrato del Master cooperativo. OpenPLC/Backplane/Remote I/O deben utilizar esta ruta para evitar que un timeout detenga el ciclo de aplicación.

### Master síncrono — uso explícito

```cpp
JWPLC_ModbusRTU.readHoldingRegistersSync(...);
JWPLC_ModbusRTU.writeSingleRegisterSync(...);
```

Estas variantes pueden bloquear hasta recibir respuesta o vencer el timeout y se reservan para commissioning, pruebas o sketches donde esa espera sea aceptable.

Alpha7 corrigió el framing multidrop observado con M2 + S1 + S2: request y response de otro nodo podían quedar acumuladas en el FIFO UART y ser interpretadas como una sola trama. El parser actual separa ADU por estructura Modbus antes de validar CRC.

El gate físico multidrop cerró con CRC=0 y exceptions=0 en ambos Slaves. Remote I/O A7.1 validó FC01, FC02, FC05 y FC15, fail-safe, pérdida de bus, reset del Slave y recuperación sin reiniciar el Master.

La integración `BUS` muestra actividad y errores como `TMO`, `CRC`, `EXC` o `RSP` sin apropiarse de `ERR`.

Documentación:

- [`JWPLC_RS485`](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md)
- [`JWPLC_ModbusRTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md)
- [`Alpha7 Modbus RTU`](docs/v2.1.0-alpha.7/MODBUS_RTU_MASTER_COOPERATIVO.md)
- [`Alpha7 Remote I/O A7.1`](docs/v2.1.0-alpha.7/REMOTE_IO_A7_1_VALIDATION.md)

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

Conecta runtime/motor con TFT y botonera. Su metadata continúa declarando `0.5.8`; el trabajo posterior contenido en el branch no debe confundirse con una nueva versión publicada de la librería.

Documentación:

- [`JWPLC_LogicRuntime`](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime/README.md)
- [`JWPLC_LogicRuntime_UI`](JWPLC/2.1.0/libraries/JWPLC_LogicRuntime_UI/README.md)

## Librerías `JWPLC_` del package

| Librería | Rol | Estado resumido |
|---|---|---|
| `JWPLC_Display` | TFT, IDLE/USER, ERR/BUS/ETH | Producción Alpha6; archive precompilado validado. |
| `JWPLC_Ethernet` | W5500, DHCP/IP, link, recuperación | Runtime cooperativo Alpha6 + fix de contención SPI Alpha7. |
| `JWPLC_RS485` | Transporte RS-485 | API estable dentro del ciclo 2.1.0. |
| `JWPLC_ModbusRTU` | Modbus RTU master/slave | Multidrop y Master cooperativo Alpha7 validados; archive final regenerado. |
| `JWPLC_GlobalPeripherals` | Objetos globales/autoload | Capa interna de integración. |
| `JWPLC_LogicRuntime` | Motor lógico v1/v2 | Experimental; v1 persistente + v2 RAM-only. |
| `JWPLC_LogicRuntime_UI` | UI/Editor FBD | Experimental; migración/consolidación en curso. |

## Precompilación y tiempos de build

El ciclo 2.1.0 reduce compilaciones sin retirar funcionalidades. Alpha5 recuperó gran parte de las TUs precompiladas y Alpha6 cerró sus archives validados.

Durante el desarrollo de Alpha7, `JWPLC_ModbusRTU` se mantuvo temporalmente en source-build para impedir que un archive anterior ocultara cambios del parser/API. Al cierre se restauró `precompiled=full` y se regeneró el archive final:

```text
Bytes   : 231062
SHA256  : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

JWPLC Basic Core compiló con cero TUs source de Modbus RTU y uso explícito del archive. Ethernet continúa source-build como decisión aceptada desde Alpha6.

Alpha7 no reclama una nueva mejora global de cold build sin un benchmark dedicado adicional.

## OpenPLC / VPP Alpha7

OpenPLC sigue siendo una integración externa al runtime Arduino del package.

Durante Alpha7 se preservó el VPP Alpha18 con reproducibilidad del payload firmado declarado y se integró feedback FC01 en la HAL/VPP.

```text
ALPHA18_VPP_STATUS=CLOSED
SIGNED_PAYLOAD_PHYSICAL=9/9
FRESH_CHECKOUT_SIGNED_PAYLOAD=9/9
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
```

Documentación: [`OPENPLC_BACKPLANE_ALPHA18_VPP.md`](docs/v2.1.0-alpha.7/OPENPLC_BACKPLANE_ALPHA18_VPP.md)

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

Las PreRelease deben utilizarse únicamente cuando la versión correspondiente ya haya sido publicada en ese índice. Alpha7 ya está publicada en el índice dev y disponible para instalación mediante Boards Manager configurado con dicho índice.

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
- evitar bloqueos innecesarios en el runtime PLC;
- no retirar periféricos del autoload normal sólo para ganar velocidad;
- registrar decisiones y resultados físicos;
- no asumir OpenPLC integrado;
- no asumir OTA definida;
- no fijar FlashFreq o bootloader definitivo antes de cerrar su configuración.

## Estado Alpha7

El cierre técnico de Alpha7 queda documentado en:

- [`ALPHA7_CLOSURE_CHECKLIST.md`](docs/v2.1.0-alpha.7/ALPHA7_CLOSURE_CHECKLIST.md)
- [`PULL_REQUEST.md`](docs/v2.1.0-alpha.7/PULL_REQUEST.md)
- [`PRE_RELEASE.md`](docs/v2.1.0-alpha.7/PRE_RELEASE.md)

Marcador de cierre:

```text
ALPHA7_TECHNICAL_CLOSURE=PASS
ALPHA7_PUBLICATION=PASS
ALPHA7_STATUS=CLOSED
NEXT_ALPHA=ALPHA8
```

OpenPLC no forma parte del autoload normal del package y no debe asumirse como runtime Arduino integrado.
