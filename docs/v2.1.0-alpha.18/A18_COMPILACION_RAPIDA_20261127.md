# A18 · Compilación rápida en Arduino IDE y OpenPLC (27/11)

Rama: `v2.1.0-alpha.18/JWPLC-Sim/feature/compilacion-rapida`. Pedido del propietario: acercar Arduino IDE y el
OpenPLC Editor a los ~4 s del JWPLC Sim.

```text
A18_COMPILACION_DETECCION_LIVIANA=PASS     # mismas librerías, prototipos y bytes en 99 sketches
A18_COMPILACION_PRECOMPILADAS_RS485_RTU_RTC=PASS   # ABI_COMPAT=PASS, mismos símbolos
```

## 1. Dónde se iba el tiempo

`[ejecutado]` en la nube (arduino-cli 1.5.1, 4 CPU, `05.HMI_Plantilla_Caldera`, alpha.17 `29632e4`):

| Fase | s |
|---|---|
| Arranque y prebuild | 0,4 |
| **Detecting libraries used** | **10,1** |
| Prototipos (ctags) | 0,7 |
| Sketch | 0,9 |
| Librerías desde fuente (GlobalPeripherals, RTC, RS485 ×2, ModbusRTU ×2) | 4,2 |
| Core (ya precompilado, `jwcontrol_precompiled_stub`) | 0,05 |
| Enlace + esptool + particiones | 2,6 |

La detección de librerías preprocesa el sketch completo una vez **por cada librería que encuentra** (17 pasadas en
este ejemplo) y en cada pasada relee `Arduino.h` con FreeRTOS, el SDK y la STL: de 360 a 1 150 encabezados. Al
editar el sketch se repite entera. En la PC del propietario esa fase toma 27,9 s.

El Sim es rápido por otra razón (caché por objeto compartida entre proyectos + semilla en el instalador). En
arduino-cli el build-path es por sketch, así que no hay caché de objetos sembrable desde el package: lo equivalente
son los `.a` precompilados, y el core ya lo está.

## 2. Cambios

1. **Detección liviana** (`df21893`). `platform.local.txt` ya define `JWPLC_LIBRARY_DISCOVERY_PHASE=1` sólo durante
   la detección (y ctags). En esa fase sólo importan `#include` y `#if`, así que:
   - `Arduino.h` carga `sdkconfig.h`, `esp_arduino_version.h`, `soc/soc_caps.h`, `pins_arduino.h` y el autoload de
     JWPLC (`JWPLC_Display_Auto.h` / `JWPLC_GlobalPeripherals_Auto.h`, que es lo que hace detectar las librerías en
     sketches sin `#include`).
   - Se vacían `Print.h`, `Stream.h`, `WString.h`, `Printable.h`, `HardwareSerial.h`, `IPAddress.h`, `Client.h`,
     `Server.h`, `Udp.h` y `esp32-hal.h` del core, `<memory>` de `FS.h`, FreeRTOS/`esp32-hal-spi.h` de `SPI.h` y
     `<atomic>` de `JWPLC_TFT.h`.
   - La compilación real no ve ningún cambio (la macro vale 0).
2. **Cinco librerías más precompiladas** (`c7b2344`): `JWPLC_RS485`, `JW_RS485`, `JWPLC_ModbusRTU`,
   `JW_ModbusRTU`, `JW_RTC` (`precompiled=full`, `src/esp32/lib*.a`). Ya estaba propuesto en
   `docs/v2.1.0-alpha.14/A14_BENCHMARK_COMPILACION_20261024.md`. Generadas con
   `tools/alpha16/regenerate_precompiled.sh build JWPLC_RS485 JW_RS485 JWPLC_ModbusRTU JW_ModbusRTU JW_RTC`:
   0 advertencias, `IMPORTS_FROM_OTHER_LIBS_MISSING=0`, `ABI_COMPAT=PASS`.

   | `.a` | Bytes | SHA-256 |
   |---|---|---|
   | `libJWPLC_RS485.a` | 119 086 | `7e213bacac9bb6cd7ce766b9ab2b088af101f54460f8d8d0c11f06b9d4ba7f5b` |
   | `libJW_RS485.a` | 121 180 | `19c3a8c4b5f47d3200ef5eae56a984e8453731f3293e099952de48f8f070af66` |
   | `libJWPLC_ModbusRTU.a` | 165 338 | `c7381e72cc76226615eab5424bbea2cdbac1d405088dd8f27386e58513810caf` |
   | `libJW_ModbusRTU.a` | 284 972 | `5ce386029cf1890179d0eb44bf9b108f8211b429ab4fb7367210b4b8726da380` |
   | `libJW_RTC.a` | 140 804 | `fa16bf298047a46617c50f9115b8202b95b6c08c4947fcec2deaf0fd811d052d` |

   `JWPLC_GlobalPeripherals` sigue en fuente: usa `JWPLC_HAS_FRAM`/`JWPLC_HAS_SD`, que cambian entre
   `jwplcbasic` y `jwplcbasiccore`, y arduino-cli sólo elige el `.a` por `build.mcu`.

   Riesgo conocido: `JWPLC_RS485.a` se compiló para las placas JWPLC (`JWPLC_HAS_RS485=1`); con la placa genérica
   `esp32` del package esa librería ya no se desactiva sola. Un `build_opt.h` del sketch tampoco cambia los `.a`.

**Regla de alpha (CLAUDE.md §3.4):** si en esta línea se toca el código de cualquiera de estas cinco librerías, se
pone en modo fuente y su `.a` se regenera al cierre.

## 3. Validación en la nube

`[ejecutado]` 108 sketches (88 ejemplos JW del package + 20 de librerías estándar: WiFi, WebServer, BLE, Ethernet,
Preferences, HTTPClient, Update, OTA, mDNS, LittleFS, SD_MMC, Wire, …), compilación limpia, `jwplc_local:esp32:jwplcbasic`:

| Comparación | Sketches | Resultado | Limpia promedio |
|---|---|---|---|
| alpha.17 vs cambio 1 | 99 | código de salida, librerías usadas, prototipos (`.ino.cpp`) y «Sketch uses» **idénticos** | 25,0 s → 12,3 s |
| alpha.17 vs cambios 1+2 | 87 | mismos librerías y símbolos (`nm -C`); ±4–16 B (Runtime −470 B) | 25,3 s → 8,8 s |

- Fallan igual antes y después: `BLE5_extended_scan` y `SD2USBMSC` (son para otros chips) y las 11 lecciones del
  curso (ya no están en alpha.18).
- La diferencia de bytes del cambio 2 es del enlazador Xtensa (relajación de `call8`/`callx8` y `movi.n`), la misma
  que dio el benchmark de alpha.17 (495 337 vs 495 473 B): ningún símbolo aparece ni desaparece.

`05.HMI_Plantilla_Caldera` en la nube:

| Caso | alpha.17 | Cambio 1 | Cambios 1+2 |
|---|---|---|---|
| Limpia / proyecto nuevo | 18,9 s | 9,2 s | 5,9 s |
| Editar el sketch y compilar | 15,0 s | 4,2 s | 4,2 s |
| Sin cambios | 3,0 s | 3,0 s | 3,0 s |

## 4. PC del propietario

`[ejecutado]` PC-MASTER-RACE (i5-13400F, Windows 10, arduino-cli 1.5.1), A = `46228c9`, B = `df21893`:

| Ejemplo | Caso | A | B |
|---|---|---|---|
| 05.HMI_Plantilla_Caldera | Limpia | ~50 s | 24,6 s |
| | Editar y compilar | 18,1 s | 12,1 s |
| | Sin cambios | 8,9 s | 8,8 s |
| 01.Display_IDLE_Status | Limpia | 49,2 s | 24,3 s |
| | Editar y compilar | 13,8 s | 10,8 s |
| 05 (verbose) | Detecting libraries | 27,9 s | 4,1 s |

Los 17 ejemplos de `JWPLC_Display`: limpia de 50–53 s a 24,5–25,3 s, «Sketch uses» idéntico al byte en los 17.

Con los dos cambios (C = `c7b2344`, A = `46228c9`), misma PC:

| Sketch | Caso | A | C |
|---|---|---|---|
| 05.HMI_Plantilla_Caldera | Limpia | 55,5 s | **17,5 s** |
| | Editar y compilar | 19,0 s | **12,1 s** |
| | Sin cambios | 9,1 s | 8,8 s |
| | Bytes | 495 337 | 495 337 |
| 01.Display_IDLE_Status | Limpia | 50,9 s | **17,0 s** |
| | Editar y compilar | 14,0 s | **11,1 s** |
| | Bytes | 484 881 | 484 889 |
| JWPLC_ModbusRTU 01.ModbusRTU_Slave_Holding | Limpia | 50,8 s | **17,0 s** |
| JW_RTC BasicRead | Limpia | 51,3 s | **16,9 s** |

Fases de la limpia del 05 (A → C): detección 28,8 → 4,2 s; prototipos 1,9 → 0,3 s; librerías 9,7 → 2,3 s (2,2 s son
`JWPLC_GlobalPeripherals`); enlace 3,4 → 3,3 s; postbuild ~2 s. Total 48,8 → 15,1 s. 46 compilaciones sin errores.
Los +8/+16 B son la relajación de `call8`/`callx8` de §3 (verificado desensamblando `JW_RTC::now()`).

`[ejecutado]` en placa (JWPLC Basic por USB, COM14, `c7b2344`, monitor a 115200 durante 15 s desde reset):

| Sketch | Bytes | Resultado |
|---|---|---|
| `JW_RTC/BasicRead` | 486 293 | «RTC found», `Valid: 1` cada segundo (11:22:23 → 11:22:36), temperatura 29,25–29,50 °C |
| `JWPLC_ModbusRTU/01.ModbusRTU_Slave_Holding` | 490 681 | esclavo ID=2 115200 8N1, HR0 cuenta segundos, sin reinicios (sin maestro: RX/TX=0) |
| `JWPLC_RS485/03.RS485_Status` | 485 729 | `Ready: yes`, `Status: OK`, AutoDirection, transmite cada 2 s, `Last error: OK` |

```text
A18_COMPILACION_PRECOMPILADAS_PLACA=PASS   # sin maestro Modbus: el intercambio RTU queda [no verificado]
```

## 5. OpenPLC Editor

El editor compila con este mismo package (`arduino-cli compile … --library <proyecto>/build/<placa>/src`), así que
los dos cambios le llegan solos. Además, `compileArduino` en
`src/backend/editor/compiler/editor-compiler-platform-port.ts` reescribe todos los archivos generados en cada
compilación: cambia su fecha y arduino-cli recompila toda la librería `src` y repite la detección. Escribir sólo
los archivos cuyo contenido cambió es un arreglo chico, pero va en el repo del editor (lo lleva Kenyi).

`[ejecutado]` en la PC del propietario, con el editor instalado (`open-plc-editor-jwplc`, arduino-cli 1.4.1 propio,
mismo `%LOCALAPPDATA%\Arduino15` que Arduino IDE) y una copia del build del proyecto
`A7_2_2_REMOTE_IO_8CH_LOOPBACK` (38 archivos en `src`, `libOpenPLCUserLib.a`), con la línea de arduino-cli que arma
el editor y la placa `jwplc_local:esp32:jwplcbasic`:

| Caso | A | C |
|---|---|---|
| Limpia | 59–65 s | **23,0 s** |
| Sin cambios | 14,8 s | **11,9 s** |
| Reescribir `src` (lo que hace el editor en cada compilación) | 17,7 s | **14,3 s** |
| Bytes | 512 041 | 512 049 |

Fases de la limpia: detección 38,8 → 6,4 s; compilación de sketch, `src` y librerías 15,6 → 8,0 s; enlace 5,7 → 5,5 s.
El precompilado de usuario del editor usa `build.library_discovery_phase=0` (visto en `--show-properties`), así que
no entra en la detección liviana. El editor instalado compila con el package del gestor de placas
(`jwplc:esp32` 2.1.0-alpha.17): la mejora le llega cuando se publique alpha.18.

## 6. Lo que queda

- En OpenPLC el piso es el enlace (5,5 s, se repite aunque no cambie nada) más la reescritura de `src`.
- El piso «sin cambios» en Windows (~8,8 s; 3 s en la nube) es enlace, esptool ×3, `gen_esp32part.exe` y arranque de
  procesos. Windows Defender está activo; medir con exclusión de `%LOCALAPPDATA%\Arduino15` y de la carpeta de
  compilación es decisión del propietario.
- `JWPLC_GlobalPeripherals` precompilada por placa (vía `build.extra_libs`, como el core) quitaría ~2,2 s más en
  limpia (≈13 s).
- Intercambio Modbus RTU con un maestro real `[no verificado]` (ver §4).
