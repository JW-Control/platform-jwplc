# v2.1.0-alpha.7 — Checklist de cierre

Fecha: 2026-09-02

## Objetivo

Cerrar Alpha7 consolidando la robustez de Modbus RTU / Remote I/O, la integración externa OpenPLC/VPP trabajada durante el alpha y las correcciones de runtime detectadas en los gates distribuidos, sin retirar periféricos del autoload normal ni romper las APIs ya validadas.

## Base y alcance

- [x] Branch técnico: `v2.1.0-alpha.7/feature/openplc-backplane-validation`.
- [x] Alpha6 permanece como base funcional publicada.
- [x] `release/v2.1.x` auditado antes del cierre.
- [x] Fix de release `#75` incorporado al branch Alpha7.
- [x] El workflow `release-jwplc-package.yml` de Alpha7 y `release/v2.1.x` quedó byte-idéntico (`blob 898c5e4e2c08a7fb725a1c58d6c30b08f2dc3982`).
- [x] La diferencia histórica de un commit en `rev-list` se documenta como ancestry distinta por cherry-pick, no como contenido pendiente.
- [x] No se realizó rebase tardío ni force-push durante el cierre.

## Modbus RTU

- [x] Parser multidrop corregido para separar ADU por estructura Modbus antes de validar CRC.
- [x] Gate físico M2 + S1 + S2 cerrado con CRC=0 y exceptions=0 en ambos Slaves.
- [x] Master cooperativo/no bloqueante establecido como API recomendada.
- [x] API Sync explícita preservada para commissioning, pruebas y compatibilidad.
- [x] FC01, FC02, FC05 y FC15 validados en Remote I/O.
- [x] Pérdida de bus detectada mediante timeout sin congelar el Master.
- [x] Fail-safe de salidas del Slave validado ante pérdida de bus.
- [x] Reconexión validada sin resetear el Master.
- [x] Reset del Slave validado con recuperación posterior sin resetear el Master.
- [x] Ejemplos y README de `JWPLC_ModbusRTU` actualizados.

Evidencia A7.1:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
FC01_READ_COILS=PASS
FC02_READ_DISCRETE_INPUTS=PASS
FC05_WRITE_SINGLE_COIL=PASS
FC15_WRITE_MULTIPLE_COILS=PASS
MASTER_COOPERATIVE=PASS
DO_REFRESH_40MS=PASS
BUS_DISCONNECT_DETECTED=PASS
FAILSAFE_ON_BUS_LOSS=PASS
MASTER_TIMEOUT_NO_FREEZE=PASS
RECONNECT_WITHOUT_MASTER_RESET=PASS
SLAVE_RESET_DETECTED=PASS
MASTER_SURVIVES_SLAVE_RESET=PASS
RECOVERY_WITHOUT_MASTER_RESET=PASS
```

## TCA / I2C

- [x] Corregido el tratamiento de lectura TCA `0xFF` para no confundir dato válido con error.
- [x] Corrección integrada al core JWPLC Basic.
- [x] `precompiled/core/JWPLCBASIC/core.a` regenerado y versionado durante Alpha7.
- [x] Diagnóstico I2C seguro utilizado en el soak sin trasladar políticas de prueba innecesarias al runtime normal.

## Ethernet / W5500

- [x] Identificada la causa del falso `LINK_OFF`: timeout temporal al adquirir el mutex SPI era colapsado por `linkUp()` como si fuese cable desconectado.
- [x] `JWPLC_Ethernet.service()` diferencia contención SPI de `LinkOFF` físico real.
- [x] Sólo `LinkOFF` real lleva el runtime a `JWPLC_ETH_STATE_LINK_OFF`.
- [x] `finishNetworkConfiguration()` toma IP + LINK dentro de una sola ventana SPI.
- [x] `statusString()` diferencia `Link OFF`, `SPI lock timeout` y `Link unknown`.
- [x] Source compilado verificado contra el package dev enlazado por Junction al repositorio.
- [x] Recovery Router -> laptop sin DHCP -> Router aprobado sin RESET.
- [x] Gate dirigido de contención SPI aprobado.

Recovery:

```text
ETH_RECOVERY_ROUTER_INITIAL=PASS
ETH_RECOVERY_ROUTER_TO_OFF=PASS
ETH_RECOVERY_LAPTOP_NO_DHCP=PASS
ETH_RECOVERY_LAPTOP_TO_OFF=PASS
ETH_RECOVERY_ROUTER_FINAL=PASS
ETH_RECOVERY_NO_RESET=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
```

Contención dirigida:

```text
HOLD_CYCLES_REQUESTED=12
HOLD_CYCLES_OBSERVED=12
HOLDER_ACQUIRE_FAILS=0
FORCED_BUS_LOCK_TIMEOUTS=12
FALSE_LINK_OFFS=0
READY_DROPS=0
WRONG_STATE_ON_TIMEOUT=0
FINAL_READY=YES
FINAL_STATE=5
FINAL_ERROR=0
FINAL_LINK=ON
ALPHA7_ETH_SPI_CONTENTION=PASS
```

Conclusión:

```text
SPI temporalmente ocupado -> READY se conserva
Cable físicamente desconectado -> LINK_OFF real
Cable reconectado -> READY recuperado sin RESET
```

## OpenPLC / VPP

- [x] VPP Alpha18 preservado y publicado dentro del flujo OpenPLC de Alpha7.
- [x] Payload firmado declarado reproducible desde checkout fresco: 9/9.
- [x] Política EOL del payload Alpha18 preservada mediante `.gitattributes`.
- [x] Feedback FC01 integrado en la HAL/VPP después del cierre de preservación Alpha18.
- [x] OpenPLC/Backplane utiliza el Master Modbus cooperativo.
- [x] Los gates del VPP no convierten OpenPLC en dependencia obligatoria del runtime Arduino.
- [x] No se redefine en Alpha7 el proceso canónico futuro de firma Ed25519.

Marcadores de preservación VPP:

```text
ALPHA18_VPP_STATUS=CLOSED
SIGNED_PAYLOAD_PHYSICAL=9/9
FRESH_CHECKOUT_SIGNED_PAYLOAD=9/9
ALPHA18_SIGNED_BYTES_PRESERVED=PASS
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
```

## Soak y diagnóstico distribuido

- [x] Soak distribuido Alpha7 incorporado como herramienta de validación.
- [x] Retry REV6 consolidado con una sola repetición antes del primer TRIGGER ambiguo.
- [x] No se repite una secuencia una vez que el TRIGGER pudo haber llegado al Slave.
- [x] Diagnóstico I2C y métricas de recuperación incorporados al sketch de prueba.
- [x] Las políticas específicas del soak se mantienen separadas del contrato del package salvo fixes de causa raíz demostrados.

## Precompilación

### Core JWPLC Basic

- [x] `precompiled/core/JWPLCBASIC/core.a` actualizado después del fix TCA.
- [x] El core precompilado continúa formando parte del perfil JWPLC Basic.

### JWPLC_ModbusRTU

- [x] Desarrollo funcional congelado antes de regenerar el archive.
- [x] `precompiled=full` restaurado.
- [x] `libJWPLC_ModbusRTU.a` regenerado desde objetos reales de Arduino CLI.
- [x] Verificación limpia con el archive aprobada.
- [x] Auditoría estricta `nm` aprobada.
- [x] Compatibilidad JWPLC Basic Core validada.
- [x] Cero TUs source de `JWPLC_ModbusRTU.cpp` en la verificación Basic Core.

Archive final:

```text
Archivo : JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
Bytes   : 231062
SHA256  : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

Marcadores:

```text
ALPHA7_MODBUS_PRECOMPILED=PASS
ALPHA7_MODBUS_BASICCORE_PRECOMPILED=PASS
ALPHA7_MODBUS_PRECOMPILED_FREEZE=PASS
```

## Build y compatibilidad

- [x] Arduino CLI 1.5.1 utilizado en los gates finales.
- [x] JWPLC Basic compila con el fix Ethernet desde source.
- [x] JWPLC Basic Core compila utilizando el archive Modbus RTU final.
- [x] Ethernet continúa source-build, decisión ya aceptada desde Alpha6.
- [x] Modbus RTU vuelve a `precompiled=full` al cierre Alpha7.
- [x] No se retiró ningún periférico del autoload por velocidad.
- [x] Los gates P1 usados para regenerar archives se interpretan como verificación de precompilación, no como benchmark comparable de tiempos.
- [x] Alpha7 no reclama una nueva mejora global de cold build frente a Alpha6 sin un benchmark dedicado adicional.

## Autoload preservado

- [x] Display.
- [x] Ethernet / W5500.
- [x] microSD.
- [x] FRAM.
- [x] RTC.
- [x] botonera.
- [x] RS-485.
- [x] Modbus RTU.
- [x] TCA / I/O.
- [x] mutex SPI global.

## Decisiones que Alpha7 no cambia

- [x] App-only continúa como herramienta de desarrollo; no es el upload default.
- [x] Bootloader precompilado definitivo no adoptado.
- [x] `bootloader.bin` no se publica como configuración definitiva.
- [x] FlashFreq/configuración universal final continúa pendiente explícito.
- [x] OTA continúa fuera de alcance.
- [x] OpenPLC no se integra como runtime obligatorio del package Arduino.
- [x] No se migra hardware a ESP32-S3 dentro de Alpha7.

## Documentación de cierre

- [x] `MODBUS_RTU_MASTER_COOPERATIVO.md`.
- [x] `REMOTE_IO_A7_1_VALIDATION.md`.
- [x] `OPENPLC_BACKPLANE_ALPHA18_VPP.md`.
- [x] `ALPHA7_CLOSURE_CHECKLIST.md`.
- [x] `PULL_REQUEST.md`.
- [x] `PRE_RELEASE.md`.
- [x] README raíz actualizado para Alpha7.

## Publicación

- [ ] PR Alpha7 abierto contra `release/v2.1.x`.
- [ ] CI del PR aprobado.
- [ ] PR mergeado.
- [ ] Workflow detecta `JWPLC_RELEASE_VERSION: 2.1.0-alpha.7`.
- [ ] ZIP `jwplc-esp32-2.1.0-alpha.7.zip` generado.
- [ ] GitHub PreRelease `v2.1.0-alpha.7` publicada.
- [ ] Índice dev actualizado.
- [ ] Instalación aislada desde índice dev validada.
- [ ] Compilación aislada del package publicado validada.
- [ ] Upload/arranque físico post-publicación validado.

## Gate técnico de cierre

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
ALPHA7_ETH_SPI_CONTENTION=PASS
ETH_ROUTER_LAPTOP_ROUTER_RECOVERY=PASS
ALPHA7_MODBUS_PRECOMPILED_FREEZE=PASS
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
ALPHA7_TECHNICAL_CLOSURE=PASS
ALPHA7_PUBLICATION=PENDING_PR_CI_RELEASE
```

Estado:

```text
ALPHA7_STATUS=TECHNICALLY_CLOSED
```
