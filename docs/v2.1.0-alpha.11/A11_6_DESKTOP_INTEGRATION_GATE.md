# Alpha11 · A11-6 Integración de escritorio y sketch

Fecha: 2026-09-06

## Objetivo

Convertir el PoC web del JWPLC HMI Designer en una herramienta de uso diario sin exigir al usuario arrancar manualmente `localhost`, manteniendo intactos Web Serial/LIVE y el codegen ya validado.

La integración no depende de APIs privadas de Arduino IDE ni requiere mantener un fork del IDE.

## Arquitectura

```text
JWPLC HMI Designer
  -> launcher Windows
  -> servidor localhost privado
  -> Edge/Chrome --app
  -> desktop.html
  -> Designer existente + designer-project.js

Sketch Arduino
  -> Proyecto.ino
  -> JWPLC_HMI_Generated.h
```

El servidor localhost existe sólo para conservar el contexto seguro requerido por Web Serial y File System Access. El usuario no debe iniciarlo manualmente.

## Funciones A11-6

```text
DESKTOP_LAUNCHER=YES
LOCALHOST_MANUAL_START=NO
EDGE_CHROME_APP_MODE=YES
PWA_MANIFEST=YES
SERVICE_WORKER=YES
NETWORK_FIRST_CACHE=YES

PROJECT_OPEN_JWHMI=YES
PROJECT_SAVE_JWHMI=YES
PROJECT_FORMAT_VERSION=1
PROJECT_DECLARATIVE_LAYER=YES
RAW_PIXEL_PROJECT_PERSISTENCE=NO_ALPHA11

SKETCH_FOLDER_LINK=YES
SKETCH_MUST_CONTAIN_INO=YES
LINK_HANDLE_PERSISTENCE=INDEXED_DB
GENERATED_HEADER_WRITE=YES
GENERATED_HEADER_NAME=JWPLC_HMI_Generated.h
GENERATED_HEADER_OVERWRITE_CONFIRM=YES
DUPLICATE_IDENTIFIER_BLOCKS_WRITE=YES

LIVE_WEB_SERIAL=PRESERVED
ARDUINO_IDE_FORK_REQUIRED=NO
```

## Launcher

Archivos:

```text
tools/jwplc-hmi-designer/JWPLC-HMI-Designer.cmd
tools/jwplc-hmi-designer/Start-JWPLC-HMI-Designer.ps1
tools/jwplc-hmi-designer/JWPLC-HMI-Server.ps1
tools/jwplc-hmi-designer/Install-JWPLC-HMI-Designer.ps1
```

El launcher:

1. verifica si el servidor local ya está activo;
2. si no está activo, inicia el servidor oculto en `127.0.0.1:8765`;
3. espera el endpoint `/__health`;
4. abre Edge o Chrome con `--app=http://127.0.0.1:8765/desktop.html`;
5. usa el navegador predeterminado sólo como fallback.

El servidor se cierra por inactividad después de 60 minutos.

## Vinculación con sketch

El botón `Vincular sketch…` usa File System Access API. La carpeta seleccionada debe contener al menos un `.ino`.

Una vez vinculada, `Actualizar HMI`:

1. valida IDs/variables C++;
2. genera el output actual;
3. exige confirmación si `JWPLC_HMI_Generated.h` ya existe;
4. escribe directamente el header dentro de la carpeta del sketch.

No se escribe ni modifica el `.ino` del usuario.

## Proyecto `.jwhmi`

Formato Alpha11:

```text
format=JWPLC_HMI_PROJECT
version=1
pages<=16
fields<=32
target=ST7789_320x170_ROT3_RGB565
```

Se persisten páginas, fields declarativos, propiedades, geometría, colores, variables/IDs y página activa.

Limitación explícita:

```text
RAW_GFX_LAYER_PERSISTENCE=NO_ALPHA11
PIXEL_LAYER_PERSISTENCE=NO_ALPHA11
```

Estas herramientas siguen siendo técnicas y no forman parte del artefacto declarativo de producción en Alpha11.

## Gate manual

En Windows + Edge/Chrome:

```text
1. Ejecutar JWPLC-HMI-Designer.cmd.
2. Confirmar apertura en ventana app sin arrancar servidor manualmente.
3. Guardar proyecto .jwhmi.
4. Cerrar y abrir proyecto .jwhmi.
5. Confirmar páginas/fields/propiedades.
6. Vincular una carpeta de sketch que contenga .ino.
7. Pulsar Actualizar HMI.
8. Confirmar overwrite cuando exista header.
9. Confirmar que JWPLC_HMI_Generated.h cambió en Arduino IDE.
10. Compilar/subir sketch.
11. Conectar LIVE y confirmar Web Serial operativo.
```

Criterio de salida:

```text
A11_6_DESKTOP_LAUNCHER=PASS
A11_6_PROJECT_SAVE_OPEN=PASS
A11_6_SKETCH_LINK=PASS
A11_6_HEADER_DIRECT_WRITE=PASS
A11_6_LIVE_PRESERVED=PASS
A11_6_SKETCH_INTEGRATION=PASS
```

Hasta completar la prueba manual:

```text
A11_6_SKETCH_INTEGRATION=IMPLEMENTED_PENDING_GATE
```
