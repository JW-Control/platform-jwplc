# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.13 -->

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

> **Canal de desarrollo:** `v2.1.0-alpha.13` es el siguiente package de
> PreRelease, con Ethernet G10 y F137 precompilado integrados en su rama base.
> La disponibilidad efectiva de su ZIP, checksum e índice dev se comprueba en
> [GitHub Releases](https://github.com/JW-Control/platform-jwplc/releases)
> y tras el PR automático hacia `main`. No alterar la versión estable `v2.0.0`.

---

# Estado actual

| Canal | Versión | Estado |
|---|---|---|
| Estable | `v2.0.0` | Release pública estable |
| Dev / PreRelease Alpha13 | `v2.1.0-alpha.13` | G10+F137 integrados; comprobar disponibilidad efectiva en Release e índice dev |
| PreRelease de referencia histórica | `v2.1.0-alpha.12` | Publicación e instalación pospublicación validadas |

Alpha12 conserva la evidencia histórica de publicación e instalación desde el índice dev. Alpha13 integró G10 y los archives Ethernet/Modbus TCP de F137 mediante [PR #125 MERGED](https://github.com/JW-Control/platform-jwplc/pull/125), con package tree `cc17716f173509b1e0f3c0df5eb99549014c1d61`. N3 13/13 Windows+Ubuntu, H-F137 físico acotado (COM4+COM14, FC03 dual, RTU, UDP) y C release-like desde ZIP con ambos FQBN pasaron. **La confirmación de que Alpha13 está publicada y es instalable se consulta en el tag/asset y el índice dev efectivos**, con la evidencia postpublicación registrada en `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md` de la base; un build release-like no equivale a esa comprobación:

```text
ALPHA12_STATUS=CLOSED_PUBLISHED
ALPHA13_SOURCE=G10_F137_INTEGRATED
ALPHA13_DOCS=TUTORIALS_AND_LOCAL_BENCHMARK_COMPLETE_PR126
ALPHA13_LOCAL_PC_BENCHMARK=PASS_3_PAIRS_BOTH_FQBN
ALPHA13_PUBLISHED_STATUS=CONSULT_RELEASE_INDEX_AND_ALPHA13_STATUS
```

Estado actual de Alpha12:

```text
AUTOCONTAINMENT=PASS
P7_POST_AUTOCONTAINMENT=PASS
PRECOMPILED_FREEZE=PASS
FINAL_BUILD_SPEED_BENCHMARK=PASS
FINAL_ARDUINO_CLI_GATE=PASS
FINAL_ARDUINO_IDE_GATE=PASS
PHYSICAL_RETEST=NOT_REQUIRED_BY_SCOPE
ALPHA12_TECHNICAL_CLOSURE=PASS
ALPHA12_RELEASE_PUBLICATION=PASS
ALPHA12_GITHUB_PRERELEASE=PASS
ALPHA12_INDEX_DEV_PR=MERGED
ALPHA12_PUBLISHED_INDEX=PASS
ALPHA12_PUBLISHED_INSTALL=PASS
ALPHA12_PUBLISHED_AUTOCONTAINMENT=PASS
ALPHA12_PUBLISHED_ARCHIVE_PARITY=PASS
ALPHA12_PUBLISHED_CI_FIXES_PRESENT=PASS
ALPHA12_PUBLISHED_COMPILE=PASS
ALPHA12_PUBLISHED_UPLOAD=PASS
ALPHA12_PUBLISHED_RUNTIME=PASS
ALPHA12_STATUS=CLOSED_PUBLISHED
```

Alpha12 ya está publicada y validada desde el package distribuido por el índice
dev. La instalación, compilación, upload y runtime post-publicación quedaron en
PASS sin usar `jwplc_local`.

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

El índice dev incluye `v2.1.0-alpha.12` y fue validado mediante instalación
aislada desde `main`.

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

- Server cooperativo con FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- Client cooperativo con FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
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

# Alpha13 — Robustez, mantenimiento y precompilación

Alpha13 refuerza el funcionamiento del JWPLC Basic **sin retirar periféricos
del autoload ni exigir configuración de hardware en el sketch**.

| Área | Mejora de Alpha13 |
|---|---|
| DNS | Protección ante respuestas truncadas para evitar bucles bloqueantes |
| TCA / salidas | Inicialización segura de EN_IO y operaciones RMW/shadow protegidas entre tareas |
| TFT | Propiedad exclusiva del bus SPI en operaciones batch |
| Ethernet TCP | Lecturas/escrituras parciales y retorno correcto ante errores |
| Ethernet UDP | Tratamiento de datagramas, fragmentación y recuperación |
| FRAM | Lectura segura de cadenas vacías y fallos SPI |
| RS-485 / Modbus RTU | Recepción UART, errores de transporte y prueba Master/Slave real |
| microSD | Corrección de carácter NUL y reconstrucción de archive |
| Runtime | Eliminación de estado privado redundante; README y ejemplos coherentes |
| W5500 / Modbus TCP | Arbitraje SPI, propiedad y reciclaje de sockets, cierre diferido, corrección del CS de W5500 |

Los cambios productivos G10 se integraron en la base de Alpha13 mediante
[PR #124](https://github.com/JW-Control/platform-jwplc/pull/124).
No se habilita OTA ni OpenPLC dentro del autoload Arduino normal.

## Evidencia y límites

- Los gates de compilación y regresión existentes cubren los perfiles
  JWPLC Basic y Basic Core, con librerías elegidas desde el package.
- Dos equipos JWPLC Basic demostraron Modbus RTU real en paralelo con
  Modbus TCP/UDP y Ethernet DHCP.
- R1: 589 solicitudes TCP FC03 válidas por dispositivo y 589 ecos UDP en
  605 segundos; R2: tres recuperaciones de red más dos transiciones PHY
  observadas por UART.
- La inyección interna de R3, el ensayo largo R4 de 1805 segundos y la
  renovación DHCP/DNS no se ejecutaron. Se mantiene esa limitación explícita,
  sin presentar cobertura parcial como PASS global.

## Archives y benchmark de compilación — F137 integrado

**Estado del código:** [PR #125](https://github.com/JW-Control/platform-jwplc/pull/125) fusionado por squash (`23fae3a3`). Los archives canónicos de **JWPLC_Ethernet** y **JWPLC_ModbusTCP** forman parte del package de la rama Alpha13, junto con sus `library.properties` (`precompiled=full`) y la política Ethernet Basic `weak` / Basic Core `strong`. El árbol **funcional de referencia N3** de `JWPLC/2.1.0` es `cc17716f173509b1e0f3c0df5eb99549014c1d61`. Los README educativos de Ethernet y Modbus TCP se ampliaron después: el **tree Git completo del package cambia por documentación**, mientras que el código, `library.properties` y los hashes de ambos `.a` siguen congelados. El gate C comprueba explícitamente esa diferencia documental y vuelve a generar el ZIP.

El archive de Ethernet contiene 12 miembros (SHA-256 `f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4`); el de Modbus TCP contiene 2 (SHA-256 `d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d`). En Basic Core la política fuerte impide el acceso al W5500; no se exige configuración de pines desde el sketch. No se recompilan estos dos `.cpp` en el consumidor con el modo precompilado. Se conservan los demás archives y el autoload normal.

Medianas de compilación **fría en runners GitHub Actions**, fuentes → ambos archives (3 pares por FQBN y sistema):

| Sistema | JWPLC Basic | JWPLC Basic Core |
|---|---|---|
| Ubuntu | 27,245 → 18,976 s (**−30,4 %**) | 35,065 → 26,141 s (**−25,4 %**) |
| Windows | 85,163 → 51,945 s (**−39,0 %**) | 95,054 → 79,908 s (**−15,9 %**) |

La compilación caliente cambió poco; estos números **no** miden velocidad de upload. Método, variabilidad, prueba cruzada y hashes: [benchmark F137](docs/v2.1.0-alpha.13/A13_PRECOMPILE_BENCHMARK_20261020.md). H física de COM4/COM14 = `PASS_FUNCIONAL_ACOTADO`, no incluye Basic Core físico, R3 interno, R4 de 1805 s, DNS/DHCP renewal ni E1/E2. La validación del ZIP final y la instalación desde el índice dev **siguen pendientes** hasta la publicación; un N3 del árbol fuente no las reemplaza.


## Inventario vigente de librerías Alpha13

El package integra librerías desde su propia instalación; el usuario
**no** necesita instalar manualmente copias con el mismo nombre.

| Biblioteca / componente | Modo en Alpha13 | Comentario |
|---|---|---|
| `JWPLC_Ethernet` | **Precompiled full** | Nuevo archive F137 y política Basic/Basic Core |
| `JWPLC_ModbusTCP` | **Precompiled full** | Nuevo archive F137 |
| `JWPLC_ModbusRTU` | Precompilada conservada | Freeze anterior no reconstruido |
| `JWPLC_Display` / `JWPLC_TFT` | Precompiladas conservadas | Backend de TFT administrado |
| `JW_FRAM` / `JW_SD` | Precompiladas conservadas | Ver contratos de uso de cada librería |
| `SPI` / `core.a` | Precompilados del package | No reconstruidos por el cambio F137 |
| `JWPLC_RS485` / `JWPLC_GlobalPeripherals` / `JW_RTC` | Fuentes intencionales | No retiradas del autoload por velocidad |

En la documentación de Alpha13, **precompilado** significa que Arduino
enlaza el archivo `.a` ya construido; no quiere decir que debas configurar
pines, inicializar el W5500 o deshabilitar periféricos. Las librerías
que permanecen en fuentes se compilan normalmente cuando el package
las requiere.

## Aprende Alpha13 según tu nivel

1. **Primer proyecto Ethernet:** sigue
   [JWPLC_Ethernet](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md),
   desde DHCP hasta el ejemplo básico. La clase avanzada al final
   enseña conexión cooperativa y lectura RX diferida **opcional**.
2. **Primer proyecto Modbus TCP:** sigue
   [JWPLC_ModbusTCP](JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md):
   primero un Server y FC03, después registros, Coils y el resto
   de funciones implementadas. No confundas «disponible» con
   «validada físicamente».
3. Para sensores, salidas, botonera o TFT, utiliza los README de
   `JWPLC_GlobalPeripherals`, `JWPLC_ModbusRTU`,
   `JWPLC_Display` y `JWPLC_TFT` antes de trabajar con
   interfaces internas.

## Benchmark final de Alpha13 en la PC Windows del operador — PASS

**Estado:** `ALPHA13_LOCAL_PC_BENCHMARK=PASS_3_PAIRS_BOTH_FQBN`.
El operador ejecutó el benchmark en su PC Windows con **Arduino CLI 1.5.1**
sobre el HEAD `158d12a242aa09cf414207c10217954a034a0b68`.
Se usó el mismo sketch de integración G10P y se realizaron **tres
compilaciones frías y tres calientes por variante y FQBN**, comparando
Ethernet y Modbus TCP desde fuentes frente a sus dos archives aprobados.

| Perfil | Fría desde fuentes | Fría con `.a` | Ahorro frío | Caliente fuentes | Caliente con `.a` |
|---|---:|---:|---:|---:|---:|
| JWPLC Basic | 52,120 s | 40,190 s | **11,930 s / 22,89 %** | 9,251 s | 9,055 s |
| JWPLC Basic Core | 54,964 s | 47,958 s | **7,006 s / 12,75 %** | 9,175 s | 9,146 s |

Todas las cifras son **medianas de tres muestras**. Los resultados
anteriores de GitHub Actions corresponden a máquinas distintas;
**no son tiempos de la PC del operador**. Los ahorros calientes
observados (0,196 s en Basic y 0,029 s en Core) son pequeños y
no se presentan como una optimización sostenida universal.

**Verificación de compilación:** el escenario desde fuentes generó 12
objetos CPP de Ethernet y 2 de Modbus TCP; con los dos `.a`
estos recuentos fueron **0 / 0**. Los hashes canónicos de los archives
permanecen intactos, y ninguna otra biblioteca o periférico fue
retirado del autoload.

Documentación y evidencia:
[resultado y las 12 filas originales](docs/v2.1.0-alpha.13/A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md),
[MANIFEST.json original de Windows](docs/v2.1.0-alpha.13/evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json)
y [procedimiento reproducible](docs/v2.1.0-alpha.13/A13_LOCAL_PC_BENCHMARK_PROCEDIMIENTO_20261021.md).

**Límite:** este benchmark demuestra ahorro de **compilación** para
el sketch y host ensayados; no mide subida USB, flashing, arranque ni
instalación desde el índice dev **publicado**. El release-like previo
valida el ZIP candidato, no sustituye el gate postpublicación.


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

# Precompilación Alpha12 — evidencia histórica

Alpha12 cerró auditoría global y freeze de precompilados. **Los cuadros de
esta sección describen Alpha12**, no el inventario final de Alpha13.
En particular, Ethernet fue `source-only` en ese corte histórico;
**en Alpha13 está precompilada** junto con Modbus TCP.

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
| `libJW_FRAM.a` | 126440 | `b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb` |

Inventario completo:

```text
docs/v2.1.0-alpha.12/ALPHA12_PRECOMPILED_FREEZE_20261004.md
```

---

# Benchmark final Alpha12

Medición final post-autocontención:

```text
RESULT_ROOT=tools/alpha12/results/final_build_speed_benchmark/20261005_123241
JOBS=0
UPLOADS=SKIPPED
RESULT_ROW_COUNT=12
PRECOMPILED_FREEZE=PASS
```

## JWPLC Basic

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 69.528 s | 16 |
| managed warm no-change | 17.856 s | 1 |
| managed warm touch | 17.799 s | 1 |
| explicit cold | 59.336 s | 16 |
| explicit warm no-change | 17.781 s | 1 |
| explicit warm touch | 17.751 s | 1 |

## JWPLC Basic Core

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 67.009 s | 79 |
| managed warm no-change | 17.013 s | 1 |
| managed warm touch | 17.037 s | 1 |
| explicit cold | 63.289 s | 79 |
| explicit warm no-change | 17.003 s | 1 |
| explicit warm touch | 17.196 s | 1 |

Respecto al rerun post-P8 anterior:

```text
BASIC_COLD_COMPILER_INVOCATIONS=20 -> 16
CORE_COLD_COMPILER_INVOCATIONS=83 -> 79
COMBINED_COLD_AVG_DELTA=-7.94%
COMBINED_WARM_AVG_DELTA=-2.53%
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

La reducción de cuatro translation units en ambos targets es estructural y
proviene de retirar del package activo los backends gráficos Adafruit legacy,
manteniendo `JWPLC_TFT` como backend del Display. Los tiempos individuales
siguen sujetos al ruido normal del host.

Evidencia:

```text
docs/v2.1.0-alpha.12/ALPHA12_FINAL_BUILD_SPEED_POST_AUTOCONTAINMENT_20261005.md
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

# PreRelease Alpha12 publicada — referencia histórica

```text
TAG=v2.1.0-alpha.12
PUBLISHED_PACKAGE_SOURCE_SHA=1011f2588fe02bdc67b14bef8c33ad3624426cb6
ZIP=jwplc-esp32-2.1.0-alpha.12.zip
SIZE=24383662
SHA256=412079a9e01cb0eaccdf6ec530b04183db1c245eb043e846fe9f0f7e2d5eb1b5
PACKAGE_ROOT=2.1.0/
GITHUB_PRERELEASE=PASS
INDEX_DEV_PR=#101
INDEX_DEV_PR_STATUS=MERGED
ISOLATED_INSTALL=PASS
ISOLATED_COMPILE=PASS
PUBLISHED_UPLOAD=PASS
PUBLISHED_RUNTIME=PASS
```

Alpha11 continúa siendo la última versión con cierre físico post-publicación
completo hasta terminar los gates de Alpha12.

---

# Documentación Alpha12

```text
docs/v2.1.0-alpha.12/ALPHA12_STATUS.md
docs/v2.1.0-alpha.12/ALPHA12_PACKAGE_INVENTORY_20261003.md
docs/v2.1.0-alpha.12/ALPHA12_PACKAGE_CLOSURE_CHECKLIST.md
docs/v2.1.0-alpha.12/ALPHA12_PRECOMPILED_FREEZE_20261004.md
docs/v2.1.0-alpha.12/ALPHA12_BUILD_SPEED_COMPARISON_20261004.md
docs/v2.1.0-alpha.12/ALPHA12_TECHNICAL_CONCLUSION_20261005.md
docs/v2.1.0-alpha.12/PRE_RELEASE.md
docs/v2.1.0-alpha.12/ALPHA12_PUBLISHED_VALIDATION_20261005.md
```

---

# Roadmap posterior

```text
Alpha13 = cleanup, robustez SPI/Ethernet/TCP/UDP, FRAM, RTU y periféricos
Alpha14 = roadmap independiente (no forma parte del alcance de Alpha13)
```

La evidencia histórica de algunas mejoras Alpha12 puede aparecer bajo ramas o
documentos originalmente etiquetados Alpha14. Se conserva para trazabilidad,
pero no cambia la numeración final del release.
