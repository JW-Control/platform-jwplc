# v2.1.0-alpha.11 — Checklist de cierre

Actualizado: 2026-09-09

## Objetivo

Cerrar Alpha11 consolidando JWPLC HMI Designer V1, la API pública declarativa `JWPLC_UI`, navegación multipágina, PixelMap, LIVE Preview, codegen, integración con sketch, robustez de botonera e integración de escritorio/Arduino IDE, sin romper APIs previamente validadas ni retirar periféricos del autoload normal.

## 0. Freeze funcional

- [x] TEXT / VALUE / BOOL / BAR validados.
- [x] PixelMap y capas validados.
- [x] optimizer `PACKED_SPAN16` validado.
- [x] visibilidad runtime de PixelMap validada.
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
- [x] guardado canónico `<Sketch>.jwhmi` implementado/validado.
- [x] icono final integrado en acceso directo, ventana y taskbar.
- [x] instalador standalone ejecutado en Windows.
- [x] launcher Arduino IDE 2.3.4 abre el Designer.
- [x] autocompletado contextual `JWPLC_Display` validado en Arduino IDE 2.3.4.

```text
A11_FUNCTIONAL_FREEZE=PASS
A11_DESIGNER_V1=PASS_USER
A11_6_STANDALONE_INSTALLER=PASS_NATIVE_ENTRYPOINT
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
A11_6_ARDUINO_IDE_AUTOCOMPLETE=PASS_USER_2_3_4
```

## 1. Identidad visual / escritorio

Asset:

```text
tools/jwplc-hmi-designer/assets/JWPLC-HMI-Designer.ico
```

- [x] icono embebido/empleado por `JWPLC-HMI-Designer.exe`.
- [x] icono en acceso directo.
- [x] icono de ventana.
- [x] icono de taskbar.
- [x] arranque Electron sin flash blanco mediante `ready-to-show`.
- [x] launcher Arduino IDE no altera LIVE ni el runtime del Designer.

## 2. `JWPLC_Display` precompilado final

Archive:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

- [x] generado desde los 6 TUs source esperados.
- [x] tamaño registrado: `849596` bytes.
- [x] SHA-256 registrado: `2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af`.
- [x] miembros `.o` auditados.
- [x] miembros byte-a-byte equivalentes a objetos source.
- [x] `JWPLC_Display` source compiles = 0 en build precompiled.
- [x] paridad EMPTY source/archive.
- [x] paridad HMI source/archive.
- [x] RAM equivalente.
- [x] archive final versionado.

```text
A11_2_PRECOMPILED_FINAL=PASS
A11_FINAL_DISPLAY_ARCHIVE=PASS
DISPLAY_ARCHIVE_COMMIT=4142f801fbacc9388bf63c3a6352696522d6b445
```

## 3. Core precompilado JWPLC Basic

Durante el gate final de runtime se detectó que `core.a` estaba desfasado respecto del source Alpha11. Se regeneró y verificó antes del cierre.

Archive:

```text
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

- [x] build fuente desde `cores/jwcontrol` PASS.
- [x] `jwcontrol` source TUs durante generación: 64.
- [x] candidate bytes: `3019320`.
- [x] SHA-256: `6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68`.
- [x] verify target normal PASS.
- [x] `jwcontrol` source TUs en target normal: 0.
- [x] `jwcontrol_precompiled_stub` TUs: 1.
- [x] `core.a JWPLCBASIC` enlazado: YES.
- [x] validación física posterior PASS.
- [x] archive versionado en commit `3cf37145d4555689b9bff80c8b3793128bb9090e`.

```text
ALPHA11_CORE_PRECOMPILED_SYNC=PASS
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

## 4. Gate de runtime cerrado

- [x] `pressed()/released()/isDown()` funcionan sin `delay()` ni Serial.
- [x] `digitalWrite(Q0_0, estado)` repetido en cada `loop()` funciona sin `delay(1)`.
- [x] shadow TCA evita I2C cuando el estado de salida no cambia.
- [x] `jwplcSystemTask` ejecuta con prioridad esperada del source Alpha11.
- [x] RTC sigue actualizando.
- [x] IO sigue actualizando.
- [x] Display/IDLE sigue actualizando.
- [x] sketch original recompilado/subido y validado por usuario.

```text
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
ALPHA11_RUNTIME_REGRESSION_GATE=PASS
```

## 5. Inicialización TFT

- [x] `TFT_CS` inicial en HIGH.
- [x] `TFT_RST` mantenido LOW durante autoload previo a init.
- [x] primer frame IDLE forzado inmediatamente después de `displayBegin`.
- [x] TFT funcional tras reset/boot.
- [x] TFT no se congela con loop intensivo.

Nota: la eliminación visual absoluta de cualquier patrón previo a la ejecución del firmware depende también del estado eléctrico/backlight. Alpha11 implementa la estabilización software sin convertir ese detalle cosmético en bloqueo del cierre funcional.

## 6. Benchmark final Alpha11

- [x] cold.
- [x] warm sin cambios.
- [x] warm touch.
- [x] JWPLC Basic.
- [x] JWPLC Basic Core.
- [x] conteo de TUs.
- [x] link observado.
- [x] tamaño app.
- [x] logs guardados.
- [x] tabla final de tiempos.
- [x] comparación contra Alpha10.
- [x] conclusión explícita.
- [x] no se retiraron periféricos del autoload.

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

Documento:

```text
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
```

## 7. Gate funcional final HMI

- [x] abrir HMI multipágina.
- [x] navegación SELECT / CONTENT.
- [x] ESC vuelve a selector.
- [x] no reingreso por OK pendiente.
- [x] VALUE dinámico.
- [x] BOOL dinámico.
- [x] BAR dinámico.
- [x] PixelMap.
- [x] `jwplcUIUpdate()` generado funciona.
- [x] TFT sin cuelgue.
- [x] launcher Desktop abre.
- [x] launcher Arduino IDE abre.
- [x] LIVE disponible.
- [x] compilación/subida desde Arduino IDE después de integrar launcher 0.1.6.

No se repiten soak históricos de Ethernet/RTU porque Alpha11 no modifica esos runtimes; el autoload normal conserva Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O.

## 8. Autocompletado Arduino IDE

Versión final Alpha11:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

- [x] `JWPLC_Display.` ofrece API recomendada.
- [x] getters/aliases redundantes no se priorizan.
- [x] `setIdleWakeMode(` ofrece `IDLE_WAKE_*`.
- [x] `setIdleWakeButton(` ofrece `BTN_*`.
- [x] `setIdleReturnMode(` ofrece `IDLE_RETURN_*`.
- [x] `setIdleReturnButton(` ofrece `BTN_*`.
- [x] `setUserRefreshMode(` ofrece `USER_REFRESH_*`.
- [x] extensión se activa en Arduino IDE 2.3.4.
- [x] instalador de VSIX desacoplado de presencia del Designer.

## 9. Documentación final Alpha11

- [x] `ALPHA11_STATUS.md` actualizado al cierre técnico.
- [x] benchmark final documentado.
- [x] Display precompilado con SHA/size documentado.
- [x] core precompilado con SHA/size documentado.
- [x] robustez de botonera documentada.
- [x] runtime sin `delay(1)` documentado.
- [x] arquitectura Designer / `.jwhmi` / `JWPLC_HMI_Generated.h` documentada.
- [x] launcher standalone / Arduino IDE documentado.
- [x] limitaciones y decisiones heredadas explícitas.
- [ ] README raíz actualizado a Alpha11.
- [ ] README `tools/jwplc-hmi-designer/` actualizado al estado final.
- [ ] README `JWPLC_Display` actualizado a Alpha11.
- [ ] `PULL_REQUEST.md` Alpha11 preparado.
- [ ] `PRE_RELEASE.md` Alpha11 preparado.

## 10. Decisiones heredadas que no cambian

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

- [x] no publicar `bootloader.bin` como definitivo.
- [x] no declarar FlashFreq universal futura.
- [x] no declarar OTA definida.
- [x] no mezclar cierre Alpha11 con cambios OpenPLC nuevos.

## 11. Pre-PR técnico

- [x] archive Display final.
- [x] core precompilado final sincronizado.
- [x] benchmark PASS / conclusión aceptada.
- [x] compilación Arduino CLI.
- [x] compilación Arduino IDE.
- [x] gate físico final de runtime.
- [x] documentación en español en curso de cierre.
- [ ] `PULL_REQUEST.md` preparado.
- [ ] `PRE_RELEASE.md` preparado.
- [ ] pull local del commit documental final.
- [ ] `git diff --check` final.
- [ ] `git status` limpio final.

Destino:

```text
v2.1.0-alpha.11/feature/hmi-designer
    -> release/v2.1.x
```

## 12. Publicación

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

## 13. Validación aislada post-publicación

- [ ] entorno aislado.
- [ ] instalar `2.1.0-alpha.11` desde índice dev publicado.
- [ ] `core list` correcto.
- [ ] compilar `jwplc:esp32:jwplcbasic`.
- [ ] `Used platform` correcto.
- [ ] `Used library` correcto.
- [ ] `core.a` publicado realmente usado.
- [ ] archive Display publicado realmente usado.
- [ ] upload físico desde package publicado.
- [ ] reset / boot.
- [ ] TFT/HMI básica.
- [ ] botonera.

## 14. Topología y cierre post-publicación

- [ ] verificar paridad/topología `release/v2.1.x` / `main` según política vigente.
- [ ] sincronización dirigida si realmente fuese necesaria.
- [ ] checklist actualizado con ZIP/SHA/size.
- [ ] cierre documental post-publicación.
- [ ] fuentes de transferencia del Proyecto actualizadas.
- [ ] ramas temporales limpiadas cuando corresponda.

## Estado actual

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_DISPLAY_PRECOMPILED=PASS
ALPHA11_CORE_PRECOMPILED=PASS
ALPHA11_BUILD_SPEED=PASS_WITH_HOST_VARIATION
ALPHA11_RUNTIME_REGRESSION_GATE=PASS_PHYSICAL
ALPHA11_AUTOCOMPLETE=PASS_USER
ALPHA11_DOCUMENTATION=IN_FINALIZATION
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PUBLICATION=PENDING
ALPHA11_STATUS=TECHNICALLY_CLOSED
```
