# v2.1.0-alpha.9 — PreRelease JWPLC Arduino package

## Resumen

`v2.1.0-alpha.9` cierra el ciclo de validación del **JWPLC Basic como Master OpenPLC con Remote I/O Modbus RTU**, usando el Backplane/VPP externo al runtime Arduino y manteniendo intacto el autoload normal del package.

El alcance publicado congela el perfil RTU físicamente validado para el taller:

```text
Master OpenPLC: 115200 / 8N1
Slave Arduino : 115200 / 8N1
Slave ID      : 2
```

OpenPLC continúa siendo una integración externa/opcional. Esta PreRelease no convierte OpenPLC en parte obligatoria del runtime Arduino.

## Cambios principales

- formalización del ejemplo canónico `JWPLC_RemoteIO_Slave_RTU` para usar un JWPLC Basic como Remote I/O Slave;
- cierre del recorrido físico `FC02 -> Ladder -> FC15 -> FC01`;
- validación de los 8 bits `I0_0..I0_7` / `Q0_0..Q0_7` uno por uno;
- persistencia del Backplane tras cerrar/reabrir proyecto y recompilar;
- recuperación automática tras power-cycle del Slave sin reiniciar el Master;
- preservación del VPP JWPLC Basic OpenPLC `2.1.0-alpha.19` firmado con Ed25519;
- distribución de taller validada con `JWPLC Editor - JWPLC Edition 4.2.8-jwplc.2` y el VPP firmado.

## Validación física del Backplane

Topología:

```text
JWPLC Master
  OpenPLC / Ladder
  COM4
  115200 8N1
       |
       | RS-485
       v
JWPLC Slave
  Arduino
  JWPLC_RemoteIO_Slave_RTU
  COM14
  Slave ID 2
  115200 8N1
```

Recorrido validado:

```text
Slave DI
  -> FC02 Read Discrete Inputs
  -> OpenPLC / Ladder
  -> FC15 Write Multiple Coils
  -> Slave DO físico
  -> FC01 Read Coils
  -> feedback al Master
```

Resultado del gate 8 canales:

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
FC02_REMOTE_INPUT_BITS=8/8 PASS
OPENPLC_LADDER_MAPPING=8/8 PASS
FC15_REMOTE_OUTPUT_BITS=8/8 PASS
FC01_OUTPUT_FEEDBACK=8/8 PASS
BIT_POSITION_MAPPING=8/8 PASS
PHYSICAL_CORRELATION=8/8 PASS
CROSSED_BITS=0
NEW_RTU_FAILURES=0
NEW_MISMATCHES=0
```

El banco físico sólo permitía activar una entrada a la vez. Por ello:

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

No se sobredeclara soporte simultáneo multibit a partir de una prueba que no se realizó.

## Persistencia y recuperación

Se verificó que la configuración del proyecto/Backplane se conserva tras cerrar y reabrir el proyecto, recompilar y volver a subir al Master.

También se realizó power-cycle del Slave manteniendo el Master encendido. Durante la ausencia del Slave aumentaron los fallos RTU esperados y, al regresar el nodo, FC01/FC02/FC15 recuperaron automáticamente la comunicación.

Marcadores:

```text
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
SLAVE_POWER_CYCLE_DETECTED=PASS
RECOVERY_AFTER_SLAVE_RESET=PASS
POST_RECOVERY_FC02=PASS
POST_RECOVERY_LADDER=PASS
POST_RECOVERY_FC15=PASS
POST_RECOVERY_FC01=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
```

## VPP JWPLC Basic OpenPLC

El VPP integrado en el repositorio conserva su versionado interno independiente del Arduino package:

```text
VPP_VERSION=2.1.0-alpha.19
PACKAGE_ID=com.jwcontrol.jwplc-basic
SIGNATURE_ALGORITHM=ED25519
SIGNATURE_KEY_ID=jwcontrol-2026
SIGNED_PAYLOAD=9/9 PASS
```

El artefacto utilizado en el taller fue verificado byte a byte:

```text
JWPLC-Basic-OpenPLC-2.1.0-alpha.19.jwcontrol-signed.vpp
Bytes: 1260931
SHA-256: E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
```

El versionado es deliberadamente distinto:

```text
JWPLC Arduino package : v2.1.0-alpha.9
JWPLC OpenPLC VPP      : 2.1.0-alpha.19
```

No se renombra ni se vuelve a firmar el VPP sólo para igualar el número del Arduino package.

## JWPLC Editor — artefacto de taller

Se validó un instalador Windows x64:

```text
Producto : OpenPLC Editor - JWPLC Edition
Versión  : 4.2.8-jwplc.2
Formato  : NSIS x64
Bytes    : 133699500
SHA-256  : 79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
```

Este instalador se generó desde el snapshot local de trabajo ya validado del fork OpenPLC Editor y se utiliza como artefacto de taller. La consolidación/publicación reproducible de esos cambios del Editor en su repositorio propio queda fuera del Arduino package Alpha9.

## Perfil RTU congelado para Alpha9

El HAL/VPP actualmente usa:

```text
JWPLC_MODBUS_BAUD   = 115200
JWPLC_MODBUS_CONFIG = SERIAL_8N1
```

Alpha9 valida y publica este perfil fijo.

La futura UI de Backplane para elegir baudrate/formato y propagar esos valores al HAL **no forma parte de esta PreRelease**.

## Compatibilidad Arduino preservada

Alpha9 no retira del autoload normal:

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

No se rompe la API Arduino ya validada.

## Decisiones heredadas del ciclo 2.1.x

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

## No incluido / pendientes explícitos

Quedan fuera de Alpha9 y deben retomarse de forma explícita:

- UI de Backplane para configurar baudrate RTU;
- UI de Backplane para configurar formato serie (`8N1`, etc.);
- propagación de esa configuración desde proyecto/VPP hacia HAL;
- referencias de miembros de Function Blocks temporizadores en Ladder (`TON0.Q`, `TOF0.Q`, `TP0.Q`) con validación de tipo y autocomplete;
- prueba física simultánea multibit del Remote I/O;
- consolidación reproducible/publicación formal del snapshot local usado para construir el instalador del JWPLC Editor;
- exposición de la HMI Arduino Alpha8 hacia Ladder/OpenPLC.

## Publicación

El workflow automático debe generar y registrar para `v2.1.0-alpha.9`:

- `jwplc-esp32-2.1.0-alpha.9.zip`;
- SHA-256;
- tamaño;
- GitHub PreRelease;
- entrada del índice dev;
- PR automático de índice hacia `main`.

El índice estable `JWPLC/package_jwplc_index.json` debe continuar apuntando al canal estable y no cambiar por esta alpha.

## Estado técnico previo a publicación

```text
ALPHA9_BACKPLANE_FIXED_PROFILE=PASS
ALPHA9_BACKPLANE_8CH_ONE_HOT=PASS
ALPHA9_BACKPLANE_PERSISTENCE=PASS
ALPHA9_BACKPLANE_RECOVERY=PASS
ALPHA9_VPP_SIGNED_PAYLOAD=PASS
ALPHA9_WORKSHOP_ARTIFACT_BUNDLE=PASS
ALPHA9_TECHNICAL_CLOSURE=PASS
ALPHA9_PUBLICATION=PENDING_AUTOMATION
```
