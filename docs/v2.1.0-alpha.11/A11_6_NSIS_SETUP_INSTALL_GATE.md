# Alpha11 · A11-6 Gate de instalación NSIS del HMI Designer

Fecha: 2026-09-09
Branch: `v2.1.0-alpha.11/feature/hmi-designer`

## Objetivo

Validar el instalador Windows final en formato `.exe` para JWPLC HMI Designer Alpha11, incluyendo en una sola instalación la aplicación y la extensión/launcher para Arduino IDE 2.

El `.cmd` y los scripts PowerShell permanecen como herramientas de desarrollo y diagnóstico; el artefacto destinado a usuario final es el Setup NSIS `.exe`.

## Artefacto validado

```text
JWPLC-HMI-Designer-Setup-0.11.0-alpha.11-x64.exe
BYTES=79243913
SHA256=1d6c0d761364ee975ea6c4e9fe6650c6c7fa4c0902e0b6756e4266440bfa7528
VSIX=jwplc-hmi-launcher-0.1.4.vsix
```

El Setup fue generado con `electron-builder` target `nsis`, arquitectura x64, `oneClick=false` y `perMachine=false`.

## Instalación esperada

Aplicación:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer\JWPLC-HMI-Designer.exe
```

Extensión Arduino IDE 2:

```text
%USERPROFILE%\.arduinoIDE\plugins\jwplc-hmi-launcher-0.1.4.vsix
```

El instalador también crea:

- desinstalador Windows;
- acceso directo de Escritorio;
- acceso en Menú Inicio `JWPLC`;
- variable de usuario `JWPLC_HMI_DESIGNER_HOME`;
- protocolo `jwplc-hmi://`;
- una única versión del VSIX, eliminando versiones anteriores `jwplc-hmi-launcher-*.vsix`.

## Gate automático post-instalación

Resultado obtenido después de ejecutar el Setup real:

```text
HMI_APP_INSTALLED=PASS
HMI_APP_SOURCE_PARITY=PASS
HMI_UNINSTALLER_INSTALLED=PASS
HMI_ENV_HOME=PASS
HMI_PROTOCOL_REGISTERED=PASS
HMI_EXTENSION_INSTALLED=PASS
HMI_EXTENSION_SINGLE_VERSION=PASS
HMI_DESKTOP_SHORTCUT=PASS
HMI_STARTMENU_SHORTCUT=PASS

TOTAL_GATES=9
FAILED_GATES=0
ALPHA11_COMBINED_INSTALLER=PASS
```

Paridad del ejecutable instalado:

```text
SOURCE_SHA256=89185582d14912332b14845083f752e759c3194b70f263088eb339966e4ea6cb
INSTALLED_SHA256=89185582d14912332b14845083f752e759c3194b70f263088eb339966e4ea6cb
```

Fuente usada para la comparación:

```text
tools/jwplc-hmi-designer/electron/release/win-unpacked/JWPLC-HMI-Designer.exe
```

Desinstalador detectado:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer\Uninstall JWPLC-HMI-Designer.exe
```

## Interpretación

El Setup NSIS reproduce correctamente el flujo de instalación combinado previamente validado con el instalador técnico y migra sobre la instalación Alpha11 existente sin perder:

- aplicación nativa Electron;
- acceso por Escritorio y Menú Inicio;
- integración mediante `JWPLC_HMI_DESIGNER_HOME`;
- fallback por protocolo `jwplc-hmi://`;
- extensión Arduino IDE 2.

No se publica el `.cmd` como instalador de usuario final.

## Estado

```text
A11_6_NSIS_SETUP_BUILD=PASS
A11_6_NSIS_SETUP_INSTALL=PASS
A11_6_NSIS_APP_SOURCE_PARITY=PASS
A11_6_NSIS_UNINSTALLER_PRESENT=PASS
A11_6_COMBINED_INSTALLER=PASS
A11_6_ARDUINO_IDE_EXTENSION_FILES=PASS
A11_6_ARDUINO_IDE_EXTENSION_PHYSICAL_GATE=PENDING
A11_6_NSIS_UNINSTALL_GATE=PENDING
```

Pendiente inmediato: reiniciar Arduino IDE 2 y validar físicamente que aparezcan `JW HMI` y `JWPLC: Abrir HMI Designer`, que ambos abran la aplicación instalada y que Arduino IDE continúe compilando/subiendo normalmente. Después se debe validar el desinstalador NSIS y confirmar que retira aplicación, VSIX, protocolo y variable de entorno.