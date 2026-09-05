# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.10 -->

Package personalizado de **JW Control** para programar **JWPLC Basic** desde Arduino IDE y Arduino CLI.

El objetivo es ofrecer una experiencia cercana a Arduino sin perder las funciones propias del controlador: E/S industriales, TFT, botonera, RTC, FRAM, microSD, Ethernet W5500, RS-485 y Modbus RTU integrados al runtime del package.

---

## Estado de versiones

| Canal | Versión | Estado |
|---|---|---|
| Público / estable | `v2.0.0` | Recomendado para proyectos estables. |
| Dev candidata | `v2.1.0-alpha.10` | **Cierre técnico aprobado**. Hotfix de shadowing de `JWPLC_Ethernet`; publicación automática al integrar en `release/v2.1.x`. |
| Siguiente trabajo | `Alpha11` | Configuración RTU del Backplane, referencias FB de timers, source freeze del fork OpenPLC Editor y aislamiento general de librerías con menor coste. |

## Alpha10 - hotfix de shadowing de JWPLC_Ethernet

Alpha10 corrige un caso real observado durante un taller con Arduino IDE: una copia antigua de `JWPLC_Ethernet` instalada en el sketchbook del usuario podía ser seleccionada antes que la copia incluida en el package JWPLC.

El fallo reproducido terminaba en linker error sobre:

```text
JWPLC_EthernetClass::diagnosticCode() const
JWPLC_EthernetClass::runtimeState() const
```

La solución adoptada añade un marker exclusivo del package:

```text
JWPLC_Bundled_JWPLC_Ethernet.h
```

que se carga sólo durante `JWPLC_LIBRARY_DISCOVERY_PHASE`. También se actualiza la metadata de `JWPLC_Ethernet` de `1.0.0` a `1.0.1`.

Validación final:

```text
ETHERNET_HOSTILE_SHADOW_TEST=PASS
JWPLC_ETHERNET_UNIFIED_SELECTION=PASS
FINAL_COMPILE_MATRIX=5/5_PASS
UNDEFINED_REFERENCE_HITS=0
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

### Benchmark de library discovery

Se evaluó proteger 1, 4 y 7 librerías mediante markers bundled. El coste warm creció de forma casi lineal:

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers | 22.094 s | base |
| `JWPLC_Ethernet` únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Alpha10 adopta el mínimo conjunto que resuelve la causa primaria reproducida:

```text
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
GENERALIZED_7_MARKER_OPTION=REJECTED_BUILD_COST
```

No se afirma que Alpha10 blinde todas las librerías JW/JWPLC frente a copias homónimas del sketchbook. Una estrategia general de menor coste queda transferida a Alpha11.

Commit técnico:

```text
c0e5c621cec71977b86becfc8d7acb26ca21e906
```

---

## Base heredada de Alpha9

Alpha9 parte del package publicado Alpha8 y **no retira ningún periférico del autoload normal**.

Su alcance OpenPLC/Backplane queda cerrado en el perfil RTU físicamente validado:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

Alpha9 valida:

- JWPLC Basic Master generado/subido por OpenPLC;
- JWPLC Basic Slave Arduino con `JWPLC_RemoteIO_Slave_RTU`;
- recorrido `FC02 -> Ladder -> FC15 -> FC01`;
- 8/8 bits remotos uno por uno sin cruces;
- persistencia de configuración del Backplane;
- recompilación posterior a reabrir proyecto;
- recuperación automática tras power-cycle del Slave sin resetear el Master;
- VPP JWPLC Basic OpenPLC `2.1.0-alpha.19` con payload Ed25519 9/9 verificado;
- artefactos de taller para `OpenPLC Editor - JWPLC Edition 4.2.8-jwplc.2`.

Artefacto Arduino publicado:

```text
jwplc-esp32-2.1.0-alpha.9.zip
Bytes: 24464282
SHA-256: 015679533e13dabbe79041771e1e85d3011970dd0c69bc62e3b51f3101043907
```

La selección de baudrate/formato desde el Backplane, las referencias tipadas `TON0.Q` / `TOF0.Q` / `TP0.Q`, el source freeze reproducible del fork OpenPLC Editor y la exposición de la HMI Alpha8 hacia Ladder quedan explícitamente pendientes y se transfieren a Alpha11 tras el hotfix Alpha10.

---

# Qué incluye JWPLC Basic

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

APIs principales:

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

Las librerías lógicas experimentales continúan separadas:

```text
JWPLC_LogicRuntime
JWPLC_LogicRuntime_UI
```

No deben confundirse con OpenPLC integrado al runtime Arduino.

---

# E/S industriales

Las E/S físicas se usan con nombres lógicos:

```text
I0_0 ... I0_7
Q0_0 ... Q0_7
```

Ejemplo:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

digitalWrite(Q0_0, digitalRead(I0_0));
```

También existen operaciones por banco:

```cpp
uint32_t inputs = JWPLC_readInputs();
uint32_t outputs = JWPLC_readOutputs();
JWPLC_writeOutputs(0x0F);
```

Alpha8 añade una vista cacheada de alto nivel:

```cpp
uint8_t inputs = JWPLC_IO.inputs();
uint8_t outputs = JWPLC_IO.outputs();

bool i0 = JWPLC_IO.input(0);
bool q0 = JWPLC_IO.output(0);
```

Estas llamadas leen el snapshot ya mantenido por el runtime y no ejecutan una nueva transacción I2C.

---

# RTC cacheado Alpha8

Además del objeto `JWPLC_RTC`, Alpha8 expone:

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

`JWPLC_Time` consume el último snapshot RTC del runtime y resulta útil para HMI o lógica ligera que no necesita forzar otra lectura física.

---

# Botonera

Objeto global recomendado:

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

El runtime mantiene el scan de la botonera. Un sketch JWPLC normal no necesita llamar `JWPLC_Buttons.update()` ni iniciar otro task de scan.

Desde Alpha8, el Display no consume los latches `pressed()` / `released()` de la aplicación para navegar. El sketch y el Display pueden observar una misma acción física sin apropiarse mutuamente del evento de aplicación.

Documentación: [`JW_MatrixButtons`](JWPLC/2.1.0/libraries/JW_MatrixButtons/README.md)

---

# Display, IDLE y HMI Alpha8

`JWPLC_Display` gestiona:

- TFT integrada;
- pantalla automática `IDLE`;
- pantalla `USER`;
- diagnósticos `ERR/BUS/ETH`;
- navegación;
- HMI declarativa Alpha8;
- coordinación SPI del display.

## IDLE seguro por defecto

Alpha8 usa:

```text
IDLE_WAKE_DISABLED
```

como comportamiento por defecto.

La entrada explícita a USER puede hacerse desde el sketch:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    JWPLC_Display.enterUserUI();
}
```

Si una aplicación desea wake automático:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
```

## Campos HMI

Tipos:

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
JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
JWPLC_Display.requestUserRefresh();
```

La HMI soporta hasta 32 campos, varias páginas, formato numérico, colores, layout, alineación, bool text, rango de barra y redibujado dirty-only.

Los valores de páginas no visibles se mantienen cacheados y se dibujan al entrar a la página correspondiente.

## Acceso TFT manual

La API Adafruit sigue disponible cuando se necesita control gráfico directo:

```cpp
auto &tft = JWPLC_Display.tft();
```

Documentación completa: [`JWPLC_Display`](JWPLC/2.1.0/libraries/JWPLC_Display/README.md)

---

# Indicadores IDLE

El IDLE muestra:

```text
PWR
RUN
ERR
BUS
ETH
```

Responsabilidades:

- `ERR`: error de aplicación, hasta 4 caracteres;
- `BUS`: diagnóstico RS-485 / Modbus RTU;
- `ETH`: diagnóstico Ethernet.

Ejemplos:

```cpp
JWPLC_Display.setErrCode("A01");
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

Códigos BUS:

```text
--- DIS INI SER SID MAP TMO CRC EXC RSP OVF FUN
```

Códigos ETH:

```text
--- DIS INI PHY LNK DHC HW IP SPI
```

---

# Ethernet W5500

`JWPLC_Ethernet` mantiene el runtime cooperativo/no bloqueante consolidado en Alpha6 y las correcciones de contención SPI de Alpha7.

Incluye:

- sondeo W5500;
- link RJ45;
- DHCP inicial cooperativo;
- IP estática;
- recuperación sin reset;
- mantenimiento DHCP T1/T2;
- coexistencia SPI con TFT/FRAM/microSD.

El autoload utiliza:

```cpp
JWPLC_Ethernet.service();
```

Las APIs síncronas `begin()` y `maintain()` se conservan por compatibilidad y commissioning.

Alpha10 añade aislamiento de library discovery para `JWPLC_Ethernet` frente a copias homónimas antiguas en el sketchbook, sin modificar su API pública.

Documentación: [`JWPLC_Ethernet`](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md)

---

# RS-485 y Modbus RTU

`JWPLC_RS485` expone el transporte industrial. La aplicación define parámetros como baudrate/configuración:

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

`JWPLC_ModbusRTU` mantiene el Master cooperativo y la ruta Sync explícita.

API cooperativa recomendada:

```cpp
JWPLC_ModbusRTU.requestReadHoldingRegisters(...);
JWPLC_ModbusRTU.requestWriteSingleRegister(...);

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

Alpha7 validó multidrop, Remote I/O y recuperación sin reset del Master. Alpha9 reutiliza esa base y cierra el recorrido OpenPLC Backplane con FC01/FC02/FC15 en hardware real.

Documentación:

- [`JWPLC_RS485`](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md)
- [`JWPLC_ModbusRTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md)
- [`JWPLC Remote I/O Slave RTU`](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/examples/JWPLC_RemoteIO_Slave_RTU/README.md)

---

# Periféricos globales

`JWPLC_GlobalPeripherals` es la capa de integración entre drivers y runtime.

En Alpha8 también aloja las implementaciones de `JWPLC_IO` y `JWPLC_Time` dentro del mismo TU existente para no añadir una compilación extra al cold build.

En Alpha10, `JWPLC_GlobalPeripherals_Auto.h` incorpora únicamente el marker bundled de `JWPLC_Ethernet` durante discovery.

Documentación: [`JWPLC_GlobalPeripherals`](JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/README.md)

---

# Precompilación y build speed

Alpha10 mantiene el trabajo de precompilación de Alpha5/Alpha6/Alpha7/Alpha8.

## Estructura de compilación preservada

Conteos de referencia:

```text
Basic cold: 15 compiladores
Core cold:  78 compiladores
Warm:        1 compilador
```

El benchmark Alpha10 mantuvo paridad estructural y de tamaño binario respecto al baseline:

```text
COMPILER_STRUCTURE_PARITY=PASS
BINARY_SIZE_PARITY=PASS
```

El marker único de `JWPLC_Ethernet` añade en el host medido aproximadamente `+1.233 s / +5.6%` al warm de `01_empty`. La alternativa de siete markers fue descartada por `+8.259 s / +37.4%`.

## Lazy-link HMI

El motor HMI se desacopló del Display base mediante hooks internos.

Gate de link heredado de Alpha8:

```text
01_empty:
  JWPLC_UI engine references = 0
  JWPLC_UI API references    = 0

HMI gate:
  UI engine linked = YES
  UI API linked    = YES
```

El `01_empty` pasó históricamente de:

```text
399696 bytes -> 396240 bytes
```

reduciendo 3456 bytes respecto al estado Alpha8 previo al lazy-link.

La HMI paga su costo de link sólo cuando la aplicación la usa.

---

# Validación física Alpha8

El gate `Display_Alpha8_HMI_Gate` validó:

```text
IDLE_SOAK_180S_NO_AUTOWAKE=PASS
unexpectedUser=0
```

Además:

- los seis botones;
- entrada USER explícita con OK;
- páginas LEFT/RIGHT;
- barra UP/DOWN;
- ESC retorna IDLE y sigue llegando al sketch;
- botones en IDLE no despiertan USER con wake deshabilitado;
- RTC cacheado avanzando;
- I/O cacheado;
- valor/texto/bool/bar;
- refresh sin flicker observado;
- pulsación sostenida sin congelamiento.

El incidente histórico de cuelgues observado en un taller con entornos anteriores se considera resuelto operacionalmente para continuar. Su causa exacta no se reproduce de forma concluyente y no se atribuye retrospectivamente a una única causa demostrada.

---

# OpenPLC / Backplane Alpha9

OpenPLC sigue siendo una integración externa al runtime Arduino del package.

Alpha9 cierra el Backplane/Remote I/O con el perfil fijo:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

Ruta físicamente validada:

```text
Slave DI -> FC02 -> OpenPLC/Ladder -> FC15 -> Slave DO -> FC01 -> feedback
```

Resultados principales:

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
FC02_REMOTE_INPUT_BITS=8/8 PASS
OPENPLC_LADDER_MAPPING=8/8 PASS
FC15_REMOTE_OUTPUT_BITS=8/8 PASS
FC01_OUTPUT_FEEDBACK=8/8 PASS
BIT_POSITION_MAPPING=8/8 PASS
PHYSICAL_CORRELATION=8/8 PASS
CROSSED_BITS=0
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
RTU_AUTOMATIC_RECOVERY=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
```

El banco de prueba sólo permitía activar una entrada a la vez:

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
```

El VPP conservado usa versionado propio:

```text
JWPLC Basic OpenPLC VPP = 2.1.0-alpha.19
Signature               = ed25519 / jwcontrol-2026
Signed payload          = 9/9 PASS
```

No asumir:

```text
OpenPLC integrado al autoload Arduino = NO
Backplane baudrate configurable por UI = NO
Backplane serial format configurable   = NO
```

La HMI Arduino de Alpha8 todavía no está expuesta a Ladder/OpenPLC.

---

# Instalación

## Canal estable

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Versión estable:

```text
2.0.0
```

## Canal dev / PreRelease

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Última versión publicada antes del merge Alpha10:

```text
2.1.0-alpha.9
```

Versión candidata:

```text
2.1.0-alpha.10
```

Estado candidato:

```text
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLICATION=PENDING_PR_CI_RELEASE
```

Para desarrollo local se utiliza el namespace `jwplc_local`.

---

# Placas

| Placa | FQBN | Uso |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Perfil ESP32 genérico del package. |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Perfil completo JWPLC Basic. |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Perfil de control/validación. |

---

# Decisiones del ciclo 2.1.0 que Alpha10 no cambia

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo.

No se retiran periféricos del autoload para acelerar compilación.

---

# Documentación Alpha10

El cierre técnico de Alpha10 se documenta en:

- `docs/v2.1.0-alpha.10/ALPHA10_BUILD_BENCHMARK.md`;
- `docs/v2.1.0-alpha.10/ALPHA10_TECHNICAL_CLOSURE.md`;
- `docs/v2.1.0-alpha.10/ALPHA10_CLOSURE_CHECKLIST.md`;
- `docs/v2.1.0-alpha.10/ALPHA10_TO_ALPHA11_HANDOFF.md`;
- `docs/v2.1.0-alpha.10/PULL_REQUEST.md`;
- `docs/v2.1.0-alpha.10/PRE_RELEASE.md`.

Estado previo a publicación:

```text
ALPHA10_ROOT_CAUSE=CONFIRMED
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
ALPHA10_MINIMUM_SAFE_FIX=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_SCOPED_FIX
ALPHA10_ETHERNET_SELECTION_VERIFIER=PASS
ALPHA10_FINAL_COMPILE_MATRIX=5/5_PASS
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_STATUS=READY_FOR_RELEASE_PR
NEXT=ALPHA11_AFTER_PUBLICATION
```

---

# Documentación Alpha9

El cierre histórico de Alpha9 se documenta en:

- `docs/v2.1.0-alpha.9/ALPHA9_TECHNICAL_CLOSURE.md`;
- `docs/v2.1.0-alpha.9/ALPHA9_CLOSURE_CHECKLIST.md`;
- `docs/v2.1.0-alpha.9/ALPHA9_PUBLICATION_CLOSURE_20260904.md`;
- `docs/v2.1.0-alpha.9/ALPHA9_TO_ALPHA10_HANDOFF.md`;
- `docs/v2.1.0-alpha.9/PULL_REQUEST.md`;
- `docs/v2.1.0-alpha.9/PRE_RELEASE.md`;
- `JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/examples/JWPLC_RemoteIO_Slave_RTU/README.md`.

Alpha9 finalizó publicado y sincronizado; Alpha10 no modifica ese cierre histórico.
