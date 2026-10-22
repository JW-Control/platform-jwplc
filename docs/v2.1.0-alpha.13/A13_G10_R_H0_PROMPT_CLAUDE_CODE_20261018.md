# Prompt de transferencia a Claude Code — Alpha13 / G10-R / H0

Actúa como **revisor independiente senior de firmware ESP32, FreeRTOS, Arduino IDE/CLI, buses SPI compartidos y Ethernet W5500**. Quiero resolver el watchdog físico de `JW-Control/platform-jwplc`, versión **JWPLC Basic v2.0.0 / package v2.1.0-alpha.13**, hito **G10-R / H0**. No necesito otra cadena automática de sondas sin un diagnóstico previo. Mi prioridad es **encontrar la causa raíz y corregirla sin romper nada**.

## Fuente obligatoria de contexto

Repositorio: `https://github.com/JW-Control/platform-jwplc`. La referencia experimental vigente es `codex/alpha13-h0-w5100-init-stage-20261018`; el **HEAD de código físicamente probado** fue `f0b9075130158b99d89e0f9a574d964236614b78`, asociado al PR **#114 Draft**. Si hay commits documentales posteriores, revisa el diff y no confundas el HEAD de documentación con el ELF probado.

**Lee primero completamente**:
- `docs/v2.1.0-alpha.13/A13_G10_R_H0_HANDOFF_CLAUDE_CODE_20261018.md`
- `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md` (el encabezado vigente prevalece sobre apartados históricos)
- `docs/v2.1.0-alpha.13/A13_H0_W5100_INIT_STAGE_20261018.md`
- Los documentos H0-B/C/D/E y la evidencia real de PR #109–#114.

Después examina los archivos de código:
- `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp` y `w5100.h`
- `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp`, `JWPLC_W5x00_Ethernet.cpp`
- `JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_spi_bus.cpp`, `peripherals_init.cpp` y autoload/ownership real
- `tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino`
- `tools/alpha13/gates/a13_g10_r_build_candidate.py`, `a13_g10_r_h0_link_stage.py`, `a13_g10_r_h0_w5100_stage.py`, `a13_g10_r_h0_init_stage.py`.

## Evidencia física que debes explicar

1. Original A: llamar `JWPLC_Ethernet.linkStatus()` en primer `G10R_READY` produce `rst:0x8 (TG1WDT_SYS_RESET)`; UART se corta después de `LINK=`.
2. B: al quitar solo las dos lecturas directas del sketch y mostrar centinela `LINK=-9`, COM4 funciona al menos 30 s; autoload y periféricos siguen activos.
3. C: una sola lectura real algo más tarde devuelve `PHY=1 STAGE=50`, sin WDT durante ~25 s.
4. D: una lectura real **en primer READY** provoca WDT. `RUN=1 ATTEMPT=1 LAST_STAGE=30 VALID=1`: mutex JWPLC tomado, `Ethernet.linkStatus()` no retornó.
5. E: con backend W5100 instrumentado, `LAST_STAGE=32` → `W5100Class::init()` no retornó.
6. F: con `W5100.init()` instrumentado, el **último resultado comprobado** es:

```text
19:07:17:324 -> G10R_READY:IP=0.0.0.0 TCP=0 UDP=0 RTU=1 BOOT=A666A262 LINK=
19:07:18:224 -> rst:0x8 (TG1WDT_SYS_RESET)
19:07:19:051 -> G10R_H0_PREVIOUS:RUN=1 ATTEMPT=1 LAST_STAGE=63 VALID=1
19:07:19:056 -> G10R_H0_TRACE=REBOOT_NO_AUTO_REPEAT
```

**STAGE 63 = `SPI.begin()` ya retornó, pero no se alcanzó STAGE 64 después de `initSS()`.** Para ESP32, `initSS()` en `w5100.h` usa `pinMode(ss_pin, OUTPUT)`. NO afirmes sin evidencia que `pinMode()` está averiado ni que `SPI.begin()` está colgado. Verifica `ss_pin`, `Ethernet.init(sspin)`, inicialización concurrente del backend, task/lock ownership, precompilación/ELF y timing. Hay un `static bool initialized` no atómico en `W5100.init()`; tratar reentrancia como hipótesis que requiere prueba.

## Método de trabajo

1. **Fase 1 obligatoria: auditoría read-only independiente.** Reconstituye call graph, secuencia de arranque/autoload/tareas, CS, locks, valor efectivo de `ss_pin`, preprocesado ESP32 y fuente real enlazada. Contrasta H0-A–F y plantea 2–4 hipótesis ordenadas, con evidencia a favor/en contra y un experimento mínimo que las distinga. Evita repetir compilaciones ya cubiertas por CI sin modificación del código.
2. **Fase 2, solo si obtienes un defecto concreto verificable:** propone la corrección mínima con su impacto y, si las normas/alcance te lo permiten, implémentala en una **rama o worktree aislados** desde la fuente correcta. No utilices el worktree local de `v2.1.0-alpha.15-HmiDesigner`. Conserva API `linkStatus()` y autoload. Haz pruebas N0/N1, fuente→objeto→ELF y compilación Arduino selectiva; N3 integrada cuando toque. Añade tests negativos que hubieran detectado el defecto. Documenta el PR en español como Draft. Si no hay causa confirmada, **NO inventes reparación**: devuelve una propuesta de prueba física única y justificada.
3. **Fase 3:** comparte un reporte consolidado en español con archivos/líneas, hipótesis/defecto demostrados, cambios propuestos o diff, consecuencias sobre concurrencia/periféricos, CI exacta, riesgos y `NEXT_GATE`.

## Límites estrictos

- **NO** cargar firmware, resetear COM4, acceder a hardware, cambiar cableado ni autorizar por tu cuenta una campaña física. COM14 Slave permanece intacto. El usuario autorizará por separado cualquier prueba.
- **NO** fusionar PR, editar Alpha15, tocar main por defecto, publicar release o cambiar bootloader/FlashFreq/OTA.
- **NO** retirar del autoload Display, Ethernet, SD, FRAM, RTC, botonera, RS485, Modbus RTU/TCP ni TCA/I/O por velocidad o para ocultar el fallo.
- **NO** dejar `LINK=-9` como solución. No sustituir silenciosamente `linkStatus()` por `isReady()` (distinta semántica), aunque puedes sugerir una API cacheada *adicional* para telemetría.
- **NO** tratar los 15/15 jobs de CI en el HEAD F como certificación física: la CI compila, el hardware **reproduce el watchdog**.
- **NO** asumir OpenPLC integrado, OTA definida ni configuración final de bootloader.
- **NO** incluir MAC privadas en issues, PR o logs públicos; waiver sin backup solo aplica a las dos unidades de desarrollo previamente consentidas.

**Quiero tu criterio técnico independiente. Si consideras que hemos perseguido el lugar equivocado durante A–F, explica por qué con referencias concretas al código y propón una ruta mejor.**