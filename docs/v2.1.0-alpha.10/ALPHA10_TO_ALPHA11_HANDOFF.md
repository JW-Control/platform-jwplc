# Handoff Alpha10 -> Alpha11

## Estado de Alpha10

Alpha10 queda técnicamente cerrado como hotfix de compatibilidad Arduino para aislar `JWPLC_Ethernet` frente a copias homónimas antiguas en el sketchbook del usuario.

```text
ALPHA10_ROOT_CAUSE=CONFIRMED
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
ALPHA10_ETHERNET_SHADOWING_FIX=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_SCOPED_FIX
ALPHA10_FINAL_COMPILE_MATRIX=5/5_PASS
ALPHA10_TECHNICAL_CLOSURE=PASS
```

Commit técnico:

```text
c0e5c621cec71977b86becfc8d7acb26ca21e906
```

## Decisión de library discovery

Se probó proteger 1, 4 y 7 librerías mediante markers bundled.

```text
M0_NONE              22.094 s
M1_ETH               23.327 s  (+5.6%)
M4_OBSERVED_STALE    26.888 s  (+21.7%)
M7_ALL               30.353 s  (+37.4%)
```

Alpha10 adopta `M1_ETH`.

No extender el mismo patrón a más librerías sin volver a medir el coste warm. Si Alpha11 desea generalizar el aislamiento, debe investigar una estrategia de menor coste de discovery.

## OpenPLC / Backplane

Alpha10 no cambia el cierre heredado de Alpha9:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

OpenPLC continúa externo/opcional al runtime Arduino.

Pendientes para Alpha11:

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
```

No asumir esos parámetros ya configurables por UI.

## Function Blocks / timers

Pendientes transferidos:

```text
TON_Q_REFERENCE=REQUIRED
TOF_Q_REFERENCE=REQUIRED
TP_Q_REFERENCE=REQUIRED
FB_MEMBER_TYPE_VALIDATION=REQUIRED
FB_MEMBER_AUTOCOMPLETE=REQUIRED
DECLARATION_NAMES_WITH_DOT=NO
ARBITRARY_DOT_ACCEPTANCE=NO
UNKNOWN_FB_MEMBER=REJECT
```

Objetivo: soportar referencias válidas como `TON0.Q`, `TOF0.Q` y `TP0.Q` mediante resolución tipada, sin permitir identificadores arbitrarios con punto.

## Fork OpenPLC Editor

Pendiente:

```text
OPENPLC_EDITOR_SOURCE_FREEZE=PENDING
```

No resetear, cambiar de rama ni limpiar a ciegas el worktree local del fork si conserva cambios no consolidados.

## HMI hacia Ladder/OpenPLC

La HMI Arduino existente continúa sin exposición directa a Ladder/OpenPLC.

```text
HMI_TO_OPENPLC_LADDER=PENDING
```

## Remote I/O

La validación one-hot 8/8 de Alpha9 permanece válida.

Pendiente explícito:

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
```

No convertir el resultado one-hot en afirmación de prueba simultánea multibit.

## Decisiones de build/release heredadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No publicar `bootloader.bin` como definitivo hasta fijar la configuración final.

## Prioridad propuesta Alpha11

1. Cerrar publicación de Alpha10 si quedara pendiente.
2. Mantener estabilidad y compatibilidad Arduino IDE.
3. Retomar configuración RTU del Backplane.
4. Resolver miembros tipados de FB/timers.
5. Consolidar source freeze del fork OpenPLC Editor.
6. Evaluar HMI hacia Ladder/OpenPLC.
7. Recién después, si hace falta, investigar aislamiento general de librerías sin la penalización M7.

## Marcador de transferencia

```text
ALPHA10_TO_ALPHA11_HANDOFF=READY
NEXT=ALPHA11
```
