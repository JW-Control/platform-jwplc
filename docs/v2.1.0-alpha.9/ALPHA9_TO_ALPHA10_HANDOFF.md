# Alpha9 -> Alpha10 — Transferencia de trabajo

Fecha: 2026-09-04

## Estado que entrega Alpha9

Alpha9 cierra el Backplane OpenPLC / Remote I/O en el perfil fijo físicamente validado:

```text
Master OpenPLC : 115200 / 8N1
Slave Arduino  : 115200 / 8N1
Slave ID       : 2
```

Ruta funcional cerrada:

```text
Slave DI -> FC02 -> Ladder -> FC15 -> Slave DO -> FC01 -> feedback
```

Marcadores principales:

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
RTU_AUTOMATIC_RECOVERY=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
BACKPLANE_FIXED_RTU_PROFILE_115200_8N1=PASS
SIGNED_FILE_HASHES=9/9 PASS
WORKSHOP_ARTIFACT_BUNDLE=PASS
```

## Artefactos de referencia

### VPP

```text
JWPLC Basic OpenPLC VPP: 2.1.0-alpha.19
Algoritmo: ed25519
Key ID: jwcontrol-2026
SHA-256 del artefacto de taller:
E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
```

### JWPLC Editor — taller

```text
OpenPLC Editor - JWPLC Edition: 4.2.8-jwplc.2
SHA-256:
79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
```

El instalador fue construido desde un snapshot local dirty validado. No asumir que el branch remoto limpio del Editor reproduce exactamente ese instalador hasta consolidar sus cambios.

## Pendiente 1 — configuración RTU del Backplane

El discovery Alpha9 confirmó que el HAL actual contiene:

```cpp
static constexpr uint32_t JWPLC_MODBUS_BAUD = 115200UL;
static constexpr uint32_t JWPLC_MODBUS_CONFIG = SERIAL_8N1;
```

El Backplane persiste `Slave ID` por slot, pero no expone aún configuración propia del bus Master para baudrate/formato.

Arquitectura recomendada:

```text
Backplane / Slot 1 controller
  -> RTU bus baudrate
  -> RTU serial format

Remote slots
  -> Slave ID por nodo
```

No reutilizar `Device > Modbus` para esto: esa pantalla corresponde al servidor Modbus del runtime, no al bus Master del Backplane.

Requisitos de compatibilidad:

```text
default baud   = 115200
default format = 8N1
```

Los proyectos antiguos sin los nuevos campos deben seguir compilando exactamente con el perfil Alpha9.

Marcadores pendientes:

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
```

## Pendiente 2 — referencias a miembros de timers IEC

El Editor rechaza actualmente `TON0.Q` desde la UI como nombre de variable con carácter ilegal.

La corrección futura no debe consistir en permitir `.` indiscriminadamente en identificadores.

Contrato deseado:

```text
Declaración:
  TON0        -> válida si tipo TON
  TON0.Q      -> inválida como nombre declarado

Referencia Ladder:
  TON0.Q      -> válida y BOOL si TON0 : TON
  TOF0.Q      -> válida y BOOL si TOF0 : TOF
  TP0.Q       -> válida y BOOL si TP0 : TP
  TON0.X      -> inválida
  BOOL0.Q     -> inválida
```

También debe existir autocomplete de miembros válidos.

Marcadores:

```text
ALPHA9_TIMER_FB_MEMBER_REFERENCE=PENDING
TON_Q_REFERENCE=REQUIRED
TOF_Q_REFERENCE=REQUIRED
TP_Q_REFERENCE=REQUIRED
FB_MEMBER_TYPE_VALIDATION=REQUIRED
FB_MEMBER_AUTOCOMPLETE=REQUIRED
DECLARATION_NAMES_WITH_DOT=NO
ARBITRARY_DOT_ACCEPTANCE=NO
UNKNOWN_FB_MEMBER=REJECT
```

## Pendiente 3 — source freeze del fork OpenPLC Editor

El build de taller conservó:

```text
Branch: develop/alpha7-openplc-remote-io-rtu
HEAD: 51843ca8beca06a72a57f56c2844cc1013859414
Dirty entries: 19
Worktree fingerprint:
11B95D5291D34D02F422B971E010DA7C5C7166D6244ADC653132AD1EFD83A505
```

Antes de publicar formalmente una build reproducible del Editor:

1. auditar esos 19 cambios;
2. separar archivos reales de backup/locales;
3. ejecutar tests existentes;
4. commit/push incremental;
5. construir desde un worktree limpio;
6. verificar SHA del nuevo instalador;
7. no sustituir el artefacto Alpha9 de taller sin una nueva validación.

## Pendiente 4 — HMI Alpha8 hacia Ladder

La HMI Arduino declarativa de Alpha8 todavía no está expuesta como mecanismo Ladder/OpenPLC.

```text
HMI_TO_LADDER_EXPOSURE=PENDING
```

No mezclar este trabajo con la corrección de RTU bus config o `TON0.Q` sin definir gates separados.

## Limitación física heredada

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

Si se dispone de un banco que permita activar varios DI a la vez, repetir un gate de patrones multibit antes de declarar esa capacidad específicamente validada.

## Rama sugerida

No iniciar Alpha10 desde una feature vieja.

Después del cierre publicado de Alpha9:

```text
git fetch origin --prune
git switch release/v2.1.x
git pull --ff-only origin release/v2.1.x
git switch -c v2.1.0-alpha.10/feature/openplc-config-and-fb-members
```

Antes del primer cambio registrar el SHA exacto de `release/v2.1.x` ya sincronizado/publicado.

## Orden recomendado Alpha10

1. consolidar primero el source del fork OpenPLC Editor que produjo el instalador de taller;
2. implementar configuración RTU del Backplane con fallback `115200 8N1`;
3. validar persistencia -> generación -> HAL -> hardware;
4. corregir resolución tipada de `TON/TOF/TP .Q`;
5. mantener HMI->Ladder como gate independiente;
6. documentar y cerrar sin mezclar pendientes.
