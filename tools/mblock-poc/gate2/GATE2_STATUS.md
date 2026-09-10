# JWPLC Basic - mBlock PoC - Gate 2 Status

## Estado hasta Gate 2H

~~~text
GATE2_HOST_ARDUINO_CLI=PASS

GATE2A_MINIMAL_MIDDLEWARE_STATIC=PASS
GATE2B_TYPEOF_REQUIRE_STATIC=PASS
GATE2C_REQUIRE_CHILD_PROCESS_STATIC=PASS

GATE2_DEVICE_IMPORT=PASS
GATE2_DEVICE_UPDATE=PASS
GATE2_DEVICE_DISCOVERY=PASS
GATE2_DEVICE_ADD=PASS

GATE2H_EVENT_HANDLER_EXPORT=PASS

GATE2_RUNTIME_CHILD_PROCESS=PENDING
~~~

## Arduino CLI detectado

~~~text
C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe
Version: 1.0.2
~~~

Este CLI se utiliza únicamente como probe en Gate 2.

No se adopta todavía como CLI final para compilar JWPLC.

## Arquitectura encontrada en mBlock

El Device instalado utiliza:

~~~text
manifest.json
    feature: ["worker"]

index.js
    -> worker.remote.runExtension("onLoad", ...)

src/handlerProxy.js
    -> Extension Event Handler generado
~~~

Por lo tanto, `src/exthandler.js` puede conservar handlers
vacíos mientras el código personalizado generado se encuentra
en `src/handlerProxy.js`.

## Gate 2H

Device instalado:

~~~text
id=jwplc_basic_poc
version=0.0.4
~~~

Marcador encontrado:

~~~text
GATE2H_ONLOAD_EXPORT=YES
~~~

Ubicación:

~~~text
src/handlerProxy.js
~~~

Conclusión:

~~~text
DEVELOPER_PLATFORM_SAVE=PASS
MEXT_EXPORT=PASS
MBLOCK_UPDATE=PASS
EVENT_HANDLER_GENERATION=PASS
~~~

## Anomalía pendiente

mBlock muestra ocasionalmente:

~~~text
File error. Install Failed
~~~

al añadir JWPLC desde Device Library.

Sin embargo:

~~~text
JWPLC Updated Successfully
DEVICE_VISIBLE=YES
DEVICE_ADDABLE=YES
WORKSPACE_LOAD=YES
~~~

Por ahora se clasifica como:

~~~text
MBLOCK_INSTALL_FAILED_TOAST=NON_BLOCKING_ANOMALY
~~~

No se considera causa para detener el PoC.

## Gate 2F

El log bruto de Gate 2F no se versiona porque el scanner
incluyó aproximadamente 12 MB de información irrelevante.

Gate 2G y Gate 2H sustituyen esa evidencia con pruebas
dirigidas y reproducibles.

## Próximo objetivo

~~~text
GATE2I_RUNTIME_PROCESS_EXECUTION
~~~

Comprobar en runtime:

~~~text
extension.onLoad
    -> require("child_process")
    -> execFile(arduino-cli, ["version"])
    -> resultado visible mediante variable global de mBlock
~~~

Gate 2I:

- no compila;
- no hace upload;
- no toca el JWPLC Basic.
