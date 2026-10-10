# Alpha13 — TFT-CLOSURE — ejecutor integrador propuesto

> **Actualización tras ejecución (20261009_220034_167fa2ad):** este documento conserva el plan inicial como historial. La ejecución física, regresiones y rebuild dieron PASS, confirmado por el operador. Consultar [cierre final](A13_TFT_CLOSURE_20261009.md).


Fecha: 2026-10-09. Estado: `PREPARADO_LOCALMENTE_NO_VERSIONADO_NO_EJECUTADO`.

## Fuente de verdad

- Rama: `v2.1.0-alpha.13/feature/cleanup-robustness`.
- HEAD remoto consultado: `4cc7b8d3bae07071accea2058f88e28c506166b9` (documental).
- HEAD local durante P2A: `ac69e41922f512c280ad116bcf79b24596befa02`.
- Hito antecedente: `TFT-PRE6-P2A=CLOSED_PASS` con cuatro compilaciones, **sin upload** y tres archivos productivos **sin commit**.
- Reglas: `docs/JWPLC_COLLABORATION_WORKFLOW.md` y `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md`.

## Archivos del ejecutor

- `tools/alpha13/gates/run_a13_tft_closure.bat` — acceso desde Windows (requiere pwsh 7, Python).
- `tools/alpha13/gates/a13_tft_closure.py` — pipeline con logs y criterios de parada.

Estos dos archivos y el presente plan **no modifican ningún archivo productivo** y todavía no se han integrado en GitHub. No se autoriza commit sin revisión. No repetir PRE5/P0/P1A/P1B/P2A.

## Uso

Extraer ZIP **desde la raíz de `platform-jwplc`** sin tocar los tres archivos modificados. Cerrar monitores serie y conectar el JWPLC.

```powershell
.\tools\alpha13\gates\run_a13_tft_closure.bat --serial-port COM4
```

`COM4` es sólo el último puerto conocido; cambiarlo por el que Windows realmente presente, o quitar el parámetro si existe un solo candidato distinto de COM1.

Si la copia original de TFT_eSPI 2.5.43 no está en `~/Documentos/Programacion/Arduino/libraries/TFT_eSPI`, especificar también `--backend "RUTA_VERIFICADA"`.

## Orden interno

1. **PREFLIGHT**: rama, HEAD ancestro PRE5, tres cambios locales exactos, staged vacío, `git diff --check`, SHA de los tres archivos, `core.a`, `Display.a`, y backup original P2A byte a byte. Ningún pull, reset, checkout, stage ni commit.
2. **TOOLCHAIN/PORT**: Arduino CLI 1.0.2 (baseline de estos gates), pyserial, FQBN `jwplc_local:esp32:jwplcbasic`, COM explícito presente o único candidato seguro.
3. **BUILD_PHYSICAL**: copiar temporalmente el sketch PRE1, inyectar `TFT_CLOSURE_RUN_ID` aleatorio y el SHA del archive productivo; compilar de forma *normal* (TFT precompilada, Display precompilada, core stub+archive; 0 `.o` de fuentes TFT, 0 selección de TFT_eSPI global). Guardar SHA256 del `*.ino.bin`.
4. **PHYSICAL_UPLOAD + SERIAL**: upload al puerto verificado, captura serial de `DISPLAY_READY=YES`, `IO_READY=YES`, estado GPIO TFT y marcador exacto para evitar falso PASS de firmware antiguo.
5. **OBSERVACIÓN HUMANA**: USB-only y power-cycle seguro. Registrar resultado del usuario, capturas/video externos y verificar nuevamente el marcador serial. La confirmación textual en consola es *provisional* hasta revisión de video/capturas.
6. **REGRESSION**: consumidores `01.Display_IDLE_Status`, `02.Display_HMI_Fields`, `Display_Idle_Return_Modes` y `JWPLC_LogicRuntime_UI_Home` usando el archive normal; todos los periféricos autoload intactos. No introducir mejoras HMI Designer ni nuevas funciones TFT.
7. **REBUILD_RECIPE**: comenzar directamente de `JWPLC_TFT.cpp` y `tft_setup.h` **ya parcheados**. Verificar hashes, clonar TFT_eSPI 2.5.43 a TEMP, parchear ST7789_Init en TEMP, verificar el guard compilado del backend, recompilar dos objetos, crear/examinar `libJWPLC_TFT.a` temporal, exigir paridad byte a byte de sus miembros y recompilar un consumidor normal seleccionando el nuevo archive. La regeneración no vuelve a transformar el fuente productivo.
8. **COMMIT_PLAN**: sólo genera `COMMIT_PLAN.md`. No ejecuta `git add`, `git commit`, `git pull`, `git reset`, `git checkout`, `git clean`, push ni publicación.

## Resultados, evidencia y bloqueo

- Logs de cada compilación, upload, serial, archiver y resumen: `tools/alpha13/results/tft_closure_<runid>/` (ignorado por Git).
- Build y reconstrucción: `%TEMP%/jwplc_a13_tft_closure_<runid>/`.
- `SUMMARY.log`, `MANIFEST.json` (binario identificado, SHA exactos, fase/errores, respuestas humanas) y `COMMIT_PLAN.md`.
- Un error detiene el pipeline preservando los tres cambios locales; **no realiza rollback automático de un P2A que ya pasó**, para evitar borrar evidencia/código con una acción no autorizada.
- Categorías: `PRODUCT`, `HARNESS`, `HARDWARE`, `ENVIRONMENT`, `PRECONDITION` o combinación explícita cuando aún falta atribución.
- `STATUS=PROVISIONAL_PASS_AWAITING_VIDEO_AND_COMMIT_AUTHORIZATION` significa pruebas informáticas y reporte humano favorables, **no** TFT-CLOSURE cerrado.
- Resultado final requiere video/capturas revisadas, receta reconstruida, revisión de logs, checklist/estado actualizados y autorización separada del commit.

## Estado canónico tras preparación (NO confundir con PASS físico)

```text
HITO=TFT-CLOSURE
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
HEAD_REMOTO=4cc7b8d3bae07071accea2058f88e28c506166b9
WORKTREE_LOCAL=3_EXPECTED_TRACKED_PRODUCT_FILES_DIRTY_P2A
EVIDENCIA_PASS=P2A_CLOSED_PASS_4_BUILDS
BLOQUEOS=P2B_NOT_EXECUTED;P3_REGRESSION_NOT_EXECUTED;REBUILD_FROM_PATCHED_SOURCES_NOT_EXECUTED
SIGUIENTE_ACCION=RUN_INTEGRATED_TFT_CLOSURE_EXECUTOR
ACCION_PROHIBIDA=GIT_PULL_RESET_CHECKOUT_CLEAN_WITHOUT_PRESERVATION;AUTO_COMMIT
REQUIERE_CONFIRMACION_USUARIO=USB_ONLY_VISUAL_AND_VIDEO_THEN_COMMIT
```
