# v2.1.0-alpha.9 — Cierre técnico

Fecha: 2026-09-04

## Objetivo

Cerrar Alpha9 con una integración OpenPLC/Backplane físicamente usable y reproducible en el perfil de taller validado, sin convertir OpenPLC en parte obligatoria del runtime Arduino y sin romper el package normal del JWPLC Basic.

El alcance final de Alpha9 se concentra en:

1. Backplane OpenPLC + Remote I/O Modbus RTU;
2. recorrido FC02 -> Ladder -> FC15 -> FC01;
3. persistencia y recuperación del enlace;
4. VPP JWPLC firmado y verificado;
5. artefactos de taller para Windows;
6. documentación explícita de lo validado y lo diferido.

## Base Git

La rama histórica de trabajo Alpha9 quedó creada sobre una fotografía anterior de `release/v2.1.x` y posteriormente `release` recibió metadata canónica de Alpha8.

Para evitar reintroducir divergencia, el cierre final se integra desde una rama nueva creada directamente desde el HEAD actual de `release/v2.1.x`:

```text
release/v2.1.x @ d5e2d360731e9bae5a2db0f7ee30213986c050cf
  -> v2.1.0-alpha.9/integration/final-closure
```

Del branch histórico Alpha9 se preserva únicamente el README canónico del ejemplo Remote I/O Slave RTU. No se hace merge ciego de una base divergente.

```text
ALPHA9_FINAL_BASE_CORRECT=PASS
```

## Topología física validada

```text
JWPLC Master
  OpenPLC Editor / Ladder
  COM4
  RTU 115200 8N1
       |
       | RS-485
       v
JWPLC Slave
  Arduino sketch JWPLC_RemoteIO_Slave_RTU
  COM14
  Slave ID 2
  RTU 115200 8N1
```

El Slave Arduino reportó:

```text
SLAVE_UPLOAD=PASS
SLAVE_ID=2
RTU=115200_8N1
MODBUS_READY=YES
SLAVE_PORT=COM14
```

El Master OpenPLC compiló y subió correctamente por COM4 usando el proyecto de loopback Remote I/O 8 canales.

## Recorrido funcional

Ruta validada:

```text
Slave I0_n
   -> FC02 Read Discrete Inputs
   -> OpenPLC / Ladder
   -> FC15 Write Multiple Coils
   -> Slave Q0_n físico
   -> FC01 Read Coils
   -> feedback al Master
```

### Gate 8 canales one-hot

Se probó:

```text
ALL_OFF_INITIAL = 0x00
I0_0            = 0x01
I0_1            = 0x02
I0_2            = 0x04
I0_3            = 0x08
I0_4            = 0x10
I0_5            = 0x20
I0_6            = 0x40
I0_7            = 0x80
ALL_OFF_FINAL   = 0x00
```

Cada estado obtuvo correlación exacta de protocolo y hardware, con al menos una muestra `valid=1` donde `requested == feedback` y `mismatch=0`.

Resultado:

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
FC02_REMOTE_INPUT_BITS=8/8 PASS
OPENPLC_LADDER_MAPPING=8/8 PASS
FC15_REMOTE_OUTPUT_BITS=8/8 PASS
FC01_OUTPUT_FEEDBACK=8/8 PASS
BIT_POSITION_MAPPING=8/8 PASS
CROSSED_BITS=0
PHYSICAL_CORRELATION=8/8 PASS
NEW_RTU_FAILURES=0
NEW_MISMATCHES=0
```

Las muestras transitorias `valid=0` no se consideran fallo del gate cuando existen muestras válidas exactas del mismo estado y los contadores de fallo/mismatch no aumentan.

### Limitación del banco

El banco físico permitía activar una única entrada a la vez.

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

Alpha9 no sobredeclara simultaneidad multibit a partir de evidencia inexistente.

## Persistencia del Backplane

Se verificaron los archivos persistidos del proyecto antes y después de cerrar/reabrir:

```text
PROJECT_HASH_BEFORE=f63bcce756686e2c932805ac347cb9c5bedaa4fe147654084b4c0889f6329765
DEVICE_HASH_BEFORE=f68a3d612ec32da8f3fad7a4ec4796aeb31c7ada26024627e7268f0df3841853
```

Después de reabrir el proyecto los hashes se conservaron y se realizó una nueva compilación/subida al Master.

```text
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
```

## Recuperación tras power-cycle del Slave

Con el Master ejecutándose sin reinicio:

1. se apagó el Slave;
2. FC02/FC15/FC01 registraron fallos esperados durante la ausencia;
3. se volvió a alimentar el Slave;
4. la comunicación recuperó automáticamente;
5. se verificó de nuevo `I0_0 -> Q0_0` físico;
6. se regresó al estado `0x00` válido.

Resultado:

```text
SLAVE_POWER_CYCLE_DETECTED=PASS
RECOVERY_AFTER_SLAVE_RESET=PASS
POST_RECOVERY_FC02=PASS
POST_RECOVERY_LADDER=PASS
POST_RECOVERY_FC15=PASS
POST_RECOVERY_FC01=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
RTU_AUTOMATIC_RECOVERY=PASS
```

## Perfil RTU validado

El HAL/VPP actual mantiene:

```cpp
static constexpr uint32_t JWPLC_MODBUS_BAUD = 115200UL;
static constexpr uint32_t JWPLC_MODBUS_CONFIG = SERIAL_8N1;
```

Alpha9 congela formalmente este perfil como el perfil validado:

```text
BACKPLANE_FIXED_RTU_PROFILE_115200_8N1=PASS
```

El `Slave ID` sí se propaga desde el Backplane por slot y fue validado con valor `2`.

La selección de baudrate/formato desde UI todavía no existe en el Backplane JWPLC. El soporte genérico del Editor para parámetros RTU no debe confundirse con la configuración del bus Master del Backplane.

## VPP JWPLC Basic OpenPLC

Estado validado del VPP contenido en `platform-jwplc`:

```text
Package ID : com.jwcontrol.jwplc-basic
Versión    : 2.1.0-alpha.19
Algoritmo  : ed25519
Key ID     : jwcontrol-2026
```

Los nueve archivos declarados en `signature.json` fueron verificados contra el payload físico y contra el `.vpp` final:

```text
SIGNED_FILE_HASHES=9/9 PASS
EMBEDDED_MANIFEST=PASS
EMBEDDED_SIGNATURE_METADATA=PASS
EMBEDDED_SIGNED_PAYLOAD=PASS
```

Artefacto de taller:

```text
JWPLC-Basic-OpenPLC-2.1.0-alpha.19.jwcontrol-signed.vpp
Bytes   : 1260931
SHA-256 : E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
```

No se resignó ni se cambió el versionado del VPP para igualarlo artificialmente a Alpha9.

## JWPLC Editor — build de taller

Se construyó:

```text
OpenPLC Editor - JWPLC Edition
Version: 4.2.8-jwplc.2
Target : NSIS x64
Bytes  : 133699500
SHA256 : 79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
```

Toolchain usado:

```text
Node v22.23.1
npm 10.9.8
```

El build terminó con:

```text
NPM_PACKAGE_EXIT_CODE=0
FRESH_INSTALLER=PASS
WORKTREE_PRESERVED=PASS
NSIS_X64_BUILD=PASS
WORKSHOP_EDITOR_INSTALLER=PASS
```

`AUTHENTICODE_STATUS=NotSigned` queda registrado. No se confunde con la firma Ed25519 del VPP.

El popup DEV observado durante el build buscaba `configs/dll/main.js`; no bloqueó el build de producción empaquetado desde `release/app` y se registra como evento no bloqueante del entorno de desarrollo.

### Trazabilidad del Editor

El instalador de taller fue construido desde:

```text
Branch: develop/alpha7-openplc-remote-io-rtu
HEAD  : 51843ca8beca06a72a57f56c2844cc1013859414
Dirty entries: 19
Worktree fingerprint:
11B95D5291D34D02F422B971E010DA7C5C7166D6244ADC653132AD1EFD83A505
```

Por ello se considera un **artefacto de taller validado**, no todavía una build reproducible a partir del branch remoto limpio. La consolidación de esos cambios del Editor debe hacerse en su repositorio propio en un ciclo posterior.

## Coexistencia previa RTU + Modbus TCP/debugger

El ciclo Alpha18 del VPP ya dejó evidencia de coexistencia RTU Backplane + Modbus TCP debugger con sesiones TCP activas y sin crecimiento de fallos RTU.

Alpha9 no repite la batería larga de Alpha18 únicamente para cerrar el perfil fijo; se conserva esa evidencia como antecedente y no se presenta como una nueva medición Alpha9 posterior a todos los cambios.

## Autoload Arduino

Alpha9 no elimina ni desactiva:

- Display;
- Ethernet/W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA/I/O;
- mutex SPI global.

```text
OPENPLC_IN_ARDUINO_AUTOLOAD=NO
AUTOLOAD_PERIPHERALS_PRESERVED=PASS
```

## Decisiones heredadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se publica un `bootloader.bin` definitivo.

## Pendientes explícitos transferidos

Alpha9 cierra su perfil fijo validado y transfiere sin ocultarlos:

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
MULTIBIT_SIMULTANEOUS=NOT_TESTED
ALPHA9_TIMER_FB_MEMBER_REFERENCE=PENDING
TON_Q_REFERENCE=REQUIRED
TOF_Q_REFERENCE=REQUIRED
TP_Q_REFERENCE=REQUIRED
FB_MEMBER_TYPE_VALIDATION=REQUIRED
FB_MEMBER_AUTOCOMPLETE=REQUIRED
OPENPLC_EDITOR_REMOTE_SOURCE_CONSOLIDATION=PENDING
HMI_TO_LADDER_EXPOSURE=PENDING
```

No se permite resolver `TON0.Q` simplemente aceptando puntos en cualquier nombre de variable. La solución futura debe distinguir declaraciones IEC de referencias tipadas a miembros de Function Blocks.

## Estado técnico final

```text
ALPHA9_FINAL_BASE_CORRECT=PASS
ALPHA9_SLAVE_PROGRAMMING_GATE=PASS
ALPHA9_MASTER_BUILD_UPLOAD=PASS
ALPHA9_BACKPLANE_8CH_ONE_HOT=PASS
ALPHA9_BACKPLANE_PERSISTENCE=PASS
ALPHA9_BACKPLANE_RECOVERY=PASS
ALPHA9_BACKPLANE_FIXED_PROFILE_115200_8N1=PASS
ALPHA9_VPP_SIGNED_PAYLOAD=PASS
ALPHA9_WORKSHOP_ARTIFACT_BUNDLE=PASS
ALPHA9_AUTOLOAD_PRESERVED=PASS
ALPHA9_TECHNICAL_CLOSURE=PASS
ALPHA9_STATUS=TECHNICALLY_CLOSED
ALPHA9_PUBLICATION=PENDING_PR_CI_RELEASE
```
