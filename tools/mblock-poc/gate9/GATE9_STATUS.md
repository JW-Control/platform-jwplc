# JWPLC Basic - mBlock - Gate 9 (instalador y arranque automático del bridge)

Fecha: 2026-10-26
Rama: `experiment/mblock-jwplc`

## Estado

~~~text
GATE9A_BRIDGE_ON_MBLOCK_ELECTRON=PASS  [ejecutado] bridge 0.6.0 con el Electron de mBlock (Node 16.16) compila
GATE9B_HIDDEN_LAUNCHER_SINGLE=PASS     [ejecutado] lanzador .vbs oculto; una segunda copia no pisa el token
GATE9C_INSTALLER_PACKAGE=PASS          [ejecutado] JWPLC-mBlock-0.12.0.zip armado; PowerShell 5.1 lo analiza sin errores
GATE9D_INSTALL_THIS_PC=PENDIENTE       instalar desde el Explorador (el propietario)
GATE9E_OTHER_MACHINE=PENDIENTE         .mext y bridge en otra máquina (el propietario)
~~~

## Objetivo

Probar el `.mext` en otra máquina sin Node ni pasos a mano: el bridge debe
instalarse con un doble clic, arrancar solo y, si no está corriendo, arrancar
cuando mBlock sube un programa.

## Decisiones

- **Sin Node en la otra máquina.** mBlock 5.4.3 es Electron 21.4.4; con
  `ELECTRON_RUN_AS_NODE=1` `mBlock.exe` ejecuta scripts como Node 16.16 (mBlock
  ya lo hace con sus servicios `resources\ml\…`). El bridge no usa nada de
  Node 18 (`fetch` es del lado de mBlock), así que corre así. Si no hay mBlock,
  el instalador usa `node.exe` del PATH. En el Administrador de tareas el bridge
  aparece como un proceso `mBlock.exe` más.
- **Sin ventana.** `start-bridge.vbs` (lo genera el instalador con las rutas de
  la máquina) pone `ELECTRON_RUN_AS_NODE` y lanza `mBlock.exe` oculto
  (`WScript.Shell.Run …, 0`). VBScript sigue en Windows 10/11; Microsoft anunció
  que lo retirará más adelante (revisar entonces).
- **Arranque.** Al iniciar sesión (acceso directo en la carpeta Inicio) y a
  demanda: el instalador registra `jwplc-bridge://` en HKCU, y el driver de la
  extensión, si `/health` no responde, abre `jwplc-bridge://start` con
  `MeApi.fe.openLink` y espera hasta 20 s.
- **Una sola copia.** Bridge 0.6.0 escribe el token al abrir el puerto (antes lo
  escribía al arrancar: una segunda copia lo pisaba y se cerraba por
  `EADDRINUSE`, dejando a mBlock con un token inválido). Con el puerto ocupado
  sale con `ALREADY_RUNNING`.
- **Archivos fuera de AppData.** Bridge en `%USERPROFILE%\.jwplc\mblock-bridge\app`,
  junto a sus datos (token, sketch, build, `bridge.log`).
- **Package publicado.** Si no hay `bridge.json`, el instalador escribe
  `fqbn=jwplc:esp32:jwplcbasic` y un sketchbook vacío propio (como `--reset`,
  Gate 8A): las librerías de `Documentos\Arduino` no tapan las del package.
- **Requisitos (solo los revisa).** Arduino CLI o Arduino IDE 2 (también
  instalado solo para el usuario, `%LOCALAPPDATA%\Programs\Arduino IDE`, nuevo
  candidato en el bridge), package `jwplc:esp32` (`arduino-cli core list`) y una
  placa CH340 conectada.
- **Bloques.** Por defecto el instalador no toca mBlock: el `.mext` se arrastra
  a su ventana. Con `-Extension` instala los bloques como
  `extension/install-extension.cjs` (el mismo script, ejecutado con el Electron
  de mBlock).

## Paquete

`node install/build-installer.cjs` (después de `extension/build-extension.cjs`)
arma `install/dist/JWPLC-mBlock-<versión>.zip` (zip con rutas `/`, el mismo
`writeZip` del `.mext`):

~~~text
Install-JWPLC-mBlock.cmd        doble clic (opciones: -Extension, -NoAutostart, -Uninstall)
Install-JWPLC-mBlock.ps1        con BOM: Windows PowerShell 5.1 lee bien las tildes
LEEME.txt
jwplc_basic_v0.12.0.mext
bridge/jwplc-mblock-bridge.cjs
extension/install-extension.cjs, extension/dist/jwplc_basic/   (para -Extension)
~~~

## Pruebas [ejecutado]

En esta PC no se puede probar el instalador completo desde la sesión de Claude:
es una app MSIX y Windows virtualiza sus escrituras en HKCU y `%APPDATA%`
(protocolo y carpeta Inicio). Se probaron las piezas:

~~~text
mBlock.exe -e (ELECTRON_RUN_AS_NODE=1)    v16.16.0, Electron 21.4.4                       OK
bridge 0.6.0 en mBlock.exe, puerto 47830   /health 0.6.0                                    PASS
start-bridge .vbs oculto, puerto 47831     /health 0.6.0; segunda copia -> ALREADY_RUNNING,
                                           token sin cambios, un solo proceso               PASS
compilar idle_hmi.ino con ese bridge       454 909 B (igual que con Node 22)               PASS
Install-JWPLC-mBlock.ps1                   ParseFile en PowerShell 5.1.19041: 0 errores     PASS
~~~

Log: `results/20261026_gate9/GATE9A_COMPILE_ELECTRON_NODE16.log`. Para las
pruebas con un segundo bridge: `JWPLC_BRIDGE_HOME` (carpeta de trabajo) y
`JWPLC_BRIDGE_PORT`; `test/idle/compile-via-bridge.cjs` respeta ambas.

## Pendiente (propietario)

1. En esta PC, desde el Explorador: `install/dist/JWPLC-mBlock-0.12.0/Install-JWPLC-mBlock.cmd`.
   Detiene el bridge de desarrollo y deja el instalado.
2. Cerrar el bridge (Administrador de tareas, `mBlock.exe` del bridge) y subir
   desde mBlock: debe arrancarlo solo.
3. En la otra máquina: descomprimir el zip, ejecutar el `.cmd`, arrastrar el
   `.mext` a mBlock y subir un programa.
