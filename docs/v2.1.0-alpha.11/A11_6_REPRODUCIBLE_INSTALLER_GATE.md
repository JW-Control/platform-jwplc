# Alpha11 · A11-6 Gate de build reproducible e instalador combinado

Fecha: 2026-09-09
Branch: `v2.1.0-alpha.11/feature/hmi-designer`

## Objetivo

Cerrar la reproducibilidad del empaquetado del JWPLC HMI Designer y consolidar el flujo de instalación de usuario en un único instalador que coloque:

- la aplicación JWPLC HMI Designer;
- la extensión/launcher para Arduino IDE 2;
- los accesos y variables de entorno asociados.

## Recursos versionados

Commit:

```text
f814f8c1ca1d17d7648be69a21d82f5337a9bd50
```

Archivos incorporados:

```text
tools/jwplc-hmi-designer/assets/JWPLC-HMI-Designer.ico
tools/jwplc-hmi-designer/electron/package-lock.json
```

Identidad validada:

```text
ICON_BYTES=169695
ICON_SHA256=9205dbb56f5f3ba60cedd4916aeea4aa3a113ca00bdf38d824aea8c56926c8cb
PACKAGE_LOCK_BYTES=150540
PACKAGE_LOCK_SHA256=352b33f31f3539ad53d4e8160ccfc5d9ff7aca450cbf687eca4f575ca6db7ff7
LOCKFILE_VERSION=3
LOCK_ROOT_VERSION=0.11.0-alpha.11
LOCK_ELECTRON=^31.4.0
LOCK_ELECTRON_BUILDER=^24.13.3
```

`package.json` y `package-lock.json` coinciden en versión y dependencias raíz.

## Política de build reproducible

`Build-JWPLC-HMI-Designer-Electron.ps1` requiere ahora:

```text
electron/package.json
electron/package-lock.json
assets/JWPLC-HMI-Designer.ico
```

La instalación de dependencias se realiza con:

```text
npm ci --no-audit --no-fund
```

en lugar de `npm install`.

Los siguientes directorios se consideran artefactos generados y permanecen fuera del control de versiones:

```text
tools/jwplc-hmi-designer/dist/
tools/jwplc-hmi-designer/electron/build/
tools/jwplc-hmi-designer/electron/node_modules/
tools/jwplc-hmi-designer/electron/release/
```

## Gate clean rebuild

Se eliminó previamente todo artefacto generado:

```text
NODE_MODULES_EXISTS=False
DIST_EXISTS=False
BUILD_EXISTS=False
RELEASE_EXISTS=False
```

Luego se ejecutó el build desde los archivos versionados.

Resultado de dependencias:

```text
npm ci
added 310 packages
```

Builder:

```text
electron-builder=24.13.3
electron=31.7.7
target=portable
arch=x64
```

Artefacto producido:

```text
JWPLC-HMI-Designer-Portable-0.11.0-alpha.11-x64.exe
PORTABLE_BYTES=71751845
PORTABLE_SHA256=6e5ffd96cd4b8492027a5bbbac6d54a0e041ed6ab11e2337d61d4beea86c806b
```

Copia usada por el instalador:

```text
tools/jwplc-hmi-designer/dist/JWPLC-HMI-Designer-Electron.exe
DIST_BYTES=71751845
DIST_SHA256=6e5ffd96cd4b8492027a5bbbac6d54a0e041ed6ab11e2337d61d4beea86c806b
```

Gates:

```text
PACKAGE_LOCK_UNCHANGED=True
PORTABLE_DIST_PARITY=True
GIT_DIRTY_COUNT=0
ALPHA11_HMI_REPRODUCIBLE_BUILD=PASS
```

## Instalador combinado Alpha11

El flujo normal de usuario queda unificado en:

```text
Install-JWPLC-HMI-Designer.cmd
```

La ejecución normal instala por defecto:

```text
Aplicación:
%LOCALAPPDATA%\JWPLC\HMI Designer\JWPLC-HMI-Designer.exe

Extensión Arduino IDE 2:
%USERPROFILE%\.arduinoIDE\plugins\jwplc-hmi-launcher-*.vsix
```

Además:

- crea accesos en Escritorio y Menú Inicio;
- define `JWPLC_HMI_DESIGNER_HOME`;
- registra `jwplc-hmi://`;
- mantiene `-NoArduinoIDEExtension` únicamente como opt-out técnico;
- conserva `-InstallArduinoIDELauncher` por compatibilidad con scripts antiguos.

La extensión no incrusta el Designer ni modifica Arduino IDE: abre la aplicación externa instalada.

## Advertencias npm observadas

Durante `npm ci` se mostraron advertencias de paquetes transitivos deprecated (`inflight`, `glob`, `boolean`, `tar`). No bloquearon el build y el árbol resuelto quedó fijado por el lockfile. Se registran como mantenimiento futuro; no se actualizan dependencias durante el cierre de Alpha11 para evitar introducir cambios de alcance.

## Estado

```text
A11_6_RESOURCES_VERSIONED=PASS
A11_6_PACKAGE_LOCK_MATCH=PASS
A11_6_CLEAN_ELECTRON_REBUILD=PASS
A11_6_PACKAGE_LOCK_UNCHANGED=PASS
A11_6_PORTABLE_DIST_PARITY=PASS
A11_6_BUILD_GIT_CLEAN=PASS
A11_6_COMBINED_INSTALLER_IMPLEMENTED=YES
A11_6_COMBINED_INSTALLER_PHYSICAL_GATE=PENDING
A11_6_ARDUINO_IDE_EXTENSION_PHYSICAL_GATE=PENDING
```

Pendiente inmediato: ejecutar el instalador combinado real, validar archivos/registro/extensión y reiniciar Arduino IDE 2 para confirmar comando y botón `JW HMI`.
