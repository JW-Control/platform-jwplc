# v2.1.0-alpha.8 — Checklist de cierre

Fecha: 2026-09-04

## Objetivo

Cerrar Alpha8 consolidando la interacción TFT/botonera y una HMI Arduino declarativa eficiente, sin retirar periféricos del autoload normal, sin romper APIs ya validadas y sin adelantar el alcance OpenPLC previsto para Alpha9.

## Base y alcance

- [x] Branch técnico: `v2.1.0-alpha.8/fix/buttons-display-autowake`.
- [x] Alpha7 cerrado/publicado tomado como base funcional.
- [x] Scope congelado:
  - Alpha8 = TFT + botonera + HMI Arduino eficiente;
  - Alpha9 = exposición de esa HMI hacia OpenPLC/Ladder.
- [x] OpenPLC no se integra al runtime Arduino dentro de Alpha8.
- [x] No se elimina ningún periférico del autoload normal.

## Display / autowake

- [x] Reproducción dirigida de transición USER no solicitada durante el diagnóstico.
- [x] Default de Alpha8 cambiado a `IDLE_WAKE_DISABLED`.
- [x] Wake automático continúa disponible por API explícita.
- [x] Navegación Display basada en flancos físicos propios.
- [x] El Display deja de depender de latches consumibles `pressed()/released()` de aplicación.
- [x] Entrada USER absorbe correctamente el botón sostenido que originó la transición.
- [x] Salida IDLE evita rebote lógico de navegación.
- [x] `JWPLC_Display.clearPendingInput()` preservado.

Marcadores:

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
- [x] `pressed()`/`released()` continúan disponibles al sketch.
- [x] `anyPressedOrRepeated()` restaurado para tratar sólo `PRESS/REPEAT` como actividad válida.
- [x] `RELEASE` no se interpreta como `pressed/repeated`.
- [x] Router de refresh del Display basado en cambio de máscara física.
- [x] Pulsación sostenida no congela runtime/TFT.

## HMI declarativa

- [x] `JWPLC_UIField`.
- [x] máximo interno de 32 campos.
- [x] `VALUE`.
- [x] `TEXT`.
- [x] `BOOL`.
- [x] `BAR`.
- [x] múltiples páginas USER.
- [x] formato numérico.
- [x] signed/unsigned.
- [x] leading zeros.
- [x] overflow visible.
- [x] layout inline/stacked.
- [x] alineación LEFT/CENTER/RIGHT.
- [x] colores de campo.
- [x] bool text.
- [x] rango de barra.
- [x] helpers `JWPLC_UIValueField`, `JWPLC_UITextField`, `JWPLC_UIBoolField`, `JWPLC_UIBarField`.
- [x] `setFields()`.
- [x] `setValue()`.
- [x] `setText()`.
- [x] `setBool()`.
- [x] `setBar()`.
- [x] `setUserPage()`.
- [x] refresh on-demand/periodic.
- [x] invalidación field/all.
- [x] no uso de `new` para runtime HMI.
- [x] no uso de `String` dinámico para buffers internos de valores.

## Dirty refresh / SPI

- [x] Valores sólo se marcan dirty si cambia el resultado formateado.
- [x] Campos de página inactiva quedan cacheados sin forzar TFT.
- [x] Entrar a USER/página muestra el último valor cacheado.
- [x] Redibujado normal limitado a región dinámica.
- [x] `fillScreen()` reservado para transiciones/redraw completo.
- [x] Una ventana de adquisición TFT para los dirty fields de una pasada.
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
- [x] `lastUpdateMs()`.
- [x] Sin nuevas transacciones físicas de I/O/RTC.
- [x] `JWPLC_IO.outputs()` documentado explícitamente como `Q0.0..Q0.7` en JWPLC Basic.

## Build speed

- [x] Baseline Alpha6 conservado.
- [x] Regresión inicial de un TU RuntimeView identificada.
- [x] RuntimeView reintegrado a `JWPLC_GlobalPeripherals.cpp`.
- [x] Basic cold vuelve a 15 compilaciones.
- [x] Core cold vuelve a 78 compilaciones.
- [x] Warm mantiene 1 compilación.
- [x] Lazy-link implementado para HMI.
- [x] `01_empty` no extrae motor HMI.
- [x] HMI gate sí extrae motor HMI.
- [x] APP vacía reducida 3456 bytes respecto al Alpha8 pre-lazy.
- [x] Variación wall-clock del host documentada.
- [x] No se reclama mejora global de segundos frente a Alpha6.
- [x] Performance freeze aprobado.

Marcadores:

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

- [x] Fuente `JW_MatrixButtons 1.0.5` no modificada por Alpha8.
- [x] `precompiled=full` continúa vigente.
- [x] Archive existente no requiere regeneración por el alcance Alpha8.

Archive actualmente versionado:

```text
libJW_MatrixButtons.a
129506 bytes
```

### JWPLC_Display

- [x] Source HMI congelado.
- [x] `precompiled=full` preservado.
- [x] Builder P1 ejecutado con `-Libraries JWPLC_Display`.
- [x] Source compile aprobado.
- [x] Verify precompiled aprobado.
- [x] 4 objetos archivados.
- [x] Gate de link precompiled aprobado.
- [x] Archive candidato usado durante validación física.
- [ ] Archive final Alpha8 versionado en Git.
- [ ] Hash del archive versionado revalidado después de pull limpio.

Candidato validado:

```text
Bytes  : 642576
SHA256 : D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
```

El archive histórico todavía presente en Git antes del cierre Alpha8 corresponde a Alpha6 y debe ser reemplazado antes del PR técnico.

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

Marcadores:

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

## Incidente histórico de taller

- [x] Se registra como incidente histórico.
- [x] No se afirma una causa única no demostrada.
- [x] Alpha8 actual se considera estable operacionalmente para continuar.
- [x] Para futuras reproducciones se exige preservar IDE/package/FQBN/Used library/SHA binario antes de reinstalar.

Clasificación:

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

## Documentación

- [x] README `JWPLC_Display` actualizado.
- [x] README `JW_MatrixButtons` actualizado.
- [x] README `JWPLC_GlobalPeripherals` actualizado.
- [x] README raíz actualizado a Alpha8.
- [x] `ALPHA8_BUILD_SPEED_AND_LAZY_LINK.md`.
- [x] `ALPHA8_HMI_BUTTON_VALIDATION.md`.
- [x] `ALPHA8_CLOSURE_CHECKLIST.md`.
- [ ] `PULL_REQUEST.md`.
- [ ] `PRE_RELEASE.md`.

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

- [x] App-only continúa como herramienta de desarrollo.
- [x] App-only no se convierte en upload default.
- [x] Bootloader precompilado definitivo no adoptado.
- [x] `bootloader.bin` no se publica como definitivo.
- [x] configuración universal final de Flash continúa pendiente.
- [x] OTA continúa fuera de alcance.
- [x] OpenPLC no se integra al autoload Arduino.
- [x] ESP32-S3 queda fuera del alcance Alpha8.

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

## Gates antes del PR técnico

- [x] Source funcional congelado.
- [x] Hardware HMI/botonera aprobado.
- [x] Lazy-link aprobado.
- [x] Conteos estructurales aprobados.
- [ ] Archive Display final versionado.
- [ ] `git diff --check` local final.
- [ ] `git status` limpio.
- [ ] compilación final Basic.
- [ ] compilación final Basic Core.
- [ ] ejemplos Alpha8 relevantes compilados.
- [ ] Arduino IDE final cuando aplique.

## Publicación

- [ ] PR Alpha8 hacia `release/v2.1.x`.
- [ ] CI verde.
- [ ] merge técnico.
- [ ] workflow reconoce `JWPLC_RELEASE_VERSION: 2.1.0-alpha.8`.
- [ ] ZIP publicado.
- [ ] SHA256/size registrados.
- [ ] GitHub PreRelease publicada.
- [ ] índice dev actualizado.
- [ ] índice estable sin cambios salvo decisión explícita.
- [ ] instalación aislada desde índice dev.
- [ ] compilación aislada.
- [ ] upload físico desde package publicado.
- [ ] TFT/boot post-upload.
- [ ] `release/v2.1.x` ancestro de `main`.
- [ ] cierre documental post-publicación.

## Estado de cierre técnico actual

```text
ALPHA8_SOURCE_FREEZE=PASS
ALPHA8_HARDWARE_GATE=PASS
ALPHA8_BUILD_ARCHITECTURE=PASS
ALPHA8_DOCUMENTATION=IN_PROGRESS
ALPHA8_FINAL_DISPLAY_ARCHIVE=PENDING_GIT_VERSIONING
ALPHA8_TECHNICAL_CLOSURE=PENDING_FINAL_ARCHIVE
```

Alpha9 no debe modificar el branch Alpha8. Puede iniciarse después de cerrar el archive/PR/publicación de Alpha8 o sobre la base publicada correspondiente, según el flujo de release.
