# Índice de Arquitectura y Referencia de Código: `electron/`

Este directorio contiene la envoltura y entorno de escritorio **Electron** para el **JWPLC HMI Designer**. Permite compilar y ejecutar la aplicación como un ejecutable nativo de Windows (`.exe`), con soporte de instalador NSIS, permisos automáticos de Web Serial para comunicarse con la placa física y acceso al sistema de archivos local para sincronizar sketches de Arduino.

---

## 1. Estructura de Archivos

| Archivo | Tipo | Tamaño / Líneas | Propósito Principal |
| :--- | :---: | :---: | :--- |
| [`main.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/main.js) | JavaScript (Node.js) | 315 líneas (~10.5 KB) | Proceso principal de Electron: ciclo de vida, servidor estático, ventana BrowserWindow, IPC y permisos WebSerial. |
| [`preload.cjs`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/preload.cjs) | CommonJS | 12 líneas (580 B) | Script de precarga seguro: expone la API nativa `window.JWPLCHMINative` al renderer mediante `contextBridge`. |
| [`bootstrap.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/bootstrap.js) | JavaScript (Node.js) | 31 líneas (933 B) | Utilidad para sincronizar la carpeta `poc/` si Electron se empaqueta o ejecuta con fuentes desacopladas. |
| [`installer.nsh`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/installer.nsh) | NSIS Script | 55 líneas (~2.1 KB) | Script de empaquetado para `electron-builder`: crea accesos directos, menú inicio y registro. |
| [`package.json`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/package.json) | JSON | 62 líneas (~1.9 KB) | Manifiesto de npm, dependencias (`electron`, `electron-builder`) y configuración de build de Windows. |
| [`package-lock.json`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/package-lock.json) | JSON | ~150 KB | Árbol de dependencias bloqueado de npm. |

---

## 2. Detalle Exhaustivo de Archivos y Funciones

### `main.js`
*Proceso principal de Electron (Main Process).*

| Línea Aprox. | Función / Manejador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L39` | `resolveWebPath` | `requestUrl` | Mapea URLs de peticiones HTTP locales a rutas relativas en el disco dentro de la carpeta `poc/`. |
| `L53` | `startStaticServer` | Ninguno | Inicia un servidor HTTP local en `127.0.0.1:8765` para servir la UI web con tipos MIME correctos. |
| `L98` | `installSerialPermissions` | Ninguno | Intercepta `session.defaultSession.setPermissionCheckHandler` y `setDevicePermissionHandler` para otorgar acceso automático al puerto WebSerial (ESP32/CH340/CP2102) sin diálogos bloqueantes. |
| `L134` | `linkedStateFile` | Ninguno | Retorna la ruta al archivo JSON persistente (`userData/linked-sketch.json`) que recuerda el último sketch abierto. |
| `L136` | `directoryContainsSketch` | `directoryPath` | Comprueba si un directorio contiene un sketch válido cuyo nombre coincida con la carpeta (`<Nombre>.ino`). |
| `L145` | `fileExists` | `filePath` | Comprobación asíncrona segura de existencia de archivo en disco. |
| `L154` | `sketchInfo` | `directoryPath` | Extrae metadatos del proyecto: ruta al `.ino`, presencia de `.jwhmi` y de `JWPLC_HMI_Generated.h`. |
| `L164` | `persistLinkedSketch` | Ninguno | Guarda en disco (`linked-sketch.json`) la ruta del sketch actualmente vinculado. |
| `L173` | `restoreLinkedSketchPath` | Ninguno | Lee al inicio la última ruta de sketch utilizada por el usuario. |
| `L184` | `installNativeIpc` | Ninguno | Registra los canales de comunicación IPC invocables desde el renderer. |
| `L188` | `ipcMain.handle('jwplc-hmi:select-sketch-directory')` | `event` | Abre el diálogo nativo de Windows (`dialog.showOpenDialog`) para seleccionar una carpeta de sketch de Arduino. |
| `L205` | `ipcMain.handle('jwplc-hmi:get-linked-sketch')` | `event` | Retorna al frontend la información del sketch vinculado actual. |
| `L210` | `ipcMain.handle('jwplc-hmi:write-generated-header')` | `event, payload` | Escribe de forma atómica en disco el archivo `JWPLC_HMI_Generated.h` con el C++ generado. |
| `L228` | `ipcMain.handle('jwplc-hmi:write-project')` | `event, payload` | Guarda de forma atómica en disco el archivo `.jwhmi` (JSON del proyecto de diseño). |
| `L248` | `createWindow` | Ninguno | Crea y configura la ventana principal `BrowserWindow` (1280x820, icono, menú personalizado, carga de `desktop.html`). |

---

### `preload.cjs`
*Bridge seguro entre el renderer y el proceso principal.*

Expone en el contexto del navegador el objeto `window.JWPLCHMINative`:

```javascript
window.JWPLCHMINative = {
  isElectron: true,
  selectSketchDirectory: () => ipcRenderer.invoke("jwplc-hmi:select-sketch-directory"),
  getLinkedSketch: () => ipcRenderer.invoke("jwplc-hmi:get-linked-sketch"),
  writeGeneratedHeader: (payload) => ipcRenderer.invoke("jwplc-hmi:write-generated-header", payload),
  writeProject: (payload) => ipcRenderer.invoke("jwplc-hmi:write-project", payload)
};
```

---

### `bootstrap.js`
*Sincronizador de frontend en builds externos.*

| Línea Aprox. | Función | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L8` | `directoryExists` | `target` | Valida si un directorio existe en el sistema de archivos. |
| `L17` | `syncExternalFrontend` | Ninguno | Copia o vincula los assets de `poc/` al directorio de empaquetado de Electron antes de compilar. |

---

### `installer.nsh`
*Script NSIS para Windows Installer.*

Define macros de NSIS para:
- `customInstall`: Creación de accesos directos en Escritorio y Menú Inicio apuntando a `JWPLC-HMI-Designer.exe`.
- `customUnInstall`: Limpieza de accesos directos y configuración de usuario.
