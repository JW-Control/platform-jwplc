# JWPLC Basic v2.1.0-alpha.3

PreRelease técnico enfocado en la integración de JWPLC Basic 2.0.0 con OpenPLC Editor mediante paquete VPP y build propio **OpenPLC Editor - JWPLC Edition**.

## Resumen

Esta alpha incorpora la primera integración funcional del flujo VPP para JWPLC Basic en OpenPLC Editor 4.2.6/4.2.7.

El objetivo principal es permitir que `JWPLC BASIC [2.0.0]` aparezca como dispositivo seleccionable dentro de OpenPLC Editor usando un paquete `.vpp` firmado por JW Control y validado por un build propio del editor.

## Cambios principales

* Se reorganizan los instaladores OpenPLC en `openplc-editor-installers/`.
* Se conserva la integración legacy OpenPLC 4.1.4.
* Se agrega estructura VPP para `JWPLC BASIC [2.0.0]`.
* Se migra el HAL JWPLC Basic al paquete VPP.
* Se agrega `manifest.json` con FQBN `jwplc:esp32:jwplcbasic`.
* Se agrega mapa I/O `I0_0..I0_7` y `Q0_0..Q0_7`.
* Se agregan specs visibles del JWPLC Basic.
* Se usa preview PNG real del board.
* Se documenta el flujo de firma Ed25519 con `jwcontrol-2026`.

## Validación realizada

* El VPP firmado con `jwcontrol-2026` es aceptado por OpenPLC Editor - JWPLC Edition.
* `JWPLC BASIC [2.0.0]` aparece en Package Manager.
* `JWPLC BASIC [2.0.0]` aparece en Device Configuration.
* El preview del board se muestra correctamente.
* Las specs del board se cargan desde el manifest VPP.

## Limitaciones conocidas

* OpenPLC Editor oficial stock no acepta el VPP firmado por JW Control porque no confía en `jwcontrol-2026`.
* Para usar este VPP se requiere OpenPLC Editor - JWPLC Edition o un build que confíe en la llave pública JW Control.
* La integración OpenPLC se mantiene como integración externa/opcional.
* No se modifica el package Arduino estable ni el autoload normal de periféricos.
* No se modifica bootloader, FlashFreq, particiones, `platform.txt`, `boards.txt`, `Arduino.h` ni periféricos base.

## Pendientes posteriores

* Validar compilación y subida con sketch OpenPLC mínimo.
* Validar lectura física de `I0_0..I0_7`.
* Validar escritura física de `Q0_0..Q0_7`.
* Validar Modbus TCP con W5500.
* Validar Modbus RTU.
* Evaluar contacto con OpenPLC/Autonomy para firma oficial o inclusión de llave pública JW Control.
