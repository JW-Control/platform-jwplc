# v2.1.0-alpha.11 — Checklist de cierre

Fecha: 2026-09-06

## Objetivo

Cerrar Alpha11 consolidando JWPLC HMI Designer V1, la API pública declarativa `JWPLC_UI`, navegación multipágina, LIVE Preview, codegen, integración con sketch, robustez de botonera e integración de escritorio/Arduino IDE sin romper APIs previamente validadas ni retirar periféricos del autoload normal.

## 0. Freeze funcional previo

- [x] TEXT / VALUE / BOOL / BAR validados.
- [x] multipágina y selector `NN/TT` validados.
- [x] `jwplcUIUpdate()` autogenerado por página.
- [x] lógica de aplicación permanece en `loop()`.
- [x] LIVE Web Serial validado físicamente.
- [x] dirty region + ACK + coalescing validados.
- [x] fallo intermitente de botonera reproducido y corregido.
- [x] limpieza de eventos pendientes al volver de CONTENT a SELECT validada.
- [x] paridad visual Designer / TFT aprobada por usuario.
- [x] responsive WIDE / MEDIUM 50 % aprobado.
- [x] Fit continuo aprobado.
- [x] launcher de Arduino IDE 2.3.4 carga icono.
- [x] launcher Arduino IDE v0.1.2 abre el Designer mediante EXE directo.
- [x] instalador standalone ejecutado en Windows.
- [ ] confirmar guardado canónico `<Sketch>.jwhmi` junto al `.ino`.
- [ ] integrar y validar icono final del EXE/accesos directos.

```text
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
A11_6_STANDALONE_INSTALLER=PASS_NATIVE_ENTRYPOINT
A11_6_PROJECT_CANONICAL_SAVE=PENDING_FINAL_USER_GATE
A11_6_EXE_ICON=PENDING_FINAL_USER_GATE
```

## 1. Icono final del Designer

Asset acordado:

```text
tools/jwplc-hmi-designer/assets/JWPLC-HMI-Designer.ico
```

El builder e instalador deben:

- [ ] embebir el icono en `JWPLC-HMI-Designer.exe`;
- [ ] conservarlo en accesos directos de Escritorio / Menú Inicio;
- [ ] confirmar icono visible después de reinstalar;
- [ ] no alterar funcionamiento LIVE / launcher Arduino IDE.

## 2. Freeze source de JWPLC_Display

Antes de regenerar el archive:

- [ ] `git status` limpio salvo cambios explícitos del cierre.
- [ ] `git diff --check`.
- [ ] revisar cambios Alpha11 de `JWPLC_Display` / `JWPLC_UI`.
- [ ] confirmar `precompiled=full` en `JWPLC_Display/library.properties`.
- [ ] confirmar que `libJWPLC_Display.a` antiguo no se usa como evidencia final.
- [ ] registrar SHA del source freeze.

```text
ALPHA11_DISPLAY_DEVELOPMENT_MODE=SOURCE
JWPLC_DISPLAY_PRECOMPILED_ARCHIVE_ACTIVE=NO
```

## 3. Regeneración final de `libJWPLC_Display.a`

Herramienta vigente:

```text
tools/build-speed-benchmark/Build-JWPLCPrecompiledDisplay.ps1
```

El archive final debe quedar en:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Gates:

- [ ] archive generado desde objetos correspondientes al source freeze.
- [ ] tamaño registrado.
- [ ] SHA-256 registrado.
- [ ] miembros `.o` auditados.
- [ ] `JWPLC_Display` source compiles = 0 en verify precompiled.
- [ ] app size source/precompiled equivalente.
- [ ] payload equivalente fuera de hashes ELF esperables.
- [ ] archive final versionado sólo después de PASS.

```text
A11_2_PRECOMPILED_FINAL=PENDING
A11_FINAL_DISPLAY_ARCHIVE=PENDING
```

## 4. Gate source vs precompiled

Comparar el estado fuente validado contra el archive final:

- [ ] JWPLC Basic compila desde source sin archive.
- [ ] JWPLC Basic compila con archive final.
- [ ] JWPLC Basic Core compila según perfil esperado.
- [ ] sketch HMI generado compila con archive final.
- [ ] `JWPLC_Display` no recompila desde fuente con archive activo.
- [ ] `core.a` / `jwcontrol_precompiled_stub` siguen enlazando según arquitectura vigente.
- [ ] RAM / flash dentro de límites.
- [ ] sin cambios funcionales visibles en TFT/botonera.

## 5. Benchmark final Alpha11

Como Alpha11 modifica `JWPLC_Display` y regenera un precompilado, repetir benchmark de cierre.

Mínimo requerido:

- [ ] cold.
- [ ] warm sin cambios.
- [ ] warm touch cuando aplique.
- [ ] JWPLC Basic.
- [ ] JWPLC Basic Core.
- [ ] conteo de TUs.
- [ ] link observado.
- [ ] tamaño app.
- [ ] logs guardados.
- [ ] tabla final de tiempos.
- [ ] comparación contra baseline vigente (Alpha8/estado heredado apropiado).
- [ ] conclusión explícita: mejora, paridad o regresión aceptada.
- [ ] no retirar periféricos del autoload por rendimiento.

La conclusión debe distinguir variación wall-clock del host de cambios estructurales en TUs/link.

## 6. Gate funcional final con archive

Con `libJWPLC_Display.a` final activo:

- [ ] abrir HMI multipágina.
- [ ] navegación SELECT / CONTENT.
- [ ] ESC vuelve a selector.
- [ ] no reingreso por OK pendiente.
- [ ] `pressed()` en `loop()` cerrado sin `delay()` ni Serial.
- [ ] VALUE dinámico.
- [ ] BOOL dinámico.
- [ ] BAR dinámico.
- [ ] `jwplcUIUpdate()` generado funciona.
- [ ] TFT sin flicker/cuelgue.
- [ ] launcher Desktop sigue abriendo.
- [ ] launcher Arduino IDE sigue abriendo.
- [ ] LIVE sigue disponible.

No hace falta repetir todos los soak históricos de Ethernet/RTU si no fueron modificados, pero el autoload normal debe seguir compilando con todos los periféricos presentes.

## 7. Documentación final Alpha11

Requeridos antes del PR:

- [ ] README raíz actualizado a Alpha11.
- [ ] README `tools/jwplc-hmi-designer/` final.
- [ ] README `JWPLC_Display` actualizado.
- [ ] robustez de botonera documentada: `pressed()/released()` en loop cerrado no requieren `delay()` ni Serial.
- [ ] comportamiento de limpieza de input pendiente documentado.
- [ ] arquitectura Designer / `.jwhmi` / `JWPLC_HMI_Generated.h` documentada.
- [ ] launcher standalone / Arduino IDE experimental documentado.
- [ ] limitaciones: frontend local basado en Edge/Chrome app mode, launcher IDE probado en 2.3.4.
- [ ] `ALPHA11_STATUS.md` cerrado.
- [ ] benchmark final documentado.
- [ ] precompiled final con SHA/size documentado.
- [ ] probado / no probado / limitaciones / pendientes explícitos.

## 8. Decisiones heredadas que no cambian en Alpha11

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

- [ ] no publicar `bootloader.bin` como definitivo.
- [ ] no declarar FlashFreq universal futura.
- [ ] no declarar OTA definida.
- [ ] no mezclar cierre Alpha11 con nuevos cambios OpenPLC.

## 9. Pre-PR técnico

- [ ] source freeze final.
- [ ] archive Display final.
- [ ] benchmark PASS / conclusión aceptada.
- [ ] compilación Arduino CLI.
- [ ] compilación Arduino IDE.
- [ ] gate físico final.
- [ ] `git diff --check`.
- [ ] `git status` limpio.
- [ ] documentación en español.
- [ ] `PULL_REQUEST.md` preparado.
- [ ] `PRE_RELEASE.md` preparado.

Destino:

```text
v2.1.0-alpha.11/feature/hmi-designer
    -> release/v2.1.x
```

## 10. Publicación

- [ ] PR técnico en español.
- [ ] CI verde.
- [ ] merge técnico a `release/v2.1.x`.
- [ ] marcador `JWPLC_RELEASE_VERSION: 2.1.0-alpha.11` reconocido.
- [ ] workflow automático válido.
- [ ] ZIP publicado.
- [ ] tamaño ZIP registrado.
- [ ] SHA-256 ZIP registrado.
- [ ] GitHub PreRelease en español.
- [ ] índice dev actualizado.
- [ ] índice estable sin cambios salvo decisión explícita.

## 11. Validación aislada post-publicación

- [ ] entorno aislado.
- [ ] instalar `2.1.0-alpha.11` desde índice dev publicado.
- [ ] `core list` correcto.
- [ ] compilar `jwplc:esp32:jwplcbasic`.
- [ ] `Used platform` correcto.
- [ ] `Used library` correcto.
- [ ] archive Display publicado realmente usado.
- [ ] upload físico desde package publicado.
- [ ] reset / boot.
- [ ] TFT/HMI básica.
- [ ] botonera.
- [ ] launcher/Designer distribuido documentado según alcance del artefacto.

## 12. Topología y cierre documental

- [ ] verificar `release/v2.1.x` ancestro de `main` después de publicación.
- [ ] sincronización dirigida si realmente fuese necesaria.
- [ ] checklist final actualizado con ZIP/SHA/size.
- [ ] cierre documental post-publicación.
- [ ] fuentes de transferencia del Proyecto actualizadas.
- [ ] ramas temporales limpiadas cuando corresponda.

## Estado actual

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS_PENDING_FINAL_PACKAGING_GATES
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_FINAL_ICON=SELECTED_PENDING_REPO_ASSET
ALPHA11_DISPLAY_PRECOMPILED=PENDING_REGENERATE
ALPHA11_BUILD_SPEED=PENDING_FINAL
ALPHA11_DOCUMENTATION=PENDING_FINAL
ALPHA11_PUBLICATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```
