# v2.1.0-alpha.9 — Cierre de publicación

Fecha: 2026-09-04

## Resultado

`v2.1.0-alpha.9` queda publicada como GitHub PreRelease y disponible en el índice dev del Boards Manager.

```text
ALPHA9_TECHNICAL_CLOSURE=PASS
ALPHA9_RELEASE_PUBLICATION=PASS
ALPHA9_DEV_INDEX=PASS
ALPHA9_MAIN_SYNC=PASS
ALPHA9_STATUS=CLOSED_PUBLISHED
```

## GitHub PreRelease

```text
Tag: v2.1.0-alpha.9
Nombre: v2.1.0-alpha.9 - JWPLC Arduino package
Target commit: 103ad0c3d578b5f1eb68a92d363746ba373bf643
PreRelease: true
Published at: 2026-09-04T09:40:03Z
```

Artefacto publicado:

```text
jwplc-esp32-2.1.0-alpha.9.zip
Bytes: 24464282
SHA-256: 015679533e13dabbe79041771e1e85d3011970dd0c69bc62e3b51f3101043907
```

El SHA-256 registrado por GitHub para el asset coincide con el metadata generado por el workflow de release.

## Automatización

La publicación se ejecutó mediante el flujo oficial:

```text
README marker
  -> Auto Release JWPLC from README
  -> Release JWPLC Arduino Package
  -> ZIP + checksum + size
  -> GitHub PreRelease
  -> branch ci/release-index-v2.1.0-alpha.9
  -> PR #85 hacia main
```

Resultados:

```text
AUTO_RELEASE_WORKFLOW=PASS
RELEASE_PACKAGE_WORKFLOW=PASS
PR_TECHNICAL_84=MERGED
PR_INDEX_85=MERGED
```

## Índice dev

`JWPLC/package_jwplc_index_dev.json` expone como primera entrada:

```text
version: 2.1.0-alpha.9
archiveFileName: jwplc-esp32-2.1.0-alpha.9.zip
checksum: SHA-256:015679533e13dabbe79041771e1e85d3011970dd0c69bc62e3b51f3101043907
size: 24464282
```

```text
DEV_INDEX_ALPHA9=PASS
```

El índice estable continúa en:

```text
2.0.0
```

```text
STABLE_INDEX_UNCHANGED=PASS
```

## Sincronización `release/v2.1.x` -> `main`

Durante el cierre se comprobó que el ruleset activo `Protect main` exige `required_linear_history` y no dispone de bypass.

La historia heredada de `release/v2.1.x` contiene merge commits. Por ello:

1. un fast-forward directo de `main` hacia `release/v2.1.x` fue rechazado por el ruleset porque introduciría merge commits;
2. `Create a merge commit` fue rechazado por la misma regla;
3. `Rebase and merge` no fue aceptado por GitHub para la historia compleja del PR de sincronización;
4. el PR #82 se integró mediante `Squash and merge`, única vía permitida sin reescribir ni forzar ninguno de los branches.

Commit de sincronización en `main`:

```text
24da8347d186fbaa37f3e5b867307a07ea0937ab
```

Antes de integrar el índice Alpha9, el tree SHA del commit de sincronización de `main` fue exactamente el mismo que el tree SHA del HEAD técnico publicado de `release/v2.1.x`:

```text
release tree: e4027a3b071906283b9498239845e384e082d411
main tree:    e4027a3b071906283b9498239845e384e082d411
```

Por tanto:

```text
MAIN_LINEAR_HISTORY_PRESERVED=PASS
RELEASE_TO_MAIN_SYNC_METHOD=SQUASH
RELEASE_MAIN_TREE_PARITY_BEFORE_INDEX=PASS
RELEASE_IS_ANCESTOR_OF_MAIN=NOT_APPLICABLE_UNDER_CURRENT_RULESET
```

El antiguo requisito de ancestría literal se reemplaza por paridad de árbol + trazabilidad explícita mientras `main` mantenga `required_linear_history` y `release/v2.1.x` conserve merge commits históricos.

No se realizó force-push ni reescritura de `release/v2.1.x`.

## Backplane OpenPLC publicado

Alpha9 publica el perfil físicamente validado:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

Recorrido validado:

```text
Slave DI -> FC02 -> OpenPLC/Ladder -> FC15 -> Slave DO -> FC01 -> feedback
```

Marcadores:

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
FC02_REMOTE_INPUT_BITS=8/8 PASS
OPENPLC_LADDER_MAPPING=8/8 PASS
FC15_REMOTE_OUTPUT_BITS=8/8 PASS
FC01_OUTPUT_FEEDBACK=8/8 PASS
BIT_POSITION_MAPPING=8/8 PASS
CROSSED_BITS=0
PHYSICAL_CORRELATION=8/8 PASS
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
RTU_AUTOMATIC_RECOVERY=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
```

Limitación declarada:

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

## VPP validado

```text
JWPLC Basic OpenPLC VPP: 2.1.0-alpha.19
Package ID: com.jwcontrol.jwplc-basic
Signature: ed25519
Key ID: jwcontrol-2026
Signed payload: 9/9 PASS
```

Artefacto de taller:

```text
JWPLC-Basic-OpenPLC-2.1.0-alpha.19.jwcontrol-signed.vpp
Bytes: 1260931
SHA-256: E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
```

## JWPLC Editor — artefacto de taller

```text
OpenPLC Editor - JWPLC Edition 4.2.8-jwplc.2
NSIS x64
Bytes: 133699500
SHA-256: 79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
```

Instalación y carga del VPP final de `JWPLC_TALLER` verificadas.

```text
WORKSHOP_EDITOR_INSTALLER=PASS
WORKSHOP_SIGNED_VPP=PASS
WORKSHOP_ARTIFACT_BUNDLE=PASS
```

El instalador no dispone de Authenticode:

```text
AUTHENTICODE_STATUS=NotSigned
```

Esto no se confunde con la firma Ed25519 del VPP.

## Autoload Arduino

Se preservan:

- Display/TFT;
- Ethernet W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA/I/O;
- mutex SPI global.

```text
AUTOLOAD_PERIPHERALS_PRESERVED=PASS
OPENPLC_IN_ARDUINO_AUTOLOAD=NO
```

## Decisiones que permanecen

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo.

## Pendientes transferidos a Alpha10

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
ALPHA9_TIMER_FB_MEMBER_REFERENCE=PENDING
TON_Q_REFERENCE=REQUIRED
TOF_Q_REFERENCE=REQUIRED
TP_Q_REFERENCE=REQUIRED
FB_MEMBER_TYPE_VALIDATION=REQUIRED
FB_MEMBER_AUTOCOMPLETE=REQUIRED
MULTIBIT_SIMULTANEOUS=NOT_TESTED
OPENPLC_EDITOR_REMOTE_SOURCE_CONSOLIDATION=PENDING
HMI_TO_LADDER_EXPOSURE=PENDING
```

## Cierre

```text
ALPHA9_RELEASE_TAG=PASS
ALPHA9_RELEASE_ASSET=PASS
ALPHA9_RELEASE_SHA256=PASS
ALPHA9_RELEASE_SIZE=PASS
ALPHA9_DEV_INDEX=PASS
ALPHA9_STABLE_INDEX_UNCHANGED=PASS
ALPHA9_MAIN_SYNC=PASS
ALPHA9_DOCUMENTATION=PASS
ALPHA9_STATUS=CLOSED_PUBLISHED
NEXT=ALPHA10
```
