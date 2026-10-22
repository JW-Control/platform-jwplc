# Alpha13 / H0 — Causa raíz del TG1WDT en `linkStatus()` y corrección F1

**Tipo:** documento canónico del hallazgo H0 (auditoría independiente Claude Code, READ-ONLY) · **Fecha:** 2026-10-19
**Estado:** causa raíz identificada y confirmada por fuente; la verificación a nivel de objeto (E1) está pendiente. **Corrección F1 aprobada en principio por el propietario**, pendiente de implementarse en una rama aislada. F2 se descarta. Fallos registrados: **F130** (proceso) y **F131** (defecto latente de producto) en [A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md](A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md).
**Firmware auditado:** `f0b9075130158b99d89e0f9a574d964236614b78` (H0-F). Los commits posteriores son solo documentales.
**Evidencia de fuente reproducible:** las 15 fuentes del overlay se recompusieron con las mismas funciones `compose()` del builder, sin `preflight` ni escritura en el repo. Resultado: 15 fuentes y 28 marcas `h0W5100Stage(`, que coincide con los invariantes de `a13_g10_r_build_candidate.py`. `utility/w5100.h` y `JWPLC_W5x00_Ethernet.cpp` no forman parte del overlay.

---

## 1. Qué pasó (resumen en una línea)

`initSS()` ejecutó `pinMode(10, OUTPUT)`. **GPIO10 es la línea IO2 de la flash SPI del ESP32**, y la flash funciona en modo QIO. La CPU deja de poder leer código de la flash y salta el Interrupt Watchdog (Timer Group 1): `rst:0x8 TG1WDT_SYS_RESET`. **No había deadlock.**

## 2. Por qué `ss_pin` valía 10 (hechos de código)

1. `utility/w5100.cpp:20-47` elige el pin por defecto **solo a partir de macros**: `PIN_SPI_SS_ETHERNET_LIB`, `PIN_SPI_SS`, `CORE_SS0_PIN`. Si no encuentra ninguna, usa `#define SS_PIN_DEFAULT 10`.
2. El variant `jwplcbasic/pins_arduino.h:16` declara `static const uint8_t SS = 5;`. Es una **variable** y no una macro, así que el preprocesador no la ve. En todo el package no existe ninguna de esas tres macros. Resultado: `W5100Class::ss_pin = 10` (`w5100.cpp:55`).
3. El único punto que corrige el pin es `Ethernet.init(_csPin)`, dentro de `probeHardware()` (`JWPLC_Ethernet.cpp:121`). Solo se ejecuta desde `service()` (autoload, en `jwplcSystemTask`) o desde `begin()`.
4. `JWPLC_EthernetClass::linkStatus()` (`JWPLC_Ethernet.cpp:529-546`) va directo a `Ethernet.linkStatus()` → `W5100.getLinkStatus()` → `init()` → `initSS()`. En ESP32, `initSS()` es la rama `#else`: `pinMode(ss_pin, OUTPUT)` (`w5100.h:469-472`). El `pinMode()` del core 3.3.8 no protege los pines de flash.
5. **Carrera de arranque.** `jwplcSystemTask` arranca después de `setup()`. Su primer tick Ethernet y el primer `G10R_READY` del sketch se habilitan en el mismo instante, `millis()≥1000` (`main.cpp:467`, sketch `:573`). `loopTask` gira sin pausa; `jwplcSystemTask` solo revisa tras `vTaskDelay(5)`. El sketch llega primero casi siempre.
6. El build corre en QIO: `jwplcbasic.build.boot=qio` y `build.memory_type={build.boot}_qspi`. En QIO, GPIO10 transporta datos de la flash.

**Encaja con los seis ensayos A–F:**
- **A, D, E, F:** el primer READY gana la carrera y falla, con o sin cable RJ45.
- **B:** sin llamadas del sketch, el autoload corrige el pin primero.
- **C:** la llamada tardía encuentra `initialized=true` y no ejecuta `initSS()`.
- **Etapa 62:** confirma que no hubo ningún init previo. El log muestra `IP=0.0.0.0`.
- **Etapa 63:** es justo la última marca antes de `pinMode(10)`.

**Hipótesis descartadas con código:**
- Deadlock: entre 63 y 64 no se toma ningún lock, y una espera de mutex cede la CPU y no dispara un WDT de TG1.
- `SPI.begin()`: es idempotente (`SPI.cpp:205-208`).
- Hardware W5500: en ese tramo no hay transacción SPI.
- Artefacto de instrumentación: A reproduce el fallo sin marcas.

## 3. Corroboración histórica (Alpha12 y anteriores)

- **No es una regresión de Alpha13.** La cadena `SS_PIN_DEFAULT` es el código original de Arduino Ethernet (Paul Stoffregen). Está presente desde **Alpha4** (entonces en `JWPLC_Ethernet_W5x00_Backend/src/utility/w5100.cpp`) y no cambió en Alpha6 ni en Alpha12.
- `w5100.cpp`, `w5100.h`, `JWPLC_Ethernet.cpp`, `JWPLC_W5x00_Ethernet.cpp`, `pins_arduino.h`, `jwplc_spi_bus.h` y `main.cpp` son **idénticos** entre Alpha12 (`main`, `aa0bfed`) y `f0b9075`.
- Alpha12 **tampoco** pasaba el CS en `linkStatus()`. El defecto estaba latente y quedaba enmascarado porque ningún sketch consultaba el enlace antes del primer `service()`. El sketch de laboratorio G10-R fue el primero en hacerlo, en `millis()≈1000`.
- **Es un defecto de producto real:** cualquier sketch de usuario que llame `JWPLC_Ethernet.linkStatus()` o `linkUp()` en `setup()` reinicia la placa al 100 %.

## 4. Lección de método para H0

Las marcas RTC eran correctas y fiables. El error fue de interpretación: durante seis ciclos de flasheo se buscó *dónde se bloqueaba* la ejecución, y nadie revisó *con qué argumento* se llamaba a `initSS()`. Tampoco se aprovechó la firma `TG1WDT` (Interrupt WDT), que por sí sola excluye una espera de mutex. Cuando una marca acota el fallo a una sola instrucción, **lo siguiente es leer esa instrucción y sus argumentos, no crear otra sonda.**

---

## 5. Corrección aprobada: F1 (solo esto)

**Criterio del propietario:** el usuario final nunca edita el variant ni configura pines. El package debe resolverlo internamente. F1 cumple ese criterio:

- Vive en la librería.
- El usuario no toca nada.
- Usa `JWPLC_ETH_CS` (`jwplc_spi_bus.h:24`) como **única fuente de verdad**. Es el mismo `#define` que ya usan `jwplcSPI_configureCSPins()`, `jwplcSPI_deselectAll()` y el `_csPin` por defecto de `JWPLC_EthernetClass`.
- **Si una revisión de hardware cambia el pin físico del W5500, basta cambiar ese único `#define`.**

**Archivo:** `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp`. No tocar el variant.

Antes de la cadena `SS_PIN_DEFAULT` (línea ~20):
```cpp
// JWPLC PATCH: el variant define SS como 'static const', no como macro.
// Sin esto SS_PIN_DEFAULT cae a 10 (GPIO10 = SD3/SPIWP de la flash ESP32)
// y initSS() lo reconfigura como GPIO -> TG1WDT_SYS_RESET.
#if !defined(PIN_SPI_SS_ETHERNET_LIB) && __has_include("jwplc_spi_bus.h")
#include "jwplc_spi_bus.h"
#define PIN_SPI_SS_ETHERNET_LIB JWPLC_ETH_CS
#endif
```

Después de la cadena (línea ~47), una guarda para que la regresión **no compile** en lugar de reiniciar la placa:
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32) && (SS_PIN_DEFAULT >= 6) && (SS_PIN_DEFAULT <= 11)
#error "SS_PIN_DEFAULT apunta a un pin de la flash SPI del ESP32 (GPIO6-11)"
#endif
```

**Qué NO cambia:** APIs, semántica de `linkStatus()` (sigue leyendo el PHY real), autoload, mutex, periféricos, rendimiento (cero accesos extra a SPI o al mutex), Arduino IDE ni Alpha15.
**Rollback:** revertir un único commit de unas 10 líneas.

**Efecto esperado:** si el sketch consulta antes del autoload, `initSS()` hace `pinMode(5, OUTPUT)`. Es inocuo, porque GPIO5 ya es salida en HIGH desde `jwplcSPI_configureCSPins()`. `W5100.init()` detecta el W5500 bajo el mutex y `linkStatus()` devuelve el PHY real. Después, `probeHardware()` encuentra `initialized=true` y continúa como siempre.

### F2 (descartada)

F2 consistía en añadir `Ethernet.init(_csPin)` en cada `linkStatus()`. Se descarta:
- Alpha12 no lo hacía.
- Con F1, solo aportaría algo si alguien llamara `configure()` con un CS distinto de 5 antes del primer `service()`. En JWPLC Basic el CS es fijo por hardware, así que no es un caso normal.
- Su costo sería casi nulo (una asignación de un byte, sin mutex ni SPI), pero no resuelve nada real.

---

## 6. Validación (sin repetir A–F, sin sondas H0-G)

**Evidencia previa sin flashear:**
- **E1:** sobre el ELF ya compilado de `f0b9075`, ejecutar `xtensa-esp32-elf-gdb -batch -ex "p/d 'W5100Class::ss_pin'" <elf>`. Debe dar **10**.
- **E2:** buscar `CONFIG_ESPTOOLPY_FLASHMODE_QIO` en `{esp32-arduino-libs}/esp32/qio_qspi/include/sdkconfig.h`. Debe valer `1`.

**N0/N1 (CI, sin hardware):**
- Gate estático: el mapeo `JWPLC_ETH_CS` y la guarda `#error` están presentes. Prueba negativa: si se elimina alguno de los dos, el gate falla.
- Test nativo host con el bloque SS real de `w5100.cpp` y stubs que imiten el variant (`static const SS=5`):
  - Sin `jwplc_spi_bus.h`: debe dar **10** (reproduce el bug).
  - Con F1: debe dar **5**.
  - Con `-DPIN_SPI_SS_ETHERNET_LIB=10 -DCONFIG_IDF_TARGET_ESP32`: debe **fallar la compilación**.

**N2/N3:**
- Compilación source-first de `jwplcbasic` con el overlay de 15 fuentes. Verificar `ss_pin==5` en el ELF con el mismo comando de E1.
- Compilar también `jwplcbasiccore`, la matriz N3 de 11 sketches (Windows y Ubuntu) y los 5 workflows en verde.

**Física mínima (requiere autorización aparte; solo COM4, COM14 no se toca):**
1. Sketch H0-F **sin cambios**: el primer READY debe mostrar `LINK=1 H0_ATTEMPT=1 H0_STAGE=50`, sin `rst:0x8`.
2. Sketch A original (`linkStatus()` en cada READY y HEARTBEAT) ≥ 15 min sin WDT. Con RJ45 desconectado y reconectado, `LINK` alterna entre 1 y 2, el DHCP se recupera y los contadores de TFT/SD/FRAM/RTC/RTU avanzan.
3. 10 arranques en frío y 10 resets por EN: ningún `rst:0x8`.

**Criterio de cierre H0:** E1 da 10 antes del fix y 5 después; N0–N3 en verde; física 1–3 sin `TG1WDT` y con PHY real (no `-9`) en el primer READY. Solo entonces G10-R-H vuelve a revisión, y R1–R4 se desbloquean con autorización separada.

---

## 7. Hallazgo colateral: la robustez G10 no está en el package

Al comparar `main` (Alpha12) con `f0b9075`:
- **G1–G8 sí se promovieron** al package: 27 archivos, unas 500 líneas.
- **La robustez Ethernet/SPI de G10 existe solo en el overlay temporal de 15 fuentes** que genera `a13_g10_r_build_candidate.py`. Frente al package de `f0b9075` faltan, entre otros:
  - `JWPLC_W5500_SafeAccess.cpp/.h` (457 líneas, no existen en el package).
  - `EthernetClient.cpp` (255 líneas distintas), `EthernetUdp.cpp` (104), `EthernetServer.cpp` (95).
  - `JWPLC_ModbusTCP.cpp` (104), `JWPLC_ModbusTCP_Client.cpp` (70).
  - Cambios en `socket.cpp`, `JWPLC_Ethernet.cpp` y `JWPLC_W5x00_Ethernet.h`.
- En el mismo intervalo, `tools/` y `docs/` crecieron ~71.000 líneas.

**Implicación:** hoy, un usuario que instale el package desde Arduino IDE **no recibe** la robustez G10 validada en banco. Es el mismo patrón que hizo perder avance en Alpha12: lo validado se quedó como temporal. **Requisito para cerrar Alpha13:** promover al package las piezas G10 que ya tienen validación, aplicar F1 en el package (no solo en el overlay) y validar el **package instalado**, no el overlay. La instrumentación H0 (marcas 10–77, `g10rH0LinkStage`) **no** debe llegar al producto.

## 8. Reglas para el siguiente paso

- No crear más sondas H0, no usar `LINK=-9` como solución, no editar el variant y no quitar periféricos ni el autoload.
- Rama nueva aislada desde `f0b9075` para F1. PR en Draft. No flashear sin autorización explícita. No modificar Alpha15.
- Documentación y PR en español. No incluir MACs.
