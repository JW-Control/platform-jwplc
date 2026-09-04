# v2.1.0-alpha.8 — Checklist de cierre

Fecha: 2026-09-04

## Objetivo

Cerrar Alpha8 consolidando la interacción TFT/botonera y una HMI Arduino declarativa eficiente, sin retirar periféricos del autoload normal, sin romper APIs ya validadas y sin adelantar la integración OpenPLC prevista para Alpha9.

## Base y alcance

- [x] Branch técnico: `v2.1.0-alpha.8/fix/buttons-display-autowake`.
- [x] Alpha7 cerrado/publicado tomado como base funcional.
- [x] Alpha8 = TFT + botonera + HMI Arduino eficiente.
- [x] Alpha9 = exposición de esa HMI hacia OpenPLC/Ladder.
- [x] OpenPLC no se integra al runtime Arduino dentro de Alpha8.
- [x] No se elimina ningún periférico del autoload normal.

## Display / autowake

- [x] Transición USER no solicitada reproducida durante diagnóstico.
- [x] `IDLE_WAKE_DISABLED` es el default Alpha8.
- [x] Wake automático sigue disponible por API explícita.
- [x] Navegación Display usa flancos físicos propios.
- [x] Display no consume latches `pressed()` / `released()` de aplicación.
- [x] Entrada USER absorbe correctamente el estado físico del botón que originó la transición.
- [x] Retorno IDLE evita rebote lógico de navegación.
- [x] `JWPLC_Display.clearPendingInput()` preservado.

```text
ALPHA8_GATE_B_AUTOWAKE_REPRODUCED=PASS
ALPHA8_DISPLAY_FALSE_USER_TRANSITION=CONFIRMED
ALPHA8_IDLE_WAKE_DISABLED_DEFAULT=PASS
ALPHA8_DISPLAY_DOES_NOT_STEAL_APP_LATCH=PASS
```

## Botonera

- [x] LEFT.
- [x] UP.
- [x] RIGHT.
- [x] ESC.
- [x] OK.
- [x] DOWN.
- [x] Sin ghost sostenido en gate dirigido.
- [x] `pressed()` / `released()` disponibles al sketch.
- [x] `anyPressedOrRepeated()` trata sólo `PRESS/REPEAT` como actividad válida.
- [x] `RELEASE` no se interpreta como press/repeat.
- [x] Router de refresh basado en cambio de máscara física.
- [x] Pulsación sostenida no congela runtime/TFT.

## HMI declarativa

- [x] `JWPLC_UIField`.
- [x] Máximo interno de 32 campos.
- [x] VALUE.
- [x] TEXT.
- [x] BOOL.
- [x] BAR.
- [x] Múltiples páginas USER.
- [x] Formato numérico.
- [x] signed/unsigned.
- [x] leading zeros.
- [x] overflow visible.
- [x] layout inline/stacked.
- [x] alineación LEFT/CENTER/RIGHT.
- [x] colores.
- [x] bool text.
- [x] rango de barra.
- [x] helpers `JWPLC_UIValueField`, `JWPLC_UITextField`, `JWPLC_UIBoolField`, `JWPLC_UIBarField`.
- [x] `JWPLC_Display.setFields()`.
- [x] `setValue()`.
- [x] `setText()`.
- [x] `setBool()`.
- [x] `setBar()`.
- [x] `setUserPage()`.
- [x] refresh on-demand/periodic.
- [x] invalidación field/all.
- [x] sin `new` para runtime HMI.
- [x] sin `String` dinámico para buffers internos de valores.

## Dirty refresh / SPI

- [x] Campo sólo dirty si cambia el resultado formateado.
- [x] Página no visible cachea el último valor sin forzar TFT.
- [x] Entrar a USER/página dibuja el último valor cacheado.
- [x] Redibujado normal limitado a región dinámica.
- [x] `fillScreen()` reservado para transiciones/redraw completo.
- [x] Una ventana de adquisición TFT para dirty fields de una pasada.
- [x] Gate físico sin flicker problemático observado.

## Vistas cacheadas

- [x] `JWPLC_IO.inputs()`.
- [x] `JWPLC_IO.outputs()`.
- [x] `JWPLC_IO.input(index)`.
- [x] `JWPLC_IO.output(index)`.
- [x] `JWPLC_IO.ready()`.
- [x] `JWPLC_IO.lastScanMs()`.
- [x] `JWPLC_Time.present()`.
- [x] `JWPLC_Time.valid()`.
- [x] `JWPLC_Time.lostPower()`.
- [x] hora/fecha/dayOfWeek.
- [x] `JWPLC_Time.lastUpdateMs()`.
- [x] Sin nuevas transacciones físicas de I/O/RTC.
- [x] `JWPLC_IO.outputs()` documentado como Q0.0..Q0.7 en JWPLC Basic.

## Build speed

- [x] Baseline corregido Alpha6 conservado.
- [x] Regresión inicial de un TU RuntimeView identificada.
- [x] RuntimeView reintegrado a `JWPLC_GlobalPeripherals.cpp`.
- [x] Basic cold = 15 compilaciones.
- [x] Core cold = 78 compilaciones.
- [x] Warm = 1 compilación.
- [x] Lazy-link HMI implementado.
- [x] `01_empty` no extrae motor HMI.
- [x] HMI gate sí extrae motor HMI.
- [x] APP vacía reducida 3456 bytes frente a Alpha8 pre-lazy.
- [x] Variación wall-clock del host documentada.
- [x] No se reclama mejora global de segundos frente a Alpha6.
- [x] Performance freeze aprobado.

```text
ALPHA8_COLD_TU_REGRESSION=RESOLVED
ALPHA8_COMPILER_COUNT_PARITY=PASS
ALPHA8_LAZYLINK_SOURCE_COMPILE=PASS
ALPHA8_LAZYLINK_PRECOMPILED=PASS
ALPHA8_EMPTY_HMI_ENGINE_LINKED=NO
ALPHA8_HMI_GATE_ENGINE_LINKED=YES
ALPHA8_EMPTY_APP_REDUCTION_BYTES=3456
ALPHA8_PERFORMANCE_FREEZE=PASS
```

## Precompilación

### JW_MatrixButtons

- [x] Fuente 1.0.5 no modificada por Alpha8.
- [x] `precompiled=full` vigente.
- [x] Archive existente no requiere regeneración por este alcance.

```text
Bytes  : 129506
SHA256 : 55BE8D7791DDAD79D613DBB199C10A504DE0F20CDF3330B6679A35DD64E25C81
```

### JWPLC_Display

- [x] Source HMI congelado.
- [x] `precompiled=full` preservado.
- [x] Builder P1 ejecutado para `JWPLC_Display`.
- [x] Source compile aprobado.
- [x] Verify precompiled aprobado.
- [x] 4 objetos archivados.
- [x] Gate de link precompiled aprobado.
- [x] Archive usado durante validación física.
- [x] Basic final: 0 TUs Display source.
- [x] Basic Core final: 0 TUs Display source.
- [x] HMI final: 0 TUs Display source.
- [x] Archive final versionado.
- [x] Hash final validado antes del commit/push.

```text
Archivo : JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
Bytes   : 642576
SHA256  : D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
Commit  : 08073e5a0244d9461d3889f02d411b68f727d638
```

```text
ALPHA8_BASIC_DISPLAY_PRECOMPILED=PASS
ALPHA8_CORE_DISPLAY_PRECOMPILED=PASS
ALPHA8_HMI_DISPLAY_PRECOMPILED=PASS
ALPHA8_FINAL_SOURCE_FREEZE=PASS
ALPHA8_FINAL_PRECOMPILED_GATE=PASS
ALPHA8_DISPLAY_ARCHIVE_COMMIT=PASS
```

## Hardware Alpha8

- [x] IDLE soak mínimo 180 s.
- [x] `unexpectedUser=0`.
- [x] RTC avanzando.
- [x] entrada USER explícita con OK.
- [x] navegación LEFT/RIGHT.
- [x] barra UP/DOWN.
- [x] ESC retorna IDLE.
- [x] ESC también llega al sketch.
- [x] botones en IDLE no despiertan USER con wake disabled.
- [x] reentrada USER.
- [x] pulsación sostenida sin freeze.
- [x] TFT observada sin congelamiento/flicker problemático.

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

## Incidente histórico de taller

- [x] Registrado como incidente histórico.
- [x] No se afirma una causa única no demostrada para todos los equipos.
- [x] Alpha8 actual se considera estable operacionalmente.
- [x] En futuras reproducciones se preservará IDE/package/FQBN/Used library/SHA binario antes de reinstalar.

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

## Ejemplos numerados para taller

Los ejemplos dependientes del JWPLC Basic se colocan en librerías `JWPLC_*`; los drivers genéricos `JW_*` permanecen reutilizables y sin ejemplos específicos de esta placa.

### `JWPLC_GlobalPeripherals`

- [x] `01.DigitalIO_Basic`.
- [x] `02.Buttons_Basic`.
- [x] `03.RTC_Basic`.
- [x] `04.FRAM_Basic`.
- [x] `05.microSD_Basic`.
- [x] `06.Runtime_Cache_Status`.

### `JWPLC_Display`

- [x] `01.Display_IDLE_Status`.
- [x] `02.Display_HMI_Fields`.
- [x] `03.Display_HMI_Pages`.
- [x] `04.Display_TFT_Direct`.

### `JWPLC_Ethernet`

- [x] `01.Ethernet_DHCP_Basic`.
- [x] `02.Ethernet_StaticIP_Basic`.
- [x] `03.Ethernet_Diagnostics`.

### `JWPLC_RS485`

- [x] `01.RS485_Send`.
- [x] `02.RS485_Echo`.
- [x] `03.RS485_Status`.

### `JWPLC_ModbusRTU`

- [x] `01.ModbusRTU_Slave_Holding`.
- [x] `02.ModbusRTU_Master_Read`.
- [x] `03.ModbusRTU_Master_Write`.

- [x] `ALPHA8_WORKSHOP_EXAMPLES.md` creado.
- [x] READMEs de Ethernet, RS-485 y Modbus RTU sincronizados con la API actual.
- [x] 19/19 ejemplos compilados con `jwplc_local:esp32:jwplcbasic`.
- [x] 0 fallos de compilación.

```text
TOTAL=19
PASS=19
FAIL=0
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
ALPHA8_WORKSHOP_EXAMPLES_TOTAL=19
```

## Documentación

- [x] README raíz actualizado a Alpha8.
- [x] README `JWPLC_Display`.
- [x] README `JW_MatrixButtons`.
- [x] README `JWPLC_GlobalPeripherals`.
- [x] README `JWPLC_Ethernet`.
- [x] README `JWPLC_RS485`.
- [x] README `JWPLC_ModbusRTU`.
- [x] `ALPHA8_BUILD_SPEED_AND_LAZY_LINK.md`.
- [x] `ALPHA8_HMI_BUTTON_VALIDATION.md`.
- [x] `ALPHA8_WORKSHOP_EXAMPLES.md`.
- [x] `ALPHA8_CLOSURE_CHECKLIST.md`.
- [x] `ALPHA8_TECHNICAL_CLOSURE.md`.
- [x] `PULL_REQUEST.md`.
- [x] `PRE_RELEASE.md`.
- [x] `ALPHA8_TO_ALPHA9_OPENPLC_HANDOFF.md`.

## Autoload preservado

- [x] Display.
- [x] Ethernet/W5500.
- [x] microSD.
- [x] FRAM.
- [x] RTC.
- [x] botonera.
- [x] RS-485.
- [x] Modbus RTU.
- [x] TCA/I/O.
- [x] mutex SPI global.

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

- [x] No se publica `bootloader.bin` como definitivo.
- [x] OpenPLC no se integra al autoload Arduino.
- [x] ESP32-S3 queda fuera del alcance Alpha8.

## Gates antes del PR técnico

- [x] Source funcional congelado.
- [x] Hardware HMI/botonera aprobado.
- [x] Lazy-link aprobado.
- [x] Conteos estructurales aprobados.
- [x] Archive Display final versionado.
- [x] `git diff --check` pre-examples.
- [x] compilación final Basic.
- [x] compilación final Basic Core.
- [x] HMI final usando precompiled Display.
- [x] lote de 19 ejemplos compilado.
- [x] `git diff --check` final post-examples.
- [x] `git status` limpio post-examples.

## Publicación

- [ ] PR Alpha8 hacia `release/v2.1.x`.
- [ ] CI verde.
- [ ] merge técnico.
- [ ] workflow reconoce `JWPLC_RELEASE_VERSION: 2.1.0-alpha.8`.
- [ ] ZIP publicado.
- [ ] SHA256/size del ZIP registrados.
- [ ] GitHub PreRelease publicada.
- [ ] índice dev actualizado.
- [ ] índice estable sin cambios salvo decisión explícita.
- [ ] instalación aislada desde índice dev.
- [ ] compilación aislada.
- [ ] verificación `Used platform/library`.
- [ ] upload físico desde package publicado.
- [ ] TFT/boot post-upload.
- [ ] compatibilidad Arduino IDE del package publicado.
- [ ] `release/v2.1.x` ancestro de `main`.
- [ ] cierre documental post-publicación.

## Estado de cierre técnico

```text
ALPHA8_SOURCE_FREEZE=PASS
ALPHA8_HARDWARE_GATE=PASS
ALPHA8_BUILD_ARCHITECTURE=PASS
ALPHA8_FINAL_DISPLAY_ARCHIVE=PASS
ALPHA8_DOCUMENTATION=PASS
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
ALPHA8_WORKSHOP_EXAMPLES_TOTAL=19
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_STATUS=TECHNICALLY_CLOSED
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```

Alpha9 no debe modificar el branch Alpha8. Debe partir de la base Alpha8 publicada/cerrada o de la rama de release ya integrada, según el flujo de publicación.