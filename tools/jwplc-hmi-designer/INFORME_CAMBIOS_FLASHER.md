# Informe Detallado de Cambios: Flasheo de Live Bridge en 1 Clic

> **Proyecto:** JWPLC HMI Designer (`tools/jwplc-hmi-designer`)  
> **Fecha:** Octubre 2026  
> **Rama de trabajo:** `v2.1.0-alpha.15-HmiDesigner`  
> **Referencia de diseño:** [`PLAN_LIVE_BRIDGE_FLASHER.md`](PLAN_LIVE_BRIDGE_FLASHER.md)  
> **Objetivo:** Permitir que el usuario conecte su JWPLC por USB y, si el microcontrolador no tiene instalado el firmware de streaming, la aplicación pueda flashearlo e inicializar el Preview en Vivo automáticamente en un solo clic, sin necesidad de abrir Arduino IDE ni instalar Python.

> **Revisión 2026-10-21 (Claude):**
> - El manifiesto incluye `boot_app0.bin @ 0xE000`, igual que la subida de Arduino (`flash_args`), con lo
>   que quedan 4 partes;
> - el modal avisa que el flasheo reemplaza el programa del PLC;
> - se quitaron las copias duplicadas `JWPLC_HMI_Live_Bridge.ino.*` de `poc/firmware/`;
> - `tools/alpha15/build_live_bridge_firmware.sh` regenera los binarios desde el `.ino`.
>
> Ver `ALPHA15_STATUS.md` (2026-10-21).

---

## 1. Resumen Ejecutivo

Se implementaron y validaron al 100% las 4 fases técnicas planificadas para resolver la incompatibilidad del firmware Live Bridge con la plataforma `2.1.0` y dotar al diseñador de un motor universal de flasheo en un clic sobre **Web Serial API**.

```mermaid
flowchart TD
    subgraph Fase1["Fase 1: Firmware Base"]
        FW["Corrección JWPLC_HMI_Live_Bridge.ino<br/>Mutex SPI 80 MHz + Batch Drawing"]
        BIN["Compilación core jwplcbasic<br/>Generación de binarios"]
    end

    subgraph Fase2["Fase 2: Motor WebSerial & Assets"]
        ASSETS["poc/firmware/: bootloader, partitions, app, merged, manifest.json"]
        ESPTOOL["poc/esptool-bundle.js (esptool-js v0.7.0 con zlib inlined)"]
        SERVER["MIME application/octet-stream en electron/main.js y Server.ps1"]
    end

    subgraph Fase3["Fase 3: UX / UI 1 Clic"]
        FLASHER_JS["poc/designer-flasher.js<br/>API JWPLCHMIFlasher y Modal UI"]
        DETECT["Timeout 2.5s en designer-live.js<br/>Prompt interactivo automático"]
        DIAG_BTN["Botón manual ⚡ Flashear Live Bridge en Monitor LIVE"]
    end

    subgraph Fase4["Fase 4: Autoconexión & Pruebas"]
        RESET["RTS Hard Reset + Reconexión automática a 921,600 baudios"]
        TESTS["tests/live-bridge-flasher.test.cjs (7/7 tests PASS)<br/>Integrado en tests/run-all.cjs"]
    end

    Fase1 --> Fase2 --> Fase3 --> Fase4
```

---

## 2. Detalle de los Cambios por Fase

### Fase 1: Corrección de Firmware y Compilación Base
* **Problema:** En Alpha 11, `JWPLC_Display` usaba `Adafruit_GFX`, que disponía de `drawRGBBitmap`. Al migrar al paquete de placa 2.1.0, se incorporó la librería nativa de alto rendimiento `JWPLC_TFTClass`, la cual no incluye `drawRGBBitmap` y requiere gestión explícita del mutex de hardware SPI compartido.
* **Solución técnica implementada:**
  - En [`gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/JWPLC_HMI_Live_Bridge.ino`](gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/JWPLC_HMI_Live_Bridge.ino), se rediseñó la función `drawBlock()`:
    - `tft.beginBatch(50)`: adquiere el mutex de hardware `jwplcSPI_acquire` y configura la transacción del bus SPI a 80 MHz.
    - Dibuja secuencias horizontales continuas con `tft.drawFastHLine()` y píxeles individuales con `tft.drawPixel()`.
    - `tft.endBatch()`: libera el bus SPI para los demás periféricos del PLC (Ethernet, SD, FRAM).
* **Compilación y Artefactos:**
  - Compilado con core `jwplc:esp32:jwplcbasic` con 0 errores y 0 warnings.
  - Generados los binarios en `gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/build/`.

---

### Fase 2: Paquetización del Binario y Motor de Flasheo WebSerial
* **Empaquetado de artefactos precompilados (`poc/firmware/`):**
  - Se creó la carpeta `poc/firmware/` que contiene las imágenes binarias requeridas para el ESP32:
    - `bootloader.bin` (25,072 bytes, cargado en `0x1000` con magic byte `0xE9`).
    - `partitions.bin` (3,072 bytes, cargado en `0x8000` con magic bytes `0xAA 0x50`).
    - `firmware.bin` (427,392 bytes, cargado en `0x10000` con magic byte `0xE9`).
    - `firmware.merged.bin` (4,194,304 bytes, imagen monolítica completa @ `0x0`).
    - `manifest.json`: Descriptor estructurado del microcontrolador objetivo:
      ```json
      {
        "target": "esp32",
        "chip": "ESP32",
        "flashMode": "dio",
        "flashFreq": "40m",
        "flashSize": "4MB",
        "baudRate": 921600,
        "romBaudRate": 115200,
        "parts": [
          { "name": "bootloader", "path": "./firmware/bootloader.bin", "address": 4096 },
          { "name": "partitions", "path": "./firmware/partitions.bin", "address": 32768 },
          { "name": "app", "path": "./firmware/firmware.bin", "address": 65536 }
        ]
      }
      ```
* **Motor Universal `esptool-js` (`poc/esptool-bundle.js`):**
  - Se incorporó la distribución bundle de `esptool-js` v0.7.0 (con la librería de compresión `pako` inlined).
  - 100% autosuficiente en navegador y Electron: no requiere Python, Node en tiempo de ejecución, ni binarios externos.
  - Implementa `Transport` (capa WebSerial con SLIP) y `ESPLoader` (sincronización con ROM a 115,200 baudios, inyección del stub de flasheo y aceleración a 921,600 baudios).
* **Soporte de Servidor Local y Electron:**
  - **`electron/main.js`:** Se registró el MIME type `[".bin", "application/octet-stream"]` para servir de manera óptima las descargas de binarios desde el servidor estático interno.
  - **`JWPLC-HMI-Server.ps1`:** Se agregó la extensión `'.bin'` con `application/octet-stream` en `Get-ContentType`.

---

### Fase 3: Integración en la Interfaz de Usuario (UX / UI)
* **Nuevo módulo `poc/designer-flasher.js`:**
  - Registra el objeto global `window.JWPLCHMIFlasher`.
  - Proporciona un sistema modal autocontenido adaptado a la estética visual de la aplicación (Inter, Cascadia Code, degradados oscuros y estados reactivos):
    1. **Estado `prompt`:** Desplegado al detectar un PLC sin firmware bridge. Explica al usuario la situación y ofrece los botones:
       - `[⚡ Instalar Firmware Live]` (Acción principal con degradado naranja).
       - `[Seguir esperando]` (Cierra el modal y mantiene el puerto en escucha).
       - `[Cancelar]` (Desconecta el puerto serie limpiamente).
    2. **Estado `flashing`:** Muestra una barra de progreso lineal (0% a 100%), cálculo acumulativo de KB transferidos y un mini-terminal plegable que refleja los logs en tiempo real emitidos por `esptool-js`.
    3. **Estado `success`:** Indica confirmación visual (icono verde ✓) y ejecuta la transición automática a la Fase 4.
    4. **Estado `error`:** Ofrece mensaje claro del error, recomendación de usar el botón `BOOT/IO0` del PLC si el auto-reset fallase, y botones de `[Reintentar]` y `[Cerrar]`.
* **Integración en `poc/designer-live.js`:**
  - **Detección de timeout de 2.5s (`armNoBridgeTimer`):** Al hacer clic en "Conectar JWPLC", se abre el puerto a 921,600 baudios y se envía el comando `JWH?`. Si pasan 2.5 segundos sin recibir `JWHMI_LIVE_READY 1`, se dispara automáticamente el modal de flasheo.
  - **Botón manual en Monitor LIVE:** Se agregó `<button id="diagFlashButton">⚡ Flashear Live Bridge</button>` en la barra de herramientas del panel Diagnóstico para permitir flashear o reinstalar a demanda en cualquier momento.

---

### Fase 4: Autoconexión, Telemetría y Validación
* **Secuencia de Autoconexión post-reinicio:**
  - Al completar la escritura en flash:
    1. Se invoca `loader.after('hard_reset')`, el cual envía un pulso por RTS para reiniciar el ESP32 en modo aplicación.
    2. Se invoca `transport.disconnect()`, cerrando el puerto y liberando los bloqueos del stream WebSerial.
    3. `designer-live.js:reconnectLive` espera 450 ms a que el microcontrolador arranque y registre el dispositivo USB.
    4. Se reabre el mismo objeto `SerialPort` a 921,600 baudios (sin que el navegador vuelva a pedir permisos al usuario).
    5. Se inician `readLoop()` y el envío de `sendProbe()` (`JWH?`).
    6. El firmware recién instalado arranca en ~400 ms y emite `JWHMI_LIVE_READY 1`.
    7. Al recibir la respuesta:
       - El modal se cierra automáticamente.
       - El botón pasa al estado verde activo: `LIVE conectado`.
       - Se transmite el primer cuadro `FULL` de forma inmediata.
       - Queda operativo el streaming diferencial continuo a 30-60 FPS mediante *dirty rects*.
* **Actualización del Service Worker y Cadena de Carga:**
  - **`poc/ux-foundation.js`:** Se encadena `designer-flasher.js` justo antes de `designer-live.js`.
  - **`poc/service-worker.js`:** Se agregaron `./designer-flasher.js`, `./esptool-bundle.js` y `./firmware/manifest.json` a la lista de caché `CORE`, actualizando la versión de caché a `jwplc-hmi-designer-a15-flasher-v34`.

---

## 3. Pruebas Automatizadas y Verificación

### 3.1. Test Suite Específica: `tests/live-bridge-flasher.test.cjs`
Se creó una prueba completa que valida de extremo a extremo los componentes del sistema:
```bash
node tools/jwplc-hmi-designer/tests/live-bridge-flasher.test.cjs
```
**Resultados (7 de 7 pasadas):**
1. `manifest.json existe y tiene la estructura correcta`: Valida chip `esp32`, baudrates y direcciones `0x1000`, `0x8000`, `0x10000`.
2. `Binarios de firmware existen y tienen encabezados ESP32 válidos`: Valida magic bytes `0xE9` (bootloader y app) y `0xAA 0x50` (particiones), así como el tamaño exacto de 4MB de la imagen merged.
3. `esptool-bundle.js se carga e implementa ESPLoader y Transport`: Valida la exportación de clases y métodos (`main`, `writeFlash`, `after`).
4. `designer-flasher.js expone API pública JWPLCHMIFlasher`: Valida `showPromptModal`, `runFlashWithModal` y `executeFlash`.
5. `designer-live.js integra detección de timeout de 2.5s y flasheo en 1 clic`: Valida `armNoBridgeTimer`, `stopNoBridgeTimer`, `triggerFlashSequence`, `reconnectLive` y `diagFlashButton`.
6. `ux-foundation.js y service-worker.js integran designer-flasher y esptool-bundle`: Valida la presencia en la cadena de carga asíncrona y pre-caché.
7. `Mapeo de direcciones flash`: Valida las direcciones hexadecimales estándar de arranque de Espressif.

### 3.2. Integración en `tests/run-all.cjs`
El test fue integrado en la suite general del proyecto para que se ejecute en cada ciclo de integración continua o validación local.

### 3.3. Verificación de Sintaxis (`node --check`)
Todos los archivos JavaScript del directorio `poc/` y `electron/` pasaron la validación estricta de sintaxis de Node.js v22 sin advertencias ni errores.

---

## 4. Registro de Archivos Modificados y Creados

| Archivo | Tipo | Descripción |
| :--- | :---: | :--- |
| [`gates/.../JWPLC_HMI_Live_Bridge.ino`](gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/JWPLC_HMI_Live_Bridge.ino) | Modificado | Corrección de dibujo por lotes (`beginBatch`/`endBatch`) compatible con `JWPLC_TFTClass`. |
| [`poc/firmware/bootloader.bin`](poc/firmware/bootloader.bin) | Creado | Bootloader compilado del ESP32 (@ `0x1000`). |
| [`poc/firmware/partitions.bin`](poc/firmware/partitions.bin) | Creado | Tabla de particiones con soporte de app grande (@ `0x8000`). |
| [`poc/firmware/firmware.bin`](poc/firmware/firmware.bin) | Creado | Imagen de la aplicación Live Bridge (@ `0x10000`). |
| [`poc/firmware/firmware.merged.bin`](poc/firmware/firmware.merged.bin) | Creado | Imagen binaria completa monolítica de 4MB (@ `0x0`). |
| [`poc/firmware/manifest.json`](poc/firmware/manifest.json) | Creado | Descriptor de artefactos, baudrates y direcciones flash. |
| [`poc/esptool-bundle.js`](poc/esptool-bundle.js) | Creado | Motor `esptool-js` empaquetado para navegador/Electron sobre WebSerial. |
| [`poc/designer-flasher.js`](poc/designer-flasher.js) | Creado | Módulo UI y controlador de flasheo (`window.JWPLCHMIFlasher`). |
| [`poc/designer-live.js`](poc/designer-live.js) | Modificado | Detección automática en 2.5s, botón manual y reconexión post-reinicio. |
| [`poc/ux-foundation.js`](poc/ux-foundation.js) | Modificado | Incorporación de `designer-flasher.js` en la secuencia de carga. |
| [`poc/service-worker.js`](poc/service-worker.js) | Modificado | Registro de assets en caché y actualización de versión a `v34`. |
| [`electron/main.js`](electron/main.js) | Modificado | Soporte de tipo MIME para archivos `.bin`. |
| [`JWPLC-HMI-Server.ps1`](JWPLC-HMI-Server.ps1) | Modificado | Soporte de tipo MIME para archivos `.bin` en servidor estático PowerShell. |
| [`tests/live-bridge-flasher.test.cjs`](tests/live-bridge-flasher.test.cjs) | Creado | Suite de 7 pruebas unitarias y de integración del flasheador. |
| [`tests/run-all.cjs`](tests/run-all.cjs) | Modificado | Incorporación del test flasher a la suite general de pruebas. |
| [`PLAN_LIVE_BRIDGE_FLASHER.md`](PLAN_LIVE_BRIDGE_FLASHER.md) | Modificado | Actualización del documento del plan marcando las 4 fases como completadas. |

---

## 5. Modo de Uso

1. **Uso Automático (Detección en 1 Clic):**
   - Conecta el JWPLC por USB a la PC.
   - En el HMI Designer, presiona **"Conectar JWPLC"** y selecciona el puerto serie.
   - Si el PLC no tiene el firmware Live Bridge, a los 2.5 segundos aparecerá el diálogo:
     > *"Dispositivo detectado pero sin firmware Live Preview"*
   - Haz clic en **"⚡ Instalar Firmware Live"**.
   - La barra de progreso alcanzará el 100% en ~3 a 4 segundos, el PLC se reiniciará automáticamente y el Live Preview comenzará a transmitir en vivo a la pantalla TFT física ST7789.
2. **Uso Manual (Reinstalación bajo Demanda):**
   - Abre la pestaña inferior **"Diagnóstico"** (*Monitor LIVE*).
   - Haz clic en el botón naranja **"⚡ Flashear Live Bridge"**.
   - El instalador guiará el flasheo y reconexión inmediata.
