# VPP con la numeración de la alpha: `2.1.0-alpha.32` → `2.1.0-alpha.15` (24/10/2026)

Rama: `v2.1.0-alpha.15/OpenPLC-HMI/feature/vpp-version-alpha15` (desde `v2.1.0-alpha.15/OpenPLC-HMI/main`).

```text
VPP_VERSION_ANTERIOR=2.1.0-alpha.32   (serie propia del VPP, firmada el 17/10)
VPP_VERSION_NUEVA=2.1.0-alpha.15      (= la alpha que la publica)
PAYLOAD=sin cambios salvo manifest.json (sólo package.version)
FIRMA=PASS (25/10, 3b46176, jwcontrol-2026; verifyPackageSignature del editor con TRUSTED_PACKAGE_KEYS: valid)
COMPILA_CON_PACKAGE_2_1_0_ALPHA_14=PASS [ejecutado] (§5)
FIRMA_Y_VERIFICACION=PASS [ejecutado] con una llave de prueba desechable (§5)
```

## 1. Por qué

El VPP tenía su propia serie de versiones (alpha.21 → alpha.32), que subía en cada cambio del payload y no
coincidía con las publicaciones del package: lo cerrado y publicado es **2.1.0-alpha.14** (HMI Designer) y esta
alpha, OpenPLC + HMI, sale como **2.1.0-alpha.15**. Desde aquí el VPP lleva la versión de la alpha que lo publica.

Equivalencias (sólo como historia: los gates y documentos anteriores conservan el número viejo):

| Versión vieja del VPP | Qué traía | Documento |
|---|---|---|
| 2.1.0-alpha.26 | Serial2 exclusivo del bus RS-485 (base del pre-release del editor 4.2.8-jwplc.3) | Alpha12 |
| 2.1.0-alpha.27 / .28 / .29 | IDLE con entradas forzadas, esclavo Remote I/O con plantilla | Alpha12 |
| 2.1.0-alpha.30 | HMI del proyecto OpenPLC en el HAL del maestro (F0, F0b, F0c) | `A14_HMI_V1_OPENPLC_20261017.md` |
| 2.1.0-alpha.31 | `jwplcUIUpdate()` en el scan (≈ 167 µs, F0d-2 FAIL) | `tools/jwplc-hmi-designer/gates/A14_F0d_OpenPLC_Tiempos/` |
| 2.1.0-alpha.32 | scan sin `jwplcUIUpdate()` y medición de tiempos (F0d PASS) | ídem |
| **2.1.0-alpha.15** | **mismo payload que alpha.32**, sólo cambia `package.version` | este documento |

Regla desde ahora: si el payload vuelve a cambiar dentro de la alpha, la versión sube como `2.1.0-alpha.15.1`,
`2.1.0-alpha.15.2`… (cada build firmado se distingue). Al cierre se decide con qué número sale el VPP publicado.

## 2. Impacto en el editor `[visto]` en openplc-editor

- Instalar desde archivo (`package-manager-module.ts`) **reemplaza** el paquete instalado sin comparar versiones:
  pasar de alpha.32 a alpha.15 (un número menor para semver) no bloquea nada.
- La HMI se activa por `capabilities.hmiDisplay` del dispositivo, no por versión (`src/backend/shared/compile/jwplc-hmi.ts`).
- Sólo el catálogo remoto (`catalog-browser.tsx`, `compareSemver`) compara versiones; hoy el VPP de JW no está en
  ningún catálogo remoto. Si algún día se publica ahí, la serie nueva debe ser creciente desde la que se publique.
- Las guías del editor (`docs/jwplc/JWPLC_HMI_V1_OPENPLC.md`, `REQUIERE_VPP=… alpha.30`) se ponen al día en la feature del
  editor que toque esa guía.

## 3. Cambios

- `openplc-editor-installers/v4.2.7/vpp/manifest.json`: `package.version` = `2.1.0-alpha.15`.
- `openplc-editor-installers/v4.2.7/build-jwplc-vpp-v2.bat`, ruta de la llave en este orden:
  1. la variable `JWPLC_VPP_KEY`;
  2. `KEY_PATH` del `.bat`;
  3. si no existe, el `*private*.pem` de `%USERPROFILE%\Documents\openplcProbe`, donde el propietario
     tiene la llave.
- `signature.json`: **no se cambia en la nube** (la llave privada no sale de la PC). Mientras no se firme, el
  VPP de esta rama no pasa la verificación del editor.

## 4. Firmar y probar en la PC (antes del merge)

En `C:\Users\jeykc\Documents\GitHub\platform-jwplc`, con esta rama:

```bat
git fetch origin
git checkout v2.1.0-alpha.15/OpenPLC-HMI/feature/vpp-version-alpha15
cd openplc-editor-installers\v4.2.7
rem la llave en C:\Users\jeykc\Documents\openplcProbe se encuentra sola;
rem en otra carpeta: set "JWPLC_VPP_KEY=C:\ruta\jwcontrol-2026-private.pem"
build-jwplc-vpp-v2.bat
```

Debe quedar:

- `vpp\signature.json` con `"version": "2.1.0-alpha.15"` y `"keyId": "jwcontrol-2026"`;
- `out\jwplc-basic-openplc-2.1.0-alpha.15.jwcontrol-signed.vpp`, y una copia en
  `C:\Users\jeykc\Documents\openplcProbe` (el `.bat` la deja ahí si la carpeta existe; otra carpeta con
  `set "JWPLC_VPP_DEST=…"`).

Luego:

1. OpenPLC → gestor de paquetes → agregar desde archivo → el `.vpp` nuevo: debe instalar sin error de firma y
   mostrar 2.1.0-alpha.15.
2. Abrir el proyecto de prueba de la HMI (HMIProbe) y compilar: igual que con alpha.32 (el payload no cambió).
3. Commit sólo de `vpp/signature.json` (`chore(vpp): firmar 2.1.0-alpha.15`) como `JW-Control`, push a esta rama.
   `out\` no se versiona.

```text
A14_VPP_ALPHA15_FIRMA=PASS [ejecutado] (3b46176; verificada en la nube con las llaves de confianza del editor)
A14_VPP_ALPHA15_INSTALA_EN_OPENPLC=PENDING [no verificado]
```

## 5. Verificación en la nube (24/10)

- `[ejecutado]` **Compilación ESP32** contra el package de esta rama (2.1.0-alpha.14 publicada,
  `jwplc_local:esp32:jwplcbasic`), con `tools/alpha15/regenerate_libJWPLC_TFT.sh compile`.
  - El sketch imita el build de OpenPLC:
    - `arduino.cpp` = `vpp/hal/jwplcbasic.cpp`;
    - `openplc.h` real de openplc-editor (`resources/sources/arduino`);
    - `.ino` con los buffers del runtime;
    - `jwplc_hmi.h` generado por `generateJwplcHmi()` del editor con el gate F0c (7 enlaces: `RELE`, `CONTADOR`
      ×3, `LIMITE` RW, `CONSIGNA` REAL RW, `LLENO`).
  - Header del gate F0c (Designer anterior): **PASS**, 447 781 B de flash, 39 668 B de RAM.
  - Header regenerado con el Designer de 2.1.0-alpha.14 (`cli/jwhmi.cjs generate` del mismo `.jwhmi`),
    `VPP_HMI_DIAGNOSTICS_TIMING` activo: **PASS**, 448 225 B, 39 732 B.
- `[ejecutado]` **Firma**: copia de `vpp/` firmada con `sign-vpp-package.mjs` y una llave de prueba generada
  con `generate-dev-keypair.mjs` (borrada después); `verifyPackageSignature` del editor: `{"valid":true}` con
  `version 2.1.0-alpha.15`. El editor confía en `jwcontrol-2026` (`src/backend/shared/utils/vpp/trusted-keys.ts`).
- `[ejecutado]` firma real con `jwcontrol-2026` (commit `3b46176`, 25/10): `verifyPackageSignature(vpp/, TRUSTED_PACKAGE_KEYS)`
  del editor devuelve `{"valid":true}`.
- `[no verificado]` instalación en OpenPLC y placa: §4, en la PC.
