# Índice de Arquitectura y Referencia de Código: `arduino-ide-launcher/`

Este directorio contiene la extensión para **Arduino IDE 2.x** (basado en Eclipse Theia / VS Code). Aporta dos capacidades principales al entorno de desarrollo del usuario:
1. **Autocompletado inteligente de C++** para la biblioteca `JWPLC_Display` y los botones de hardware de la plataforma JWPLC.
2. **Botón en la barra de estado y comando** para lanzar la aplicación de escritorio **JWPLC HMI Designer** directamente desde Arduino IDE.

---

## 1. Estructura de Archivos

| Archivo | Tipo | Tamaño / Líneas | Propósito Principal |
| :--- | :---: | :---: | :--- |
| [`extension.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/arduino-ide-launcher/extension.js) | JavaScript (Node/VS Code API) | 306 líneas (~11.8 KB) | Lógica de la extensión: proveedor de autocompletado, detección de ejecutable del Designer y lanzamiento. |
| [`package.json`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/arduino-ide-launcher/package.json) | JSON | 45 líneas (~1.1 KB) | Manifiesto de extensión VS Code / Arduino IDE: comandos, puntos de activación, menús y metadatos. |
| [`README.md`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/arduino-ide-launcher/README.md) | Markdown | 66 líneas (~2.0 KB) | Guía de uso, instalación manual y arquitectura del VSIX. |
| [`media/jw-hmi.svg`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/arduino-ide-launcher/media/jw-hmi.svg) | SVG | 328 bytes | Icono del botón `JW HMI` en la barra de estado inferior de Arduino IDE. |

---

## 2. Detalle Exhaustivo de Funciones (`extension.js`)

| Línea Aprox. | Función | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L10` | `designerRoots` | Ninguno | Retorna las rutas donde puede estar instalado el Designer (prioriza `%LOCALAPPDATA%\JWPLC\HMI Designer`). |
| `L21` | `findDesignerExe` | Ninguno | Busca en el sistema de archivos la presencia de `JWPLC-HMI-Designer.exe`. |
| `L29` | `cleanElectronChildEnv` | Ninguno | Limpia variables de entorno de Theia/Electron (`ELECTRON_RUN_AS_NODE`, etc.) para evitar conflictos al invocar la app externa. |
| `L41` | `openByProtocol` | Ninguno | Intenta invocar el launcher mediante el protocolo URL de Windows `jwplc-hmi://`. |
| `L49` | `psQuote` | `value` | Escapa comillas y caracteres especiales para ejecución segura en PowerShell. |
| `L53` | `launchViaWindowsShell` | `exe` | Lanza el proceso del Designer en segundo plano usando PowerShell `Start-Process`. |
| `L101` | `launchDirect` | `exe` | Lanza directamente el ejecutable con `child_process.spawn` desacoplado (`detached: true`). |
| `L130` | `launchDesigner` | Ninguno | Orquestador de apertura: detecta si la app está instalada, prueba lanzamiento directo, shell o protocolo, y emite alertas si no la encuentra. |
| `L163` | `enumCompletion` | `label, detail, index` | Construye un elemento de autocompletado para constantes de color y enumeraciones. |
| `L171` | `methodCompletion` | `label, insertText, detail, index` | Construye un elemento de autocompletado para funciones de la API de C++. |
| `L179` | `recommendedDisplayMethods` | Ninguno | Catálogo de métodos recomendados de `JWPLC_Display` (ej. `showPage`, `setText`, `setBar`, `setBool`). |
| `L213` | `buttonCompletions` | `contextLabel` | Sugerencias de lectura no bloqueante de botones físicos (`pressed(BTN_UP)`, `released(BTN_OK)`). |
| `L224` | `displayConfigCompletions` | `document, position` | Analiza el texto previo al cursor en sketches `.ino` para decidir qué sugerencias de autocompletado ofrecer contextualmente. |
| `L266` | `activate` | `context` | Punto de entrada de la extensión: registra el comando `jwplc.openHmiDesigner`, el botón de barra de estado y el `CompletionItemProvider`. |
| `L300` | `deactivate` | Ninguno | Hook de desactivación y liberación de recursos. |
