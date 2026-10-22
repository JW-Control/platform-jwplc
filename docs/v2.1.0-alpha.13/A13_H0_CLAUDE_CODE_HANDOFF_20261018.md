# Transferencia de investigación a Claude Code — JWPLC Basic Alpha13 / H0 / G10-R

**Fecha:** 2026-10-18. **Idioma de trabajo:** español. **Objetivo:** auditoría causal independiente del watchdog W5500 durante consultas de linkStatus. **No implementar ni escribir hardware hasta recibir aprobación**.

## Repositorio y aislamiento

- Repo `JW-Control/platform-jwplc`.
- Alpha de trabajo `v2.1.0-alpha.13`, integración G10-I/G10-R, PR base #109 Draft.
- Cadena de ramas experimentales: PR #110 (omit link), PR #111 (single PHY), PR #112 (READY 8), PR #113 (W5100 PHY stages), PR #114 (W5100.init stages).
- Rama documento/diagnóstico actual: `codex/alpha13-h0-w5100-init-stage-20261018`.
- HEAD **efectivamente subido a COM4 y CI 15/15 PASS:** `f0b9075130158b99d89e0f9a574d964236614b78`.
- Commit posterior **SOLO documental** del informe físico: `51c717514779534d029383590fdcebb84367cbd8`. No confundir SHA del firmware con SHA actual del branch.
- Worktree Windows de laboratorio H0-F: `C:\Users\jeykc\Documentos\GitHub\platform-jwplc-alpha13-h0-init-f`. Este directorio puede seguir en el HEAD firmware `f0b9075`; no forzar checkout/pull. Repo principal de desarrollo y rama `v2.1.0-alpha.15-HmiDesigner` deben permanecer intactos.
- Dos placas propias de desarrollo: COM4 Master (microSD), COM14 Slave (sin microSD), ambas ESP32-D0WD-V3 40MHz y flash físico 4MB. No divulgar MAC. USB y router aislado, sin cargas Q/RS485. El propietario **renunció expresamente** al backup completo solo para estas unidades de desarrollo, con waiver local bajo Documents. Esta renuncia **NO** autoriza futuras escrituras.

## Evidencia A/B/C/D/E/F, sin extrapolar

| Ensayo | Modificación experimental | Resultado |
|---|---|---|
| A | Sketch READY/HEARTBEAT llama `JWPLC_Ethernet.linkStatus()` periódicamente | Reset `rst:0x8 TG1WDT_SYS_RESET` cada ~2s, línea `LINK=` truncada |
| B | Sustituye consultas PHY directas en sketch por `LINK=-9` | ~30s sin WDT, estados IP/DHCP observables |
| C | Única llamada física ~1,5s después de setup | `PHY=1 STAGE=50` (retorno, mutex liberado), ~25s sin WDT |
| D | Consulta desde primer READY con máximo 8 y RTC | Primer intento WDT, `ATTEMPT=1 LAST_STAGE=30 VALID=1` |
| E | W5100.cpp staged con fases 31–39 | Primer intento WDT, `ATTEMPT=1 LAST_STAGE=32 VALID=1` |
| F | Añade 18 fases 60–77 en init() staged | Primer intento WDT, **`ATTEMPT=1 LAST_STAGE=63 VALID=1`** |

H0-F: fuente original `utility/w5100.cpp` dentro de `W5100Class::init()`: después del guard `static bool initialized`, se llama `SPI.begin();` y se registra **63**, luego `initSS();` y se registra **64**. Stage 63 confirma que SPI.begin retornó; no hay stage 64. Intervalo **entre stage 63 y 64**. Esto **no prueba** si fue un mutex, un deadlock Arduino SPI, problema CS, race, scheduling o corrupción de estado.

Log F: boot manual 19:07:16, primer READY en `LINK=` 19:07:17, WDT `rst:0x8` 19:07:18, reinicio con `LAST_STAGE=63` 19:07:19; luego recupera IP `192.168.0.31`, TCP/UDP/RTU reportados activos y `LINK=-9`. El centinela -9 **NO es estado físico real** ni una solución. El firmware desarma consultas tras el primer watchdog mientras RTC conserve estado.

## Código y fuentes a auditar PRIMERO

1. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp`: `W5100Class::init()`, `initSS()`, `resetSS()`, `isW5200()`, `isW5500()`, `softReset()`, PHY y CS.
2. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h`: inline `initSS()`, initSS/SS pin state, macros SPI backend.
3. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp`: `linkStatus()`, `service()`, `probeHardware()`, `acquireBus()`, `releaseBus()`, estado cacheado, DHCP.
4. `JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_spi_bus.cpp` y autoload `peripherals_init.cpp`.
5. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.cpp` y código Ethernet/Modbus TCP/UDP relevante.
6. `tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino`.
7. `tools/alpha13/gates/a13_g10_r_h0_link_stage.py`, `a13_g10_r_h0_w5100_stage.py`, `a13_g10_r_h0_init_stage.py`, `a13_g10_r_build_candidate.py`. Verificar que fases staged corresponden al código **real del objeto compilado**.
8. Docs `docs/v2.1.0-alpha.13/A13_H0_W5100_INIT_STAGE_20261018.md`, `A13_H0_W5100_PHY_STAGE_20261018.md`, `A13_H0_READY_WINDOW_20261017.md`, `A13_H0_LINK_STAGE_TRACE_20261017.md`; histórico y estado Alpha13.
9. Tres fuentes permanentes de contexto del Proyecto si están disponibles: `00_LEER_PRIMERO_JWPLC.md`, `01_PROTOCOLO_CONTINUIDAD_Y_CI.md`, `02_INDICE_DE_FUENTES_Y_MIGRACION.md`. Si faltan, pedirlas, no suponer contenido.

## Preguntas de investigación (no asumir respuesta)

- ¿`initSS()` es realmente una operación bloqueante en este backend ESP32, o contiene un macro `SPI` con comportamiento distinto al supuesto? ¿Es función inline?
- ¿El primer READY llega en un momento en que `W5100.init()` se invoca de manera reentrante o concurrente desde autoload, y el `static bool initialized` protege el estado compartido?
- ¿Puede `SPI.begin()` reinicializar periféricos o locks aunque el wrapper JWPLC haya tomado su mutex? ¿Hay dos locks (JWPLC y SPI core) con distinto orden de adquisición?
- ¿Se ejecuta el task del sketch en el mismo core y prioridad que la lógica Ethernet/autoload? ¿Qué ocurre si se bloquea en `initSS` sin que se alcance la siguiente marca?
- ¿Hay riesgo de que el propio método de instrumentación haya modificado timing o confundido la etapa? Analizar falsos positivos.
- ¿Existe un camino sin nueva lectura PHY que use `isReady()` o `runtimeState()` para telemetría, conservando intacta la semántica de `linkStatus()`?
- ¿Cuál es el cambio **mínimo, fundamentado y reversible** que corrige causa raíz sin deshabilitar TFT/ETH/SD/FRAM/RTC/botonera/RS485/Modbus/TCA ni romper APIs?

## Contrato de trabajo para Claude Code

**Fase 1 obligatoria READ-ONLY.** No cambiar rama, no git checkout/pull/merge, no modificar archivos, no subir ni borrar flashes, no tocar COM4/COM14, no crear PR, no lanzar tests físicos, no modificar Alpha15. Confirmar HEAD/current worktree y revisar rutas anteriores. Emitir:
1. Línea temporal de evidencia A–F con observación vs inferencia.
2. Diagrama del flujo real de llamadas y locks SPI por task/core, con rutas/funciones y referencias de línea.
3. Análisis específico de `initSS()` y razones posibles de STAGE=63.
4. Lista de hipótesis priorizada por evidencia, incluyendo contradicciones y tests para falsarlas.
5. Propuesta de cambio **mínimo** en rama aislada (NO implementarlo aún), con compatibilidad y rollback.
6. Plan de validación no física eficiente (reuso de pruebas válidas, source-first, objetos SHA, CI/guards); después plan físico mínimo bajo autorización separada.
7. Riesgos residuales y criterio exacto de cierre H0/G10-R.

**Fase 2 solo tras aprobación explícita:** abrir rama experimental independiente a partir de commit indicado, implementar fix validable con tests, registrar diff/CI; no cargar placas hasta nueva aprobación. R1–R4 permanecen bloqueados. Prohibido editar `JWPLC/2.1.0` productivo en fase 1; no usar `LINK=-9` como solución. Mantener PRs #109–#114 Draft y documentación en español. Si detecta defecto de diseño en nuestras pruebas o una contradicción, informarlo abiertamente en lugar de forzar una explicación de deadlock.
