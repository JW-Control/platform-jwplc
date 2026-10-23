# Mapa Maestro y Referencia de Código: `tools/jwplc-hmi-designer/`

Este documento es el **Índice Maestro de Navegación y Arquitectura** del proyecto **JWPLC HMI Designer**. Está diseñado para servir como el punto de referencia principal tanto para desarrolladores como para **modelos de lenguaje con ventanas de contexto reducidas** (chats satélite), permitiendo localizar de inmediato qué archivo y qué función se debe consultar o modificar sin necesidad de cargar miles de líneas de código en memoria.

---

## 1. Mapa General del Proyecto

```text
tools/jwplc-hmi-designer/
├── INDEX.md                                      <- Este documento maestro y guía de orquestación
├── README.md                                     <- Estado de cierre Alpha11, contratos C++ y reglas de runtime
├── Build-*.ps1 / Install-*.ps1                   <- Scripts de compilación, empaquetado e instalación
├── JWPLC-HMI-Server.ps1                          <- Servidor HTTP local embebido
├── Start-JWPLC-HMI-Designer.ps1                  <- Launcher de arranque en navegador/app
│
├── poc/                                          <- APLICACIÓN WEB / FRONTEND CORE
│   ├── INDEX.md                                  <- Índice detallado de los 31 archivos y +300 funciones
│   ├── app.js                                    <- Motor central: canvas 320x170, fields, historial, herramientas
│   ├── designer-bar.js                           <- Inspector y renderer de barras y medidores circulares
│   ├── designer-bool.js                          <- Indicadores booleanos y switches
│   ├── designer-codegen.js                       <- Generador de C++ (JWPLC_HMI_Generated.h)
│   ├── designer-fonts.js / -fonts-data.js        <- Motor y datos de fuentes GFX y TFT_eSPI
│   ├── designer-live.js                          <- Live Preview por WebSerial @ 921600 baud
│   ├── designer-native-project.js                <- Puente nativo Electron para vincular sketches .ino
│   ├── designer-pages.js                         <- Gestor de múltiples páginas HMI
│   ├── designer-pixelmap*.js                     <- Editor de pixel art, optimizador RLE y workbench
│   ├── designer-project*.js                      <- Guardado/apertura .jwhmi y File System API
│   ├── desktop-responsive.js                     <- Adaptabilidad a ventanas de escritorio y paneles
│   └── styles.css / desktop-responsive.css       <- Estilos y temas visuales
│
├── cli/                                          <- CLI jwhmi: proyectos .jwhmi sin la UI (ver cli/INDEX.md)
├── agent/                                        <- Guía para agentes y referencia del formato .jwhmi
├── schema/                                       <- jwhmi.schema.json (generado desde cli/lib/spec.cjs)
├── examples/                                     <- Proyectos de ejemplo en forma corta
├── pruebas-ia/                                   <- Pruebas de HMI completas generadas por IA con la CLI
│
├── electron/                                     <- ENTORNO DE ESCRITORIO
│   ├── INDEX.md                                  <- Índice del proceso principal, preload e IPC
│   ├── main.js                                   <- Proceso Electron: ventana, servidor estático, WebSerial
│   ├── preload.cjs                               <- ContextBridge seguro (window.JWPLCHMINative)
│   └── package.json / installer.nsh              <- Configuración de compilación NSIS
│
├── arduino-ide-launcher/                         <- EXTENSIÓN ARDUINO IDE 2.x
│   ├── INDEX.md                                  <- Índice de autocompletado y launcher
│   ├── extension.js                              <- Autocompletado C++ de JWPLC_Display y comando 'JW HMI'
│   └── package.json                              <- Manifiesto VSIX
│
├── gates/                                        <- PRUEBAS Y GATES FÍSICOS ALPHA11
│   ├── INDEX.md                                  <- Índice de todos los gates y sketches .ino
│   ├── A11_LivePreview_WebSerial/                <- Firmware ESP32 para Live Preview WebSerial
│   ├── A11_Pages_Navigation/                     <- Gate físico de navegación por páginas
│   └── A11_2* / A11_3* / A11_7*                  <- Paridad GFX, métricas de fuentes, persistencia
│
├── JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/ <- ESPECIFICACIÓN DE REDISEÑO UI
│   ├── INDEX.md                                  <- Resumen del paquete UI
│   ├── JWPLC_HMI_Designer_BAR_Propuesta_2_Referencia.png
│   └── JWPLC_HMI_Designer_Inspector_BAR_Propuesta_2_Especificacion_UI.md
│
├── scripts/                                      <- SCRIPTS DE MANTENIMIENTO
│   ├── INDEX.md                                  <- Documentación de utilidades
│   └── generate-gfx-fonts.js                     <- Descarga y empaqueta fuentes de TFT_eSPI
│
├── assets/                                       <- ICONOS E IMÁGENES DE SISTEMA
│   ├── INDEX.md
│   └── JWPLC-HMI-Designer.ico
│
└── dist/                                         <- ARTEFACTOS GENERADOS (.vsix, .exe)
    └── INDEX.md
```

---

## 2. Detalle Exhaustivo de Archivos en la Raíz

| Archivo | Tipo / Líneas | Funciones / Puntos Clave | Descripción / Propósito |
| :--- | :---: | :--- | :--- |
| [`Build-ArduinoIDE-Launcher.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Build-ArduinoIDE-Launcher.ps1) | PowerShell<br>(83 líneas) | `param([string]$OutputDirectory)` | Compila la extensión para Arduino IDE 2.x generando el paquete `dist/jwplc-hmi-launcher-*.vsix`. |
| [`Build-JWPLC-HMI-Designer-Electron.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Build-JWPLC-HMI-Designer-Electron.ps1) | PowerShell<br>(84 líneas) | `param([switch]$SkipDependencies)` | Prepara dependencias de Electron, copia iconos y genera la versión portable unpacked en `electron/release/win-unpacked/`. |
| [`Build-JWPLC-HMI-Designer-Exe.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Build-JWPLC-HMI-Designer-Exe.ps1) | PowerShell<br>(67 líneas) | `param([string]$OutputPath, [string]$IconPath)` | Compila `JWPLC-HMI-Designer-Launcher.cs` con el compilador Roslyn de .NET Framework (`csc.exe`) para producir el ejecutable nativo liviano `JWPLC-HMI-Designer.exe`. |
| [`Build-JWPLC-HMI-Designer-Setup.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Build-JWPLC-HMI-Designer-Setup.ps1) | PowerShell<br>(110 líneas) | `param([switch]$SkipDependencies)` | Invoca `electron-builder` para generar el instalador final de Windows basado en NSIS (`JWPLC-HMI-Designer-Setup-*.exe`). |
| [`Install-ArduinoIDE-Launcher.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Install-ArduinoIDE-Launcher.ps1) | PowerShell<br>(99 líneas) | `param([string]$DesignerHome, [switch]$SkipBuild)` | Instala la extensión VSIX en el directorio de plugins de Arduino IDE 2 (`%USERPROFILE%\.arduinoIDE\plugins`), habilitando el autocompletado independientemente de si la app de escritorio está instalada. |
| [`Install-JWPLC-HMI-Designer.cmd`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Install-JWPLC-HMI-Designer.cmd) | Batch<br>(29 líneas) | Ejecutor directo de Windows | Lanza `Install-JWPLC-HMI-Designer.ps1` con permisos de ejecución omitidos (`-ExecutionPolicy Bypass`). |
| [`Install-JWPLC-HMI-Designer.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Install-JWPLC-HMI-Designer.ps1) | PowerShell<br>(268 líneas) | - `[L57] Get-InstalledDesignerProcesses`<br>- `[L74] Test-InstallExeUnlocked`<br>- `[L93] Stop-InstalledDesigner`<br>- `[L184] New-JwplcShortcut` | Instalador local completo: detiene instancias previas, compila el launcher `.exe`, copia los archivos a `%LOCALAPPDATA%\JWPLC\HMI Designer`, crea accesos directos en Escritorio y Menú Inicio, e instala el plugin de Arduino IDE. |
| [`JWPLC-HMI-Designer-Launcher.cs`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC-HMI-Designer-Launcher.cs) | C# (.NET)<br>(53 líneas) | - `[L8] Main()`<br>- `[L39] Log(string root, string message)` | Programa lanzador ejecutable mínimo sin consola que invoca silenciosamente `Start-JWPLC-HMI-Designer.ps1` y registra errores en `launcher-error.log` si falla. |
| [`JWPLC-HMI-Designer.cmd`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC-HMI-Designer.cmd) | Batch<br>(3 líneas) | Ejecución en segundo plano | Acceso directo por comando para invocar `Start-JWPLC-HMI-Designer.ps1` con ventana oculta. |
| [`JWPLC-HMI-Server.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC-HMI-Server.ps1) | PowerShell<br>(143 líneas) | - `[L12] Get-ContentType([string]$Path)`<br>- `[L26] Write-HttpResponse`<br>- Health check `/__health` | Servidor HTTP local multi-hilo basado en `HttpListener` en el puerto 8765. Resuelve tipos MIME, sirve estáticos de `poc/` y se apaga automáticamente tras 60 minutos de inactividad. |
| [`Start-JWPLC-HMI-Designer.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Start-JWPLC-HMI-Designer.ps1) | PowerShell<br>(72 líneas) | - `[L11] Test-JwplcHmiServer`<br>- `[L20] Find-ChromiumBrowser` | Verifica si el servidor HTTP ya está activo; si no, lo inicia. Detecta navegadores Chromium (Edge o Chrome) y abre la app en modo ventana independiente (`--app=http://127.0.0.1:8765/desktop.html`). |
| [`Test-JWPLC-HMI-Designer-Install.ps1`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/Test-JWPLC-HMI-Designer-Install.ps1) | PowerShell<br>(135 líneas) | - `[L15] Write-Gate([string]$Name, [bool]$Pass, [string]$Detail)` | Suite de pruebas de regresión e instalación: verifica la presencia de ejecutables portables, NSIS, VSIX en plugins, accesos directos y endpoint de salud `/__health`. |
| [`README.md`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/README.md) | Markdown<br>(384 líneas) | Documentación de cierre | Especificación técnica de cierre Alpha11, estructura del header generado `JWPLC_HMI_Generated.h`, límites V1 (16 páginas, 32 campos) y validaciones físicas. |

---

## 3. Guía de Flujo de Trabajo para Chats Satélite (Modelos de Menor Contexto)

Para trabajar de manera ultra-eficiente dividiendo tareas entre este **Chat Principal (Orquestador)** y **Chats Satélite (Modelos Ligeros)**:

### El Rol de este Chat Principal:
1. **Planificación y Arquitectura:** Diseñar la solución, identificar dependencias cruzadas (ej. entre UI, Electron IPC y generación C++).
2. **Delimitación de Tareas:** Indicarte exactamente qué archivo, qué líneas y qué función debe modificar el chat satélite.
3. **Revisión y Validación:** Verificar que el código generado por los chats satélite cumpla los contratos y gates del proyecto.

### Cómo Iniciar y Guiar un Chat Satélite:
Cuando abras una nueva ventana con un modelo de menor ventana de contexto, **NO le pegues archivos gigantes enteros**. En su lugar, usa esta plantilla:

> **Prompt de Arranque para Chat Satélite:**
> ```text
> Actúa como programador especializado en el proyecto JWPLC HMI Designer.
> Estamos trabajando en la carpeta: tools/jwplc-hmi-designer/
> 
> Consulta el archivo de contexto: [Ruta al INDEX.md relevante, ej: tools/jwplc-hmi-designer/poc/INDEX.md]
> 
> Tarea específica:
> - Archivo objetivo: poc/designer-bar.js
> - Función a modificar: drawBarField() (aprox. línea 1526)
> - Objetivo: [Explicación concisa del cambio puntual]
> 
> Reglas:
> 1. No alteres la estructura del proyecto ni cambies nombres de contratos C++.
> 2. Mantén compatibilidad con la pantalla ST7789 (320x170 px).
> 3. Devuelve únicamente el bloque de código modificado con contexto mínimo.
> ```

---

## 4. Enlaces Directos a los Índices de Cada Carpeta

- 📁 [**`poc/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/INDEX.md) — Índice maestro del frontend web (todos los scripts, funciones y líneas). Incluye el rediseño de la barra superior (cápsulas + iconos SVG) y el nuevo Inspector de figuras por pestañas con `cornerRadius` (RECT/TRIANGLE/POLYGON).
- 📁 [**`electron/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/electron/INDEX.md) — Proceso principal de escritorio, permisos de WebSerial e IPC nativo.
- 📁 [**`arduino-ide-launcher/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/arduino-ide-launcher/INDEX.md) — Extensión para Arduino IDE 2.x y autocompletado C++.
- 📁 [**`gates/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/INDEX.md) — Gates físicos, firmware de Live Preview y tests de paridad.
- 📁 [**`JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/INDEX.md) — Especificación UI del inspector BAR por pestañas.
- 📁 [**`scripts/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/scripts/INDEX.md) — Scripts generadores de fuentes desde TFT_eSPI.
- 📁 [**`assets/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/assets/INDEX.md) — Iconos y recursos visuales del sistema.
- 📁 [**`dist/INDEX.md`**](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/dist/INDEX.md) — Paquetes binarios generados (.vsix, .exe).
