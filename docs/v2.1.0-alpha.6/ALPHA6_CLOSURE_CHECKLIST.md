# v2.1.0-alpha.6 — Checklist de cierre

Fecha: 2026-08-29

## Base e integración

- [x] Se detectó que la línea Alpha6 original no partía del cierre funcional final de Alpha5.
- [x] Backup del Alpha6 previamente validado creado.
- [x] Alpha6 trasladada sobre `release/v2.1.x @ 64068556`.
- [x] Integración sin conflictos reales.
- [x] `jwplcbasic.build.core=jwcontrol_precompiled_stub` preservado.
- [x] `precompiled/core/JWPLCBASIC/core.a` preservado.
- [x] Backend W5500 separado eliminado y consolidado dentro de `JWPLC_Ethernet`.
- [x] Smoke source-fallback posterior a la corrección: PASS.

## Estado técnico

- [x] Runtime Ethernet cooperativo/no bloqueante validado.
- [x] W5500 detectado.
- [x] DHCP inicial validado.
- [x] T1 renew validado físicamente.
- [x] T2 rebind validado físicamente.
- [x] Recuperación de link sin reset validada.
- [x] Router -> laptop sin DHCP -> router validado.
- [x] IP estática validada.
- [x] HTTP real validado.
- [x] Stress SPI/Ethernet 10 min aprobado.
- [x] Mutex SPI sin fallas de seguridad.
- [x] FRAM/SD/TFT coexistiendo durante stress.
- [x] Hooks DHCP excluidos del build normal.
- [x] `BUS` físico validado con timeout y recuperación RTU.
- [x] `ERR` alfanumérico validado físicamente.
- [x] `ETH` diagnóstico visual validado.
- [x] APIs legacy conservadas.

## Display precompilado

- [x] Source build fresco generado sobre la base corregida.
- [x] Dos objetos Display fuente verificados.
- [x] Archive regenerado con esos objetos.
- [x] Miembros extraídos byte-idénticos a source.
- [x] Build archive compila cero TUs source de Display.
- [x] Arduino reporta librería precompilada.
- [x] RAM source/archive idéntica.
- [x] Conjunto de símbolos idéntico.
- [x] APP source/archive idéntica.
- [x] Binario source/archive idéntico.
- [x] Archive final versionado.
- [x] SHA-256 registrado.

Archive final:

```text
368174 bytes
4da9143e5e80d8ad0890e25bda8802ecee489b2a8c452c3ef1be556cff9541a7
```

## Compilación

- [x] Cold compile normal final sobre `379246c9`.
- [x] `precompiled=full` activo.
- [x] Cero objetos/TUs source de Display en build normal.
- [x] `core.a` precompilado Basic observado.
- [x] Git limpio después del build.
- [x] Benchmark final Basic.
- [x] Benchmark final Basic Core.
- [x] 12/12 fases OK.
- [x] Comparación Alpha5 vs Alpha6 corregida y documentada.
- [x] Regresión cold cercana al 10 % documentada explícitamente.
- [x] Warm builds mejoran en promedio 4.43 %.
- [x] Penalización cold aceptada como costo conocido, sin retirar periféricos.

Marcadores:

```text
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_WARM_AVG_IMPROVEMENT=4.43_PERCENT
```

## Autoload y compatibilidad

- [x] Display permanece integrado.
- [x] Ethernet/W5500 permanece integrado.
- [x] microSD permanece integrada.
- [x] FRAM permanece integrada.
- [x] RTC permanece integrado.
- [x] botonera permanece integrada.
- [x] RS-485 permanece integrado.
- [x] Modbus RTU permanece integrado.
- [x] TCA/I/O permanece integrado.
- [x] No se retiraron periféricos por rendimiento.
- [x] Arduino CLI validado.
- [x] Compatibilidad Arduino IDE preservada.

## Decisiones de release

- [x] App-only: herramienta de desarrollo; no default.
- [x] Bootloader precompilado: no adoptado.
- [x] `bootloader.bin` definitivo: no publicado.
- [x] Configuración universal final de flash: pendiente explícito.
- [x] OTA: fuera de alcance.
- [x] OpenPLC integrado obligatorio: fuera de alcance.
- [x] `JWPLC_LogicRuntime_UI` versionado adicional: diferido a otro alpha.

## Documentación

- [x] README raíz actualizado.
- [x] README `JWPLC_Display` actualizado.
- [x] README `JWPLC_Ethernet` actualizado.
- [x] README `JWPLC_GlobalPeripherals` creado.
- [x] README `JWPLC_RS485` actualizado.
- [x] README `JWPLC_ModbusRTU` actualizado.
- [x] README `JWPLC_LogicRuntime` actualizado.
- [x] README `JWPLC_LogicRuntime_UI` actualizado sin cambiar su versión.
- [x] Validación final Alpha6 corregida.
- [x] Tabla Alpha5 vs Alpha6 corregida.
- [x] Corrección de base registrada.
- [x] PR body actualizado en español.
- [x] PreRelease actualizada en español.
- [x] Cierre de publicación documentado en `ALPHA6_PUBLICATION_CLOSURE_20260829.md`.

## GitHub / publicación

- [x] PR corregido Alpha6 abierto contra `release/v2.1.x` (`#69`).
- [x] CI del PR aprobado.
- [x] PR mergeado.
- [x] Auto workflow detectó `JWPLC_RELEASE_VERSION: 2.1.0-alpha.6`.
- [x] `Release JWPLC Arduino Package` aprobado.
- [x] ZIP `jwplc-esp32-2.1.0-alpha.6.zip` publicado.
- [x] SHA-256 y tamaño generados por workflow.
- [x] GitHub PreRelease `v2.1.0-alpha.6` publicada.
- [x] índice dev actualizado.
- [x] PR automático de índices hacia `main` creado (`#70`).
- [x] PR de índices mergeado.
- [x] instalación limpia desde el índice dev validada.
- [x] compilación aislada desde el package publicado validada.
- [x] carga física aislada por USB validada.
- [x] arranque posterior al upload sin boot loop validado.
- [x] TFT posterior al upload validada (`BUS: INI`, `ETH: LNK` sin comunicaciones conectadas).
- [x] documentación de cierre de publicación añadida.

Artefacto publicado:

```text
ZIP    : jwplc-esp32-2.1.0-alpha.6.zip
Size   : 24294308 bytes
SHA256 : cfd81391e80852f26c279ca67885227d6f24e4d3ec6b93d715e072176878c9f1
```

Gate aislado final:

```text
ALPHA6_ISOLATED_INSTALL=PASS
ALPHA6_ISOLATED_COMPILE=PASS
COMPILE_SECONDS=53.027
APP_BIN_BYTES=395120
ALPHA6_ISOLATED_PHYSICAL_UPLOAD=PASS
ALPHA6_POST_UPLOAD_TFT=PASS
ALPHA6_POST_UPLOAD_BOOT=PASS
```

## Higiene de ramas

- [x] Divergencia histórica `main` / `release/v2.1.x` auditada.
- [x] `JWPLC/2.1.0` de la sincronización verificado idéntico al release Alpha6 (`PACKAGE_DIFF_COUNT=0`).
- [x] PR de sincronización `#71` mergeado mediante merge commit excepcional.
- [x] `release/v2.1.x` confirmado como ancestro de `main`.
- [x] `Require linear history` reactivado después de la sincronización.

Marcadores:

```text
ALPHA6_BRANCH_DIVERGENCE=RESOLVED
RELEASE_IS_ANCESTOR_OF_MAIN=PASS
MAIN_LINEAR_HISTORY_PROTECTION=RESTORED
```

## Gate final

```text
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
ALPHA6_BUILD_SPEED=PASS
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_ISOLATED_INSTALL=PASS
ALPHA6_ISOLATED_COMPILE=PASS
ALPHA6_ISOLATED_PHYSICAL_UPLOAD=PASS
ALPHA6_POST_UPLOAD_TFT=PASS
ALPHA6_POST_UPLOAD_BOOT=PASS
ALPHA6_PUBLICATION_CLOSURE=PASS
```

Estado:

```text
ALPHA6_STATUS=CLOSED
```
