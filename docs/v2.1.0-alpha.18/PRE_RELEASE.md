# v2.1.0-alpha.18 — Compilación unas 3 veces más rápida, ejemplos sólo del JWPLC Basic y OpenPLC con JWPLC Sim

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea `JWPLC-Sim`, rama `v2.1.0-alpha.18/JWPLC-Sim/main` en platform-jwplc y en openplc-editor. Se
publica sobre la 2.1.0-alpha.17.

## Descargas

| Qué | Dónde |
|---|---|
| **OpenPLC Editor – JWPLC Edition 4.3.3-jwplc.3** (Windows x64, con el HMI Designer adentro) | [`OpenPLC-Editor.exe`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.3/OpenPLC-Editor.exe) · [notas y SHA-256](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.3.3-jwplc.3) |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.17**, firmado (`jwcontrol-2026`), sin cambios en esta alpha | [`JWPLC-Basic.vpp`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.3/JWPLC-Basic.vpp) |
| **JWPLC HMI Designer 2.1.0-alpha.18** para Arduino IDE (mismo código que alpha.17) | adjuntos a esta Release: `HMI-Designer-Setup.exe`, `HMI-Designer-Portable.exe` y `HMI-Designer-SHA256.txt` |
| **Package Arduino** `jwplc:esp32@2.1.0-alpha.18` | índice dev (ver README) |

Los instaladores no tienen firma de código: si Windows SmartScreen avisa, **Más información → Ejecutar de todas
formas**.

## Qué trae

- **Compilación unas 3 veces más rápida** (#248) en Arduino IDE y en OpenPLC Editor, sin tocar el sketch.
  - **Búsqueda de librerías liviana:** durante la búsqueda de librerías de arduino-cli
    (`JWPLC_LIBRARY_DISCOVERY_PHASE=1`), `Arduino.h` y los headers pesados del core, `FS.h`, `SPI.h` y
    `JWPLC_TFT.h` solo exponen sus `#include`. La compilación real no cambia: los binarios son idénticos byte a byte.
  - **Cinco librerías más vienen precompiladas:** `JWPLC_RS485`, `JW_RS485`, `JWPLC_ModbusRTU`,
    `JW_ModbusRTU` y `JW_RTC`. Los binarios difieren en ±8/16 bytes, por la relajación de llamadas del enlazador
    Xtensa, con los mismos símbolos.
  - **Mediciones en la PC del propietario** (Windows con Defender activo):

    | Caso | Antes | Ahora |
    |---|---|---|
    | `05.HMI_Plantilla_Caldera`, compilación limpia | 55 s | 17,5 s |
    | `05.HMI_Plantilla_Caldera`, tras editar el sketch | 19 s | 12 s |
    | `02.Runtime_En_Vivo` (medido por el propietario) | 100 s | 30 s |
    | Proyecto OpenPLC `A7_2_2_REMOTE_IO`, compilación limpia | 59–65 s | 23 s |

  - **Prueba en placa:** RTC, esclavo Modbus RTU y estado del RS-485 en PASS (COM14).
  - Detalle completo en [A18_COMPILACION_RAPIDA_20261127.md](A18_COMPILACION_RAPIDA_20261127.md).
- **Ejemplos sólo del JWPLC Basic:** el menú Ejemplos ya no muestra los ejemplos de arduino-esp32 que no aplican
  al equipo. Las librerías siguen instaladas y compilan igual. Se quitan los ejemplos de:
  - **Otro chip:** USB, ESP_SR, OpenThread, Zigbee, ESP_HostedOTA, TFLiteMicro y Matter.
  - **Periféricos que en el equipo están soldados de otra forma:** SD_MMC, Ethernet del core, PPP, ESP_I2S,
    Adafruit_BusIO y Wire.
  - **Sin uso en el equipo:** FFat, Console, Insights y RainMaker.
  - **Ejemplos que se saltan el mutex del bus SPI:** SPI y SD. Para la microSD se usa `JW_SD`.
  - **De `ESP32`, los que piden pines libres.** Quedan ChipID, MacAddress, ResetReason, FreeRTOS, Time y Timer.
  - **Se mantienen:** WiFi, Bluetooth, servidores web, OTA y almacenamiento interno.
  - **Al actualizar arduino-esp32** se repite con `tools/alpha18/podar_ejemplos_esp32.sh`.
- **OpenPLC Editor 4.3.3-jwplc.3** (openplc-editor #19, #20 y #23):
  - Botón **Simular** que abre el proyecto en el JWPLC Sim, y `--open-project` para abrirlo desde el Sim.
  - Canales 2 y 3 del TREND y pisos independientes de la Baliza enlazables a variables PLC.
  - Guardar ya no reescribe un HMI sin cambios.
  - Modbus TCP (firmware OpenPLC) que arranca aunque el cable de red se conecte después del encendido.
- **Curso al JWPLC Sim** (#247): el curso del JWPLC Basic, sus retos y las arquitecturas viven ahora en la librería
  `JWPLC_Aprende` del repositorio jwplc-sim (decisión del propietario, 20/11). Último SHA del platform con el curso:
  `5b0d0bd`.

## Instalación

1. Package: en Arduino IDE agregar el índice dev e instalar **JW Control ESP32 Boards 2.1.0-alpha.18** (ver README).
2. OpenPLC Editor: ejecutar `OpenPLC-Editor.exe` (4.3.3-jwplc.3; actualiza una JWPLC Edition anterior).
3. El editor usa el package instalado desde el gestor de placas: con 2.1.0-alpha.18 instalado, sus compilaciones
   también son más rápidas.

## Verificación

Ver `ALPHA18_STATUS.md` §3.

## Limitaciones conocidas

- Windows Defender agrega tiempo a cada compilación. Excluir `%LOCALAPPDATA%\Arduino15` y la carpeta de
  compilación la reduce más, pero no se midió.
- OpenPLC Editor todavía reescribe todos los archivos generados en cada compilación y recompila la librería del
  proyecto. Que solo reescriba los que cambiaron es una mejora propuesta para el editor.
- Pendientes del editor:
  - Gates H1 y H2 incompletos.
  - Arreglo de Modbus TCP (H3) aún sin prueba en placa.
  - Ver `ALPHA18_STATUS.md` §3.
- Sólo Windows x64 para los instaladores.

## Versiones que forman esta Release

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` | 2.1.0-alpha.18 | `release/v2.1.x` al publicar |
| HMI Designer | 2.1.0-alpha.18 (código de alpha.17) | instaladores: árbol de la Release; dentro del editor: platform-jwplc `3f29398` |
| VPP JWPLC Basic | 2.1.0-alpha.17 | platform-jwplc `0647a84` |
| OpenPLC Editor – JWPLC Edition | 4.3.3-jwplc.3 | tag `jwplc-editor-v4.3.3-jwplc.3` (openplc-editor #23) |
| JWPLC Sim (gate `SIM_REGRESSION`) | — | jwplc-sim `main` en la corrida del PR de cierre |
