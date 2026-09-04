# v2.1.0-alpha.9 — Checklist de cierre

Fecha: 2026-09-04

## Objetivo

Cerrar Alpha9 con el Backplane OpenPLC / Remote I/O físicamente validado en el perfil fijo `115200 8N1`, manteniendo el Arduino package compatible y dejando explícitos los pendientes que no pertenecen a este cierre.

## Base y Git

- [x] Rama final creada desde `release/v2.1.x` actual.
- [x] Base final: `d5e2d360731e9bae5a2db0f7ee30213986c050cf`.
- [x] No se hace merge ciego del branch histórico divergente Alpha9.
- [x] README canónico del Slave RTU trasladado a la rama final.
- [x] Documentación pública en español.
- [ ] CI del PR técnico aprobado.
- [ ] Merge técnico a `release/v2.1.x`.

## Slave Arduino

- [x] Sketch `JWPLC_RemoteIO_Slave_RTU` cargado físicamente.
- [x] COM14.
- [x] Slave ID `2`.
- [x] `115200 8N1`.
- [x] `MODBUS_READY=YES`.
- [x] README canónico incluido.

```text
ALPHA9_SLAVE_PROGRAMMING_GATE=PASS
```

## Master OpenPLC

- [x] Proyecto `A7_2_2_REMOTE_IO_8CH_LOOPBACK` utilizado.
- [x] Master en COM4.
- [x] Compilación OpenPLC correcta.
- [x] Upload físico correcto.
- [x] Scan aproximado de 20 ms observado.
- [x] FC02/FC15/FC01 activos.

```text
ALPHA9_MASTER_BUILD_UPLOAD=PASS
```

## Backplane 8 canales

- [x] `ALL_OFF_INITIAL`.
- [x] `I0_0 -> Q0_0`.
- [x] `I0_1 -> Q0_1`.
- [x] `I0_2 -> Q0_2`.
- [x] `I0_3 -> Q0_3`.
- [x] `I0_4 -> Q0_4`.
- [x] `I0_5 -> Q0_5`.
- [x] `I0_6 -> Q0_6`.
- [x] `I0_7 -> Q0_7`.
- [x] `ALL_OFF_FINAL`.
- [x] FC02 = 8/8.
- [x] Ladder mapping = 8/8.
- [x] FC15 = 8/8.
- [x] FC01 feedback = 8/8.
- [x] posiciones de bit = 8/8.
- [x] crossed bits = 0.
- [x] correlación física = 8/8.
- [x] nuevos fallos RTU = 0 durante el gate estable.
- [x] nuevos mismatches = 0.

```text
BACKPLANE_8CH_ONE_HOT_GATE=PASS
```

### Limitación

- [ ] patrón multibit simultáneo físicamente validado.

```text
MULTIBIT_SIMULTANEOUS=NOT_TESTED
REASON=PHYSICAL_TEST_RIG_ONE_INPUT_AT_A_TIME
```

No es bloqueante para el alcance one-hot declarado de Alpha9; queda explícitamente documentado.

## Persistencia

- [x] configuración de proyecto localizada.
- [x] configuración de device localizada.
- [x] hashes antes registrados.
- [x] cerrar/reabrir proyecto.
- [x] hashes sin cambios.
- [x] recompilación posterior.
- [x] upload posterior.

```text
BACKPLANE_PERSISTENCE=PASS
BACKPLANE_RECOMPILE=PASS
```

## Recuperación

- [x] baseline estable previo.
- [x] power-cycle de Slave con Master encendido.
- [x] fallo FC02 detectado durante ausencia.
- [x] fallo FC15 detectado durante ausencia.
- [x] fallo FC01 esperado durante ausencia.
- [x] recuperación automática tras volver el Slave.
- [x] prueba post-recovery `I0_0 -> Q0_0`.
- [x] feedback FC01 post-recovery.
- [x] retorno válido a `0x00`.

```text
RTU_AUTOMATIC_RECOVERY=PASS
POST_RECOVERY_PHYSICAL_IO=PASS
```

## Perfil RTU

- [x] perfil Master validado `115200 8N1`.
- [x] perfil Slave validado `115200 8N1`.
- [x] Slave ID 2 propagado/operativo.
- [x] HAL actual confirmado con `JWPLC_MODBUS_BAUD = 115200UL`.
- [x] HAL actual confirmado con `JWPLC_MODBUS_CONFIG = SERIAL_8N1`.
- [ ] UI Backplane para baudrate.
- [ ] UI Backplane para formato serie.
- [ ] propagación configurable de baudrate/formato hacia HAL.

```text
BACKPLANE_FIXED_RTU_PROFILE_115200_8N1=PASS
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
```

Los tres pendientes configurables se transfieren al siguiente ciclo y no se presentan como implementados en Alpha9.

## VPP

- [x] Package ID `com.jwcontrol.jwplc-basic`.
- [x] VPP versión `2.1.0-alpha.19`.
- [x] firma `ed25519`.
- [x] key ID `jwcontrol-2026`.
- [x] 9/9 hashes en disco.
- [x] 9/9 hashes dentro del `.vpp` final.
- [x] manifest embebido correcto.
- [x] metadata de firma embebida correcta.
- [x] estructura ZIP normalizada correcta.

```text
VPP_BYTES=1260931
VPP_SHA256=E881FA81B9D6C5D7DF19086599BBEAF287DC8D2291942AA899F9021C1F4B0582
WORKSHOP_SIGNED_VPP=PASS
```

## JWPLC Editor — taller

- [x] `OpenPLC Editor - JWPLC Edition`.
- [x] versión `4.2.8-jwplc.2`.
- [x] Node `22.23.1`.
- [x] npm `10.9.8`.
- [x] build main = 0.
- [x] build renderer = 0.
- [x] electron-builder = PASS.
- [x] NSIS x64 = PASS.
- [x] source snapshot preservado durante build.
- [x] hash final registrado.
- [x] instalación probada.
- [x] VPP de `JWPLC_TALLER` removido/reinstalado y cargado correctamente.

```text
INSTALLER_BYTES=133699500
INSTALLER_SHA256=79C22B2C1816170997CA8A8F5D952DA0BE2098EA2941C5D7E58D0800716C47C7
WORKSHOP_EDITOR_INSTALLER=PASS
WORKSHOP_ARTIFACT_BUNDLE=PASS
```

- [ ] Authenticode del instalador.

```text
AUTHENTICODE_STATUS=NotSigned
```

No es la firma Ed25519 del VPP y no invalida el artefacto interno de taller.

- [ ] snapshot del Editor reproducible únicamente desde branch remoto limpio.

```text
OPENPLC_EDITOR_REMOTE_SOURCE_CONSOLIDATION=PENDING
```

## Arduino package / autoload

- [x] Display preservado.
- [x] Ethernet/W5500 preservado.
- [x] microSD preservada.
- [x] FRAM preservada.
- [x] RTC preservado.
- [x] botonera preservada.
- [x] RS-485 preservado.
- [x] Modbus RTU preservado.
- [x] TCA/I/O preservado.
- [x] mutex SPI preservado.
- [x] OpenPLC continúa externo/opcional al runtime Arduino.

## Decisiones heredadas de packaging

- [x] App-only no se adopta como upload default.
- [x] bootloader precompilado no adoptado.
- [x] bootloader generado por SDK/ELF.
- [x] perfil flash actual se conserva como perfil validado.
- [x] configuración universal final continúa pendiente.
- [x] OTA no definida.
- [x] no publicar `bootloader.bin` definitivo.

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

## Ladder / timers

- [ ] `TON0.Q` aceptado por el editor con resolución tipada.
- [ ] `TOF0.Q` aceptado.
- [ ] `TP0.Q` aceptado.
- [ ] unknown member rechazado.
- [ ] scalar `.Q` rechazado.
- [ ] autocomplete de miembros FB.

```text
ALPHA9_TIMER_FB_MEMBER_REFERENCE=PENDING
```

Se transfiere al siguiente ciclo; no se habilita `.` indiscriminadamente en nombres de variables.

## HMI hacia Ladder

- [ ] exposición de HMI Arduino Alpha8 hacia OpenPLC/Ladder.

```text
HMI_TO_LADDER_EXPOSURE=PENDING
```

## Documentación

- [x] `ALPHA9_TECHNICAL_CLOSURE.md`.
- [x] `ALPHA9_CLOSURE_CHECKLIST.md`.
- [x] `PRE_RELEASE.md`.
- [x] README del Remote I/O Slave.
- [ ] README raíz Alpha9.
- [ ] `PULL_REQUEST.md`.
- [ ] cierre post-publicación con ZIP/SHA/size.

## Publicación

- [ ] PR técnico Alpha9 hacia `release/v2.1.x`.
- [ ] CI verde.
- [ ] merge técnico.
- [ ] marcador `JWPLC_RELEASE_VERSION: 2.1.0-alpha.9`.
- [ ] workflow automático.
- [ ] ZIP `jwplc-esp32-2.1.0-alpha.9.zip`.
- [ ] SHA-256 del ZIP.
- [ ] tamaño del ZIP.
- [ ] GitHub PreRelease.
- [ ] índice dev actualizado.
- [ ] índice estable sin cambios.
- [ ] PR automático de índices hacia `main`.
- [ ] sincronización `release/v2.1.x -> main`.
- [ ] cierre documental post-publicación.

## Estado técnico previo al PR

```text
ALPHA9_TECHNICAL_CLOSURE=PASS
ALPHA9_STATUS=TECHNICALLY_CLOSED
ALPHA9_PUBLICATION=PENDING_PR_CI_RELEASE
```
