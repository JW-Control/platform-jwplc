# A19 — Librerías por placa: JWPLC Basic (`jwplc`) y ESP32 Board (`jwesp`)

Fecha: `2026-11-28` · Rama: `v2.1.0-alpha.19/JWPLC-Sim/feature/librerias-por-placa`

```text
A19_LIBRERIAS_POR_PLACA=PASS            (arduino-cli 1.3.1, nube)
A19_LIBRERIAS_POR_PLACA_IDE=PENDING     (Arduino IDE 2.3.10 en la PC del propietario)
```

## 1. Pedido del propietario (28/11/2026)

Con **JWPLC Basic** elegido, *Archivo → Ejemplos* no debe mostrar librerías del core ESP32 (ArduinoOTA, BLE,
BluetoothSerial, ESP_NOW, Hash…). Con **ESP32 Board**, no deben salir las librerías `JW_`/`JWPLC_`. ESP32 Board
sigue existiendo. Decisión: dos packages en el **mismo** índice, packagers `jwplc` (Basic) y `jwesp` (genérico).

## 2. Causa `[visto]` en código

- arduino-cli `commands/service_library_list.go` (`LibraryList` con FQBN): solo filtra las librerías de plataforma
  por **plataforma** (la de la placa y la del core referenciado con `build.core=vendor:core`). No hay filtro por
  placa. `architectures=` de `library.properties` solo marca compatibilidad por arquitectura, y las dos placas
  son `esp32`.
- arduino-ide `examples-service-impl.ts`: «Ejemplos para <placa>» = librerías `PLATFORM_BUILTIN` y
  `REFERENCED_PLATFORM_BUILTIN` de esa lista, con al menos un ejemplo.
- Consecuencia: mientras el Basic y ESP32 Board compartan `jwplc:esp32`, cada una ve las librerías de la otra.
  Tampoco sirve que una plataforma referencie el core de la otra, porque hereda también sus librerías. Por eso la
  genérica lleva su propia copia del core (2,5 MB).

## 3. Solución

| Package (gestor de placas) | Packager | Placas visibles | Ejemplos |
|---|---|---|---|
| JWPLC | `jwplc` | JWPLC Basic | solo `JW_` y `JWPLC_` (17 librerías) |
| ESP32 Board | `jwesp` | ESP32 Board (`jwesp:esp32:esp32`) | los originales de arduino-esp32 3.3.8 (42 librerías) |

- **Basic:** `tools/alpha19/ejemplos_solo_jw.sh` borra las carpetas `examples/` de las librerías del core
  (reemplaza a `tools/alpha18/podar_ejemplos_esp32.sh`). Las librerías siguen y compilan: `#include <WiFi.h>` en el
  Basic compila. La lista `EXCEPCIONES` del script deja visibles las que el propietario elija.
- **ESP32 Board oculta en `jwplc`:** `jwplc:esp32:esp32` queda con `hide=true` («ESP32 Board (compatibilidad)»).
  No sale en el menú, pero los scripts que fijan ese FQBN siguen compilando. Hoy lo usa Laundry
  (`jwplc:esp32:esp32:PartitionScheme=no_fs` en `jwplc-laundry-firmware` `tools/compilar.ps1`,
  `tools/grabador/logica.py` y el README del HMI 1.5.0) y el proyecto `jwplc-laundry-c10` de jwplc-dev-mcp.
- **Genérica:** no se versiona aparte. `JWPLC/tools/jwplc_esp32_generico.py` la arma al publicar desde
  `JWPLC/<versión>`: quita las librerías JW/JWPLC, `Adafruit_BusIO`, la variante `jwplcbasic`, el core
  `jwcontrol_precompiled_stub` y `precompiled/`; usa `JWPLC/esp32-generico/boards.txt` (ESP32 Board y ESP32 Family);
  renombra `platform.txt`; y devuelve los ejemplos de arduino-esp32 de la versión de `package.json`, clonados de
  GitHub.
- **Publicación:** `release-jwplc-package.yml` arma la genérica, y `jwplc_release.py prepare --generic-folder`
  genera `jwesp-esp32-<versión>.zip`. El script agrega el packager `jwesp` a los dos índices (sin `tools`
  propios: sus `toolsDependencies` apuntan a las herramientas de `jwplc`) y sube los dos zips a la Release.
  La lista `boards` de cada entrada sale de su `boards.txt` (sin placas ocultas).

## 3b. Solo el JWPLC Basic (decisión del propietario, 28/11)

- **JWPLC Basic Core queda oculta** (`jwplcbasiccore.hide=true`). Solo se vende y se mantiene el Basic: no hay
  unidades Core ni JWPLC I/O. El FQBN sigue compilando, y para mostrarla otra vez basta quitar esa línea. Sale
  también de la tabla de placas del README.
- **Ejemplos `JW_` visibles en el Basic:** los 20 ejemplos de `JW_DWIN`, `JW_FRAM`, `JW_MatrixButtons`,
  `JW_ModbusRTU`, `JW_RS485`, `JW_RTC` y `JW_SD` del package están escritos para el Basic. Usan
  `JWPLC_RS485_RX_PIN`, `JWPLC_FRAM_CS`, la botonera y otros recursos del equipo. Por eso se quedan.
- **`JW_RTC` con ESP32 Board:** la copia del package usa el puente I2C del core del Basic (`jwplcI2C_*`). En un
  ESP32 genérico se compila la versión de la librería global (`JW-Control/JW-Libraries`, en la carpeta
  `libraries` del usuario), como hace Laundry. arduino-cli da prioridad a la carpeta del usuario (3) sobre la
  plataforma (2), así que también gana con la placa oculta `jwplc:esp32:esp32`.

## 4. Pruebas `[ejecutado]` (nube, arduino-cli 1.3.1)

Índice de prueba servido por HTTP local con los dos zips que generó `jwplc_release.py`. Instalación con
`core install` en un directorio de datos vacío:

| Prueba | Resultado |
|---|---|
| `core install jwesp:esp32` en limpio | instala las herramientas de `jwplc` y la plataforma `jwesp` |
| `core install jwplc:esp32` | sin descargar herramientas otra vez |
| `lib list --fqbn jwplc:esp32:jwplcbasic`, librerías con ejemplos | 17, todas `JW_`/`JWPLC_` |
| `lib list --fqbn jwesp:esp32:esp32`, librerías con ejemplos | 42 de arduino-esp32, ninguna JW |
| `board listall` | ESP32 Board (`jwesp`), JWPLC Basic |
| compilar `JWPLC_GlobalPeripherals/01.DigitalIO_Basic` y `JWPLC_Display/01.Display_IDLE_Status` (Basic) | OK |
| compilar un sketch con `#include <WiFi.h>` en el Basic | OK |
| compilar `WiFi/WiFiScan`, `WebServer/HelloServer` y `ESP32/ChipID` con `jwesp:esp32:esp32` | OK |
| compilar un sketch de solo core con `jwplc:esp32:esp32:PartitionScheme=no_fs` (placa oculta) | OK |
| compilar `JW_RTC/BasicRead` con `jwplc:esp32:esp32` | **falla al enlazar** (ver §5.5, ya pasaba en alpha.18) |

## 5. Pendientes

1. Prueba en Arduino IDE 2.3.10 (PC del propietario) con el índice de la PreRelease: menú Ejemplos con las dos
   placas, y actualizar desde alpha.18 con ESP32 Board elegida.
2. Lista `EXCEPCIONES` (ejemplos del core visibles en el Basic): vacía hasta que el propietario decida.
3. Quien use ESP32 Board en el IDE debe instalar el package «ESP32 Board» y volver a elegir la placa, porque el
   FQBN cambia. Va en `PRE_RELEASE.md` del cierre.
4. **Hecho (28/11, PR de `…/feature/nombres-packages`):** el propietario pidió nombres más cortos en el gestor de
   placas. `jwplc` pasa de «JW Control ESP32 Boards» a **«JWPLC»** y `jwesp` de «JW Control ESP32 genérico» a
   **«ESP32 Board»**. Las entradas viejas del índice conservan su nombre. `jwplc_release.py` ya no busca la
   plantilla por nombre, sino la entrada `esp32` más reciente del packager `jwplc`. La web (JW-Page) todavía dice
   «JW Control ESP32 Boards» en 9 lugares; se actualiza cuando salga el alpha.19.
5. **Aclarado con el propietario (28/11):** Laundry usa ESP32 Board y no usa el RTC, así que no le afecta. Hallazgo original: el `libJW_RTC.a` precompilado en alpha.18 (#248) llama a
   `jwplcI2C_begin`, `jwplcI2C_readReg8` y `jwplcI2C_readRegs`, que solo existen en el core del JWPLC Basic. Con
   `jwplc:esp32:esp32` (la placa que fija Laundry) un sketch con `JW_RTC` del package no enlaza. Si Laundry usa el
   `JW_RTC` del package y no el de su carpeta `libraries`, alpha.18 ya lo rompió. Revisar con el propietario.
