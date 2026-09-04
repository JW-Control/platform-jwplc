# Pull Request — JWPLC v2.1.0-alpha.9

## Título sugerido

`feat(alpha9): cerrar OpenPLC Backplane y Remote I/O RTU`

## Ramas

```text
Base: release/v2.1.x
Head: v2.1.0-alpha.9/integration/final-closure
```

Base final usada:

```text
d5e2d360731e9bae5a2db0f7ee30213986c050cf
```

## Resumen

Alpha9 cierra el uso del JWPLC Basic como **Master OpenPLC con Remote I/O Modbus RTU** en el perfil fijo físicamente validado `115200 8N1`, formaliza el Slave Arduino de referencia y documenta los artefactos de taller utilizados para la validación.

La integración OpenPLC sigue siendo externa/opcional al runtime Arduino. No se retiran periféricos del autoload normal ni se rompen las APIs Arduino ya probadas.

## Motivo de la rama final

La rama histórica Alpha9 fue creada antes de una sincronización de metadata de Alpha8 en `release/v2.1.x`, por lo que terminó 1 commit ahead / 1 behind respecto al release actual.

Para evitar una integración de historia divergente, el cierre se rehace sobre el HEAD actual de `release/v2.1.x` y conserva del branch histórico únicamente el cambio realmente nuevo del Arduino package: el README canónico del ejemplo `JWPLC_RemoteIO_Slave_RTU`.

```text
ALPHA9_FINAL_BASE_CORRECT=PASS
```

## Backplane / Remote I/O

Topología validada:

```text
JWPLC Master / OpenPLC / COM4 / 115200 8N1
            |
            | RS-485
            v
JWPLC Slave / Arduino / COM14 / ID 2 / 115200 8N1
```

Recorrido:

```text
Slave DI -> FC02 -> OpenPLC Ladder -> FC15 -> Slave DO -> FC01 -> feedback
```

Gate físico:

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

El banco permitió activar sólo una entrada a la vez:

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

## Persistencia y recuperación

Se cerró/reabrió el proyecto, se verificó persistencia de configuración y se recompiló/subió nuevamente al Master.

Luego se realizó power-cycle del Slave manteniendo el Master activo. Se detectaron los fallos RTU esperados durante la ausencia y la recuperación fue automática al volver el nodo.

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

## Perfil RTU Alpha9

Alpha9 valida explícitamente:

```text
Master: 115200 / 8N1
Slave : 115200 / 8N1
ID    : 2
```

El HAL actual mantiene `JWPLC_MODBUS_BAUD = 115200UL` y `JWPLC_MODBUS_CONFIG = SERIAL_8N1`.

La configuración de baudrate/formato desde la UI del Backplane queda pendiente y no se presenta como implementada.

## VPP OpenPLC

El VPP conservado en `platform-jwplc` usa versionado independiente:

```text
Package ID : com.jwcontrol.jwplc-basic
VPP version: 2.1.0-alpha.19
Signature  : ed25519
Key ID     : jwcontrol-2026
Payload    : 9/9 PASS
```

Artefacto validado para taller:

```text
JWPLC-Basic-OpenPLC-2.1.0-alpha.19.jwcontrol-signed.vpp
Bytes   : 1260931
SHA-256 : E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
```

No se cambia la versión del VPP sólo para igualarla al número del Arduino package.

## JWPLC Editor — taller

Artefacto probado:

```text
OpenPLC Editor - JWPLC Edition 4.2.8-jwplc.2
NSIS x64
Bytes   : 133699500
SHA-256 : 79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
```

La instalación fue verificada y el VPP de la carpeta de taller fue removido/reinstalado correctamente.

El instalador se construyó desde un snapshot local validado del fork OpenPLC Editor. La consolidación reproducible de ese snapshot en el repositorio `openplc-editor` queda fuera de este PR del Arduino package.

## Autoload preservado

Se mantienen:

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

No se publica `bootloader.bin` como definitivo.

## No incluido / pendientes futuros

- configuración de baudrate RTU desde Backplane;
- configuración de serial format desde Backplane;
- propagación de esos parámetros hacia HAL;
- prueba física multibit simultánea;
- referencias tipadas `TON0.Q`, `TOF0.Q`, `TP0.Q` y autocomplete;
- exposición de la HMI Arduino Alpha8 hacia Ladder/OpenPLC;
- consolidación/publicación reproducible del snapshot del JWPLC Editor usado para el taller.

## Documentación

- `docs/v2.1.0-alpha.9/ALPHA9_TECHNICAL_CLOSURE.md`;
- `docs/v2.1.0-alpha.9/ALPHA9_CLOSURE_CHECKLIST.md`;
- `docs/v2.1.0-alpha.9/PULL_REQUEST.md`;
- `docs/v2.1.0-alpha.9/PRE_RELEASE.md`;
- README canónico del ejemplo `JWPLC_RemoteIO_Slave_RTU`;
- README raíz actualizado al marcador de Alpha9.

## Checklist para merge

- [x] Base final correcta.
- [x] Master OpenPLC compila y sube.
- [x] Slave Arduino compila y sube.
- [x] FC02 8/8.
- [x] Ladder mapping 8/8.
- [x] FC15 8/8.
- [x] FC01 feedback 8/8.
- [x] correlación física 8/8.
- [x] persistencia.
- [x] recompilación.
- [x] recuperación tras power-cycle de Slave.
- [x] VPP firmado 9/9.
- [x] artefactos de taller verificados por SHA-256.
- [x] autoload Arduino preservado.
- [x] pendientes/no incluido documentados.
- [x] PreRelease en español.
- [x] checklist actualizado.
- [ ] CI del PR aprobado.

## Después del merge

El README contiene:

```text
JWPLC_RELEASE_VERSION: 2.1.0-alpha.9
```

y existe:

```text
docs/v2.1.0-alpha.9/PRE_RELEASE.md
```

Por lo tanto el workflow automático debe:

1. generar `jwplc-esp32-2.1.0-alpha.9.zip`;
2. calcular SHA-256 y tamaño;
3. crear la GitHub PreRelease `v2.1.0-alpha.9`;
4. actualizar el índice dev;
5. abrir el PR automático de índices hacia `main`.

Después se debe sincronizar `release/v2.1.x -> main` y registrar el cierre post-publicación.
