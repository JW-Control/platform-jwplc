# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.11 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde
Arduino IDE y Arduino CLI.

El package integra al runtime del JWPLC los periféricos principales del equipo:

- entradas y salidas industriales;
- TFT y HMI;
- botonera;
- RTC;
- FRAM;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- Modbus TCP;
- TCA/I/O.

No se retiran periféricos del autoload normal sólo para reducir tiempos de
compilación.

> **Nota de publicación:** el marcador oculto
> `JWPLC_RELEASE_VERSION: 2.1.0-alpha.11` sigue apuntando a la última
> PreRelease publicada. El workflow de Auto Release lo consume, por lo que no
> debe cambiarse a Alpha12 hasta el cierre/merge de publicación.

---

# Estado actual

| Canal | Versión | Estado |
|---|---|---|
| Estable | `v2.0.0` | Release pública estable |
| Dev / PreRelease publicada | `v2.1.0-alpha.11` | Publicada y validada |
| En desarrollo | `v2.1.0-alpha.12` | Cierre técnico/package en curso |

Alpha11 permanece como última versión publicada:

```text
ALPHA11_STATUS=CLOSED_PUBLISHED
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
```

Estado actual de Alpha12:

```text
P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=PASS_CLOSED
P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION=PASS_CLOSED
PRECOMPILED_FREEZE=PASS
FINAL_BUILD_SPEED_BENCHMARK=PASS_CLOSED
FINAL_CLI_IDE_UPLOAD_GATES=NEXT
```

Alpha12 todavía **no está publicada**.

---

# Índices de Boards Manager

## Dev / PreRelease

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

## Estable

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

El índice dev no debe apuntar a Alpha12 hasta cerrar package, PR, merge,
PreRelease e instalación aislada.

---

# Boards

## JWPLC Basic

FQBN público:

```text
jwplc:esp32:jwplcbasic
```

FQBN usado durante desarrollo local:

```text
jwplc_local:esp32:jwplcbasic
```

Periféricos del perfil completo:

- 8 entradas digitales industriales;
- 8 salidas por relé;
- TCA6424A / I/O industrial;
- TFT ST7789;
- botonera frontal;
- RTC;
- FRAM;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- arbitraje del SPI compartido.

---

# Alpha12 — mejoras principales

Alpha12 consolida el trabajo de comunicaciones/runtime desarrollado después de
Alpha11.

## Modbus TCP

Nueva librería:

```text
JWPLC_ModbusTCP
```

Incluye:

- Server cooperativo;
- Client cooperativo;
- FC01, FC02, FC03, FC04;
- FC05, FC06, FC15, FC16;
- recuperación de sesión;
- coexistencia con RTU/UDP/Display;
- integración con el Ethernet autoload del JWPLC.

Uso:

```cpp
#include <JWPLC_ModbusTCP.h>
```

Referencia completa de API y ejemplos:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md
```

## Modbus RTU

Alpha12 mantiene compatibilidad e incorpora un motor Master seleccionable:

```cpp
JWPLC_ModbusRTU.motor(ASYNC); // default recomendado
JWPLC_ModbusRTU.motor(SYNC);  // bloqueante
```

API principal:

```text
readCoils()
readDiscreteInputs()
readHoldingRegisters()
readInputRegisters()

writeSingleCoil()
writeSingleRegister()
writeMultipleCoils()
writeMultipleRegisters()
```

También se mantienen las variantes `request...()` y `...Sync()`.

Referencia completa de **todas las funciones públicas** y ejemplos:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md
```

## Ethernet, TCP y UDP

El runtime Ethernet sigue siendo automático.

Objetos principales:

```cpp
JWPLC_Ethernet
EthernetClient
EthernetServer
EthernetUDP
```

Alpha12 endurece internamente:

- DHCP y recuperación;
- conexión/desconexión TCP;
- transmisión cooperativa;
- recepción TCP/UDP;
- W5500;
- coexistencia en SPI compartido.

Las APIs estándar de usuario se mantienen.

La referencia completa documenta también las extensiones async/fast-path,
marcándolas como avanzadas para no confundirlas con la API normal:

```text
JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md
```

## Nuevo backend JWPLC_TFT

`JWPLC_Display` ya no expone públicamente tipos Adafruit/TFT_eSPI.

Dibujo directo:

```cpp
JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);
JWPLC_TFT.fillRect(...);
JWPLC_TFT.drawLine(...);

JWPLC_TFT.setCursor(...);
JWPLC_TFT.print(...);
```

Acceso desde Display:

```cpp
auto &tft = JWPLC_Display.tft();
```

TFT_eSPI está encapsulado dentro del package; el usuario no necesita instalarlo
ni configurarlo.

Referencia completa de primitivas, texto, batching y tipos públicos:

```text
JWPLC/2.1.0/libraries/JWPLC_TFT/README.md
```

## Display / HMI

La API HMI desarrollada en Alpha11 continúa disponible.

Alpha12 migra el backend a `JWPLC_TFT` y conserva:

- páginas;
- TEXT / VALUE / BOOL / BAR;
- PixelMap;
- HMI Designer;
- modo IDLE;
- indicadores RUN / ERR / BUS / ETH;
- dirty rendering.

Referencia:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/README.md
```

## DataLog y runtime

Alpha12 también consolida:

- DataLog buffered;
- autoservicio del almacenamiento;
- coexistencia SD / TFT / Ethernet;
- recuperación de comunicaciones;
- full runtime simultáneo bajo TCP + RTU + UDP.

---

# Inicio rápido de APIs Alpha12

## Modbus RTU Slave

```cpp
uint16_t holding[8] = {};

void setup()
{
    JWPLC_ModbusRTU.setHoldingRegisters(
        holding,
        8);

    JWPLC_ModbusRTU.begin(
        2,
        115200,
        SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

## Modbus RTU Master ASYNC

```cpp
uint16_t values[4] = {};

void setup()
{
    JWPLC_ModbusRTU.begin(
        247,
        115200,
        SERIAL_8N1);

    JWPLC_ModbusRTU.motor(ASYNC);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone())
    {
        JWPLC_ModbusRTU.readHoldingRegisters(
            2, 0, 4, values, 1000);
    }

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            // values[] actualizado.
        }

        JWPLC_ModbusRTU.clearMasterResult();
    }
}
```

## Modbus TCP Server

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t holding[16] = {};

void setup()
{
    JWPLC_ModbusTCP.setHoldingRegisters(
        holding,
        16);

    JWPLC_ModbusTCP.beginServer(
        1,
        502);
}
```

## Modbus TCP Client

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t values[4] = {};

void setup()
{
    JWPLC_ModbusTCPClient.begin(
        IPAddress(192, 168, 1, 50),
        1,
        502);
}

void loop()
{
    JWPLC_ModbusTCPClient.task();

    if (!JWPLC_ModbusTCPClient.busy() &&
        !JWPLC_ModbusTCPClient.done())
    {
        JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
            0, 4, values, 1000);
    }

    if (JWPLC_ModbusTCPClient.done())
    {
        JWPLC_ModbusTCPClient.clearResult();
    }
}
```

## Ethernet DHCP

```cpp
JWPLC_Ethernet.useDHCP();

if (JWPLC_Ethernet.isReady())
{
    Serial.println(
        JWPLC_Ethernet.localIP());
}
```

## UDP

```cpp
EthernetUDP udp;

if (JWPLC_Ethernet.isReady())
{
    udp.begin(5000);

    int packetSize =
        udp.parsePacket();

    if (packetSize > 0)
    {
        uint8_t buffer[64];

        int n =
            udp.read(buffer, sizeof(buffer));
    }
}
```

Enviar:

```cpp
udp.beginPacket(
    IPAddress(192, 168, 1, 100),
    5000);

udp.print("JWPLC");
udp.endPacket();
```

## TFT directa

```cpp
JWPLC_TFT.begin();

JWPLC_TFT.fillScreen(
    JWPLC_TFT_BLACK);

JWPLC_TFT.setCursor(10, 10);
JWPLC_TFT.setTextColor(
    JWPLC_TFT_WHITE);

JWPLC_TFT.println("JWPLC");
```

---

# JWPLC HMI Designer

Alpha11 incorporó **JWPLC HMI Designer V1**, que continúa disponible.

Campos:

```text
TEXT
VALUE
BOOL
BAR
PIXELMAP
MULTIPAGE
```

Genera:

```text
JWPLC_HMI_Generated.h
```

Proyecto recomendado:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

PixelMap incluye:

- RGB565;
- capas;
- brush/eraser;
- fill;
- eyedropper;
- línea;
- rectángulo;
- undo/redo;
- visibilidad runtime;
- codegen optimizado.

LIVE Preview usa Web Serial.

---

# Robustez del runtime

La línea 2.1.x mantiene:

- botonera operativa sin delays artificiales;
- `digitalWrite()` repetitivo sin congelar RTC/Display;
- Ethernet cooperativo;
- recuperación de link;
- Modbus RTU y TCP simultáneos;
- SPI compartido entre Display, Ethernet, SD y otros periféricos.

Alpha12 confirmó coexistencia durante 600 s:

```text
TCP Modbus   = 250.001 req/s
RTU          = 796.953 req/s
RTU scan     = 99.619 scans/s
UDP FAST     = 0.998 Mbps
UDP delivery = 100.000 %
FULL_RUNTIME_CLEAN=YES
```

Estas cifras son evidencia de qualification, no garantías hard-real-time.

---

# Precompilación Alpha12

Alpha12 cerró auditoría global y freeze de precompilados.

## Release-like activos

```text
core.a
JWPLC_Display
JWPLC_ModbusRTU
JWPLC_TFT
JW_SD
SPI
```

## Source-only intencional

```text
JW_RTC
JWPLC_GlobalPeripherals
JWPLC_Ethernet
JWPLC_RS485
```

No se precompilan sólo por perseguir una cifra de benchmark menor.

Artifacts principales:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `core.a` | 3042444 | `78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb` |
| `libJWPLC_ModbusRTU.a` | 292390 | `424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085` |
| `libSPI.a` | 103412 | `b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445` |
| `libJW_SD.a` | 362316 | `1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0` |
| `libJWPLC_Display.a` | 941228 | `c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02` |
| `libJWPLC_TFT.a` | 1091098 | `5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738` |

Inventario completo:

```text
docs/v2.1.0-alpha.12/ALPHA12_PRECOMPILED_FREEZE_20261004.md
```

---

# Benchmark final Alpha12

Ejecutado en la PC principal:

```text
Host        = PC-MASTER-RACE
CPU         = Intel Core i5-13400F
Logical CPU = 16
RAM         = ~23.8 GiB
Arduino CLI = 1.0.2
Jobs        = 0
```

## JWPLC Basic

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 66.983 s | 20 |
| managed warm no-change | 18.004 s | 1 |
| managed warm touch | 17.988 s | 1 |
| explicit cold | 66.986 s | 20 |
| explicit warm no-change | 17.893 s | 1 |
| explicit warm touch | 17.663 s | 1 |

## JWPLC Basic Core

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 75.242 s | 83 |
| managed warm no-change | 17.277 s | 1 |
| managed warm touch | 17.143 s | 1 |
| explicit cold | 71.672 s | 83 |
| explicit warm no-change | 17.282 s | 1 |
| explicit warm touch | 17.215 s | 1 |

Conclusión:

```text
ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS
ALPHA12_WARM_BEST_FORMAL_SERIES=YES
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Comparación:

```text
docs/v2.1.0-alpha.12/ALPHA12_BUILD_SPEED_COMPARISON_20261004.md
```

---

# Modelo de librerías y decisiones vigentes

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE

APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

No se publica `bootloader.bin` como definitivo hasta fijar la configuración
final.

OpenPLC no forma parte del autoload Arduino normal.

---

# Referencias de API

Los README de librería son la referencia de usuario. Deben contener ejemplos
para todas las funciones que formen parte del contrato público soportado.

| Área | Referencia |
|---|---|
| Modbus RTU | `JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md` |
| Modbus TCP | `JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md` |
| Ethernet / TCP / UDP | `JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md` |
| TFT directa | `JWPLC/2.1.0/libraries/JWPLC_TFT/README.md` |
| Display / HMI | `JWPLC/2.1.0/libraries/JWPLC_Display/README.md` |

Los README distinguen entre:

```text
RECOMENDADA
COMPATIBILIDAD
AVANZADA
INTERNA / NO CONTRATO DE USUARIO
```

para que un usuario o una IA no confunda una función interna de qualification
con una API recomendada de aplicación.

---

# Última PreRelease publicada

Hasta cerrar Alpha12:

```text
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
```

Alpha11 fue instalada, compilada, subida y validada en hardware real.

---

# Documentación Alpha12

```text
docs/v2.1.0-alpha.12/ALPHA12_STATUS.md
docs/v2.1.0-alpha.12/ALPHA12_PACKAGE_INVENTORY_20261003.md
docs/v2.1.0-alpha.12/ALPHA12_PACKAGE_CLOSURE_CHECKLIST.md
docs/v2.1.0-alpha.12/ALPHA12_PRECOMPILED_FREEZE_20261004.md
docs/v2.1.0-alpha.12/ALPHA12_BUILD_SPEED_COMPARISON_20261004.md
```

---

# Roadmap posterior

```text
Alpha13 = TFT / Display update
Alpha14 = OpenPLC + mejoras de integración sobre TCP/RTU optimizados
```

La evidencia histórica de algunas mejoras Alpha12 puede aparecer bajo ramas o
documentos originalmente etiquetados Alpha14. Se conserva para trazabilidad,
pero no cambia la numeración final del release.
