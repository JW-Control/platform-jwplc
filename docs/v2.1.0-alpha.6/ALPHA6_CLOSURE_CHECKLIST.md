# v2.1.0-alpha.6 — Checklist de cierre

Fecha: 2026-08-28

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

- [x] Source build fresco generado.
- [x] Dos objetos Display fuente verificados.
- [x] Archive regenerado con esos objetos.
- [x] Miembros extraídos byte-idénticos a source.
- [x] Build archive compila cero TUs source de Display.
- [x] Arduino reporta librería precompilada.
- [x] RAM source/archive idéntica.
- [x] Conjunto de símbolos idéntico.
- [x] Delta de aplicación explicado por linker fill.
- [x] Archive final versionado.
- [x] SHA-256 registrado.

Archive final:

```text
368202 bytes
a0094a9d9bf5c40bbd91a18514d97c488b2e8ba1ba6c18ec8161cb74445b416e
```

## Compilación

- [x] Cold compile normal final.
- [x] `precompiled=full` activo.
- [x] Cero objetos/TUs source de Display en build normal.
- [x] Git limpio después del build.
- [x] Benchmark final Basic.
- [x] Benchmark final Basic Core.
- [x] 12/12 fases OK.
- [x] Comparación Alpha5 vs Alpha6 documentada.
- [x] Sin regresión relevante frente a Alpha5.
- [x] Warm builds mejorados en conjunto.

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
- [x] Validación final Alpha6 documentada.
- [x] Tabla Alpha5 vs Alpha6 documentada.
- [x] PR body preparado en español.
- [x] PreRelease preparada en español.

## GitHub / publicación

- [ ] PR Alpha6 abierto contra `release/v2.1.x`.
- [ ] CI del PR aprobado.
- [ ] PR mergeado.
- [ ] Auto workflow detecta `JWPLC_RELEASE_VERSION: 2.1.0-alpha.6`.
- [ ] `Release JWPLC Arduino Package` aprobado.
- [ ] ZIP `jwplc-esp32-2.1.0-alpha.6.zip` publicado.
- [ ] SHA-256 y tamaño generados por workflow.
- [ ] GitHub PreRelease `v2.1.0-alpha.6` publicada.
- [ ] índice dev actualizado.
- [ ] PR automático de índices hacia `main` creado.
- [ ] PR de índices mergeado.
- [ ] instalación limpia desde Boards Manager validada.
- [ ] documentación de cierre de publicación añadida.

## Gate

Antes del merge:

```text
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_BUILD_SPEED=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_FINAL_PRODUCTION_COLD=PASS
```

Estado:

```text
READY_FOR_PR
```
