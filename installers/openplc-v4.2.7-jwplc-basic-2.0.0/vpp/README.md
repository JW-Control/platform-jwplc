# VPP - JWPLC Basic 2.0.0

Este directorio contiene el contenido raíz del paquete `.vpp` para OpenPLC Editor 4.2.7.

## Archivos

```txt
manifest.json
assets/jwplcbasic.svg
hal/jwplcbasic.cpp
examples/
```

## Dispositivo expuesto

```txt
JWPLC BASIC [2.0.0]
```

FQBN Arduino:

```txt
jwplc:esp32:jwplcbasic
```

Board Manager URL:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

## Mapa I/O

| Canal JWPLC | Dirección IEC | Tipo |
|---|---|---|
| I0_0..I0_7 | %IX0.0..%IX0.7 | Digital Input |
| Q0_0..Q0_7 | %QX0.0..%QX0.7 | Digital Output |

No se exponen AIN/AOUT en esta alpha.

## Firma

No editar `signature.json` manualmente. Se genera con:

```powershell
node ..\tools\sign-vpp-package.mjs .\vpp jw-control-dev .\keys\jw-control-dev-private.pem
```

Para que OpenPLC Editor stock acepte el paquete, la llave pública usada debe estar en el trust store del editor o el paquete debe estar firmado por el pipeline oficial de OpenPLC.