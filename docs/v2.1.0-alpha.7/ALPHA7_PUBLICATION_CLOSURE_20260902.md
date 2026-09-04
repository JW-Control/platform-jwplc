# v2.1.0-alpha.7 — Cierre de publicación

Fecha: 2026-09-02

## Estado

```text
ALPHA7_STATUS=CLOSED
ALPHA7_TECHNICAL_CLOSURE=PASS
ALPHA7_PR_CI=PASS
ALPHA7_PR_MERGED=PASS
ALPHA7_PRERELEASE=PASS
ALPHA7_DEV_INDEX=PASS
```

Alpha7 queda cerrada y publicada. No se deben incorporar nuevos cambios funcionales a esta versión.

## Pull Request de release

PR principal:

```text
#76 feat(alpha7): cerrar Modbus RTU, Remote I/O y OpenPLC/VPP
Base: release/v2.1.x
Head validado: 4b720f8ce71a847d419871beaa8ccae09f9a945d
CI JWPLC Package Smoke: PASS
Merge method: rebase
release/v2.1.x después del merge: 8177a3214054833460780352fcbd8d2a29440872
```

El CI compiló correctamente los cinco smoke tests definidos por `CI JWPLC Package Smoke` y publicó sus artefactos.

## PreRelease publicada

```text
Tag    : v2.1.0-alpha.7
Nombre : v2.1.0-alpha.7 - JWPLC Arduino package
Estado : PreRelease
Target : 8177a3214054833460780352fcbd8d2a29440872
```

Artefacto:

```text
ZIP    : jwplc-esp32-2.1.0-alpha.7.zip
Size   : 24338775 bytes
SHA256 : 97bf1412ede34c34963a0d408f40d4d4fa0bd87515247f39517516bce91401a9
```

## Índice dev

El workflow de publicación generó el PR automático:

```text
#77 ci(release): update JWPLC package indexes for v2.1.0-alpha.7
```

El PR fue mergeado a `main` mediante rebase.

El índice dev publicado contiene como primera entrada:

```text
version : 2.1.0-alpha.7
url     : https://github.com/JW-Control/platform-jwplc/releases/download/v2.1.0-alpha.7/jwplc-esp32-2.1.0-alpha.7.zip
checksum: SHA-256:97bf1412ede34c34963a0d408f40d4d4fa0bd87515247f39517516bce91401a9
size    : 24338775
```

Por tanto:

```text
ALPHA7_BOARDS_MANAGER_DEV_ENTRY=PASS
```

## Precompilación al cierre

Se preservan los archives precompilados ya validados del package y el core JWPLC Basic.

`JWPLC_ModbusRTU` vuelve a `precompiled=full` con:

```text
Bytes   : 231062
SHA256  : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

`JWPLC_Ethernet` permanece deliberadamente en source-build en Alpha7.

La precompilación de Ethernet se difiere a Alpha8 para no reabrir los gates funcionales y físicos ya cerrados.

## Pendientes transferidos a Alpha8

```text
ALPHA8_ETHERNET_PRECOMPILE=PENDING
ALPHA8_BUILD_BENCHMARK=PENDING
ALPHA8_WORKSHOP_EXAMPLES=PENDING
```

Los ejemplos didácticos completos del taller se compartirán por separado y se integrarán al repositorio en Alpha8. Los ejemplos técnicos Modbus/Remote I/O de Alpha7 sí permanecen versionados.

## Decisiones que permanecen pendientes

Alpha7 no define:

- OTA;
- FlashFreq/configuración universal definitiva;
- `bootloader.bin` definitivo;
- migración a ESP32-S3;
- OpenPLC como runtime obligatorio del package Arduino;
- proceso canónico futuro de firma VPP.

App-only permanece como herramienta auxiliar de desarrollo y no como upload por defecto.

## Cierre

```text
ALPHA7_STATUS=CLOSED
NEXT_ALPHA=ALPHA8
```
