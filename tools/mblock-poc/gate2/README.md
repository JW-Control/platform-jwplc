# JWPLC Basic - mBlock PoC - Gate 2

## Objetivo

Comprobar si un Custom Upload Middleware configurado mediante
mBlock Extension Builder puede ejecutar un proceso externo
desde mBlock Desktop.

## Arquitectura evaluada

~~~text
Device JWPLC Basic
    -> Arduino C
    -> Custom Upload Middleware
    -> child_process
    -> arduino-cli version
~~~

## Middleware

Nombre:

~~~text
jwplc_probe
~~~

Archivo:

~~~text
jwplc_probe_upload_middleware.js
~~~

Ubicación en Extension Builder:

~~~text
My Plugin
    -> My Device
    -> JWPLC Basic PoC
    -> Connection settings
    -> Middleware
    -> Add
    -> Custom
~~~

## Gate 2

El middleware intenta dos rutas:

~~~text
1. require("child_process")
2. window.MbApi.require("child_process")
~~~

La segunda es una interfaz interna detectada en mBlock 5.4.3
y se utiliza únicamente con fines de PoC.

## PASS

Se requiere:

~~~text
GATE2_MIDDLEWARE_INVOKED=YES
GATE2_CHILD_PROCESS_AVAILABLE=YES
GATE2_ARDUINO_CLI_EXECUTED=YES
GATE2_ARDUINO_CLI_EXIT=0
GATE2_RESULT=PASS
~~~

## Importante

El `arduino-cli` encontrado actualmente mediante PATH es:

~~~text
C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe
Arduino CLI 1.0.2
~~~

Esto es aceptable únicamente para Gate 2.

No debe asumirse todavía como CLI de compilación JWPLC.

Antes del Gate de compilación se localizará y fijará
explícitamente la instalación Arduino CLI utilizada por
el package JWPLC.

## Restricciones

Gate 2:

- no compila;
- no hace upload;
- no modifica hardware;
- no usa esptool;
- no fija FlashFreq;
- no asume app-only;
- no modifica platform-jwplc.
