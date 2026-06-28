# OpenPLC Editor v4.2.7 + JWPLC Basic 2.0.0

Etapa: `v2.1.0-alpha.3`  
Objetivo: migrar la integración existente de JWPLC Basic 2.0.0 al flujo de paquetes `.vpp` usado por OpenPLC Editor 4.2.7.

## Resumen

OpenPLC Editor 4.2.7 ya no debe integrarse únicamente sobrescribiendo el catálogo interno `hals.json`. La vía nueva es un **Vendor Plugin Package** (`.vpp`) con `manifest.json`, assets, HAL y firma (`signature.json`).

Esta carpeta contiene:

```txt
openplc-v4.2.7-jwplc-basic-2.0.0/
├── README.md
├── docs/
│   └── OPENPLC_4_2_7_VPP_NOTES.md
├── tools/
│   ├── apply-openplc-4.2.7-jwplc-patch.ps1
│   ├── build-unsigned-vpp.ps1
│   ├── generate-dev-keypair.mjs
│   └── sign-vpp-package.mjs
└── vpp/
    ├── manifest.json
    ├── README.md
    ├── assets/
    │   └── jwplcbasic.svg
    ├── examples/
    │   ├── JWPLC_Basic_Blink_Q0_0/README.md
    │   └── JWPLC_Basic_IO_Test_I0_to_Q0/README.md
    └── hal/
        └── jwplcbasic.cpp
```

## Estado actual

- Estructura VPP creada.
- Manifest VPP creado para `JWPLC BASIC [2.0.0]`.
- HAL JWPLC migrado al paquete VPP.
- Scripts de empaquetado y firma agregados.
- Parche de compatibilidad 4.2.7 agregado para reutilizar los archivos ya validados en `installers/openplc-jwplc-basic-v2.0.0`.

## Importante sobre la firma

OpenPLC Editor 4.2.7 exige firma criptográfica para importar paquetes `.vpp` desde archivo. Por eso, el archivo generado por `build-unsigned-vpp.ps1` sirve para revisar estructura, pero **no será aceptado por OpenPLC Editor stock** hasta que exista un `signature.json` firmado con una llave confiable para el editor.

Para pruebas internas hay dos caminos:

1. Firmar con una llave JW Control y usar un build propio de OpenPLC Editor que confíe en esa llave.
2. Coordinar con OpenPLC/Autonomy para que el paquete sea firmado por su pipeline o para que agreguen la llave pública JW Control.

## Flujo recomendado para alpha3

1. Instalar OpenPLC Editor 4.2.7 limpio.
2. Instalar el package Arduino JWPLC 2.0.0 desde Boards Manager.
3. Aplicar el parche de compatibilidad:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\apply-openplc-4.2.7-jwplc-patch.ps1
```

4. Generar el `.vpp` sin firma para inspección:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-unsigned-vpp.ps1
```

5. Firmar el paquete con llave de desarrollo, solo si se está usando un build de OpenPLC que confía en la llave JW Control.
6. Validar selección de `JWPLC BASIC [2.0.0]`, compilación, subida USB, I/O, Modbus TCP y depuración.

## Qué no modifica

Esta etapa no modifica:

- `boards.txt`
- `platform.txt`
- `Arduino.h`
- `peripherals_init.cpp`
- `jwplc_peripherals.cpp`
- `bootloader.bin`
- particiones
- FlashFreq
- autoload normal de periféricos

La integración OpenPLC se mantiene como integración externa/opcional.