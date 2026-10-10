# Alpha13-G4 — Propiedad de tarea del batch TFT

Actualizado: 2026-10-10. Estado: **CANDIDATE_AND_GATE_VERSIONED_NOT_EXECUTED**.

## Hallazgo

`JWPLC_TFTClass::_batchActive` es un booleano global; en `beginBatch()` y `acquireForOperation()` basta ese flag para saltar el mutex SPI, sin identificar la tarea propietaria. `endBatch()` puede dar `g_backend.endWrite()` y `jwplcSPI_release()` desde una tarea ajena. El mutex global del bus es **no recursivo** y protege a todos los periféricos SPI. Una tarea ajena jamás debe saltar el mutex ni liberarlo.

El hallazgo se deriva del código. No se declaró fallo físico previo ni se reproduce el error cargando la versión vulnerable.

## Candidato conservador

Dos fuentes nuevas, en `tools/alpha13/candidates/g4/` (fuera del producto):

- `JWPLC_TFT.h`: sustituye el flag privado por `std::atomic<void *> _batchOwner`; no se altera firma pública ni colores.
- `JWPLC_TFT.cpp`: identifica al propietario mediante `xTaskGetCurrentTaskHandle()`; el mismo propietario puede llamar repetidamente a `beginBatch` sin segunda adquisición (semántica idempotente existente), pero una tarea distinta espera el mutex compartido; `endBatch` ajeno no hace nada y operaciones sueltas deben adquirir SPI si son de otra tarea.

Se preservan los bytes del setup privado `tft_setup.h` y la corrección del ST7789 que limpia GRAM antes de DISPON. No se toca `core.a`, `Display.a`, `jwplc_spi_bus.cpp` ni los demás periféricos.

## Un solo gate integrado

`tools/alpha13/gates/run_a13_g4_integrated.bat --serial-port COM4` orquesta:

1. Preflight de rama/HEAD/sha y código candidato (sin producto sucio).
2. Copia del backend externo TFT_eSPI **2.5.43** en `%TEMP%` y validación de SHA; parches de mantenimiento, sin modificar la instalación.
3. Compilación source-first temporal de dos miembros (`JWPLC_TFT.cpp.o`, `TFT_eSPI.cpp.o`); creación y extracción de `libJWPLC_TFT.a` con paridad de miembros.
4. Enlace normal del archive temporal sin seleccionar TFT_eSPI externa ni recompilar los fuentes TFT durante el sketch.
5. Compilación/flash solo tras confirmación `BANCO`, con prueba FreeRTOS de dos tareas y token único: batch del propietario, denegación a tercero, `endBatch` no autorizado inofensivo, prohibición de draw ajeno, liberación y readquisición; 50 ciclos, cero errores.
6. Confirmación visual del operador de que no hay fondo blanco/flicker y TFT permanece estable.
7. **Solo después** adopción local reversible de 3 archivos: `JWPLC_TFT.cpp`, `JWPLC_TFT.h`, `libJWPLC_TFT.a`. Respaldo en `%TEMP%` antes de escribir.
8. Cuatro regresiones normales con el archive productivo adoptado y auditoría Git/hashes; conservar cambio **sin commit** para cierre separado.

Si falla alguna fase se detiene y restaura los tres archivos tras haberlos adoptado. **Un upload ya realizado no se revierte automáticamente en el equipo físico.** El gate guarda `SUMMARY.log`, `MANIFEST.json`, serial, logs y hashes. Modo `--skip-physical`: prueba solo reconstrucción temporal; no adopta ni cierra G4.

El test no conmuta salidas de relé, pero sí reemplaza el sketch actual. Debe ejecutarse con JWPLC en banco sin cargas/actuadores conectados, nunca controlando una máquina real. La observación humana no se simula.

## Resultado esperado

```text
STATUS=PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE
PRODUCT_DIRTY_FILES=3
NEW_TFT_ARCHIVE_SHA256=<hash calculado en el equipo>
NEXT_GATE=A13_G4_CLOSURE_AFTER_REVIEW
```

No realizar `git pull/reset/checkout/clean` sobre el producto adoptado después de PASS. El hito se cerrará mediante un commit productivo posterior, con autorización, más checklist/documentación. No abrir gates cerrados G1/G2/G3/TFT-CLOSURE.

`OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`, `TFT_NEW_FEATURES=OUT_OF_SCOPE`.
