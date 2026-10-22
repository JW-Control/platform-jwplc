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

## Incidencia R1 y recuperación segura del backend (2026-10-10)

Primera ejecución: `PREFLIGHT=PASS`, después
`BACKEND_RECIPE=REVIEW` por
`BACKEND_SHA_MISMATCH:TFT_Drivers/ST7789_Init.h`.
No se ejecutaron compilación, upload, validación física ni adopción de producto.
El SHA actual de la instalación local **todavía no se conoce** a partir de la
consola compartida. No asumir qué cambio produjo el desacuerdo.

La identidad original pinneada
`e21cae2ac84285dc0e77648eca753f41f7da3c271594ede750b725136c10`
fue contrastada contra el fichero del tag oficial
`Bodmer/TFT_eSPI V2.5.43`, ruta `TFT_Drivers/ST7789_Init.h`.
La identidad parcheada de mantenimiento se mantiene fija:
`44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc`.

El ejecutor actualizado:
- informa SHA local, identidad conocida, presencia de guard y anclas;
- admite únicamente las dos identidades pinneadas, con normalización CRLF/LF verificada;
- cuando el init local difiere, descarga del tag oficial **solo a memoria** y
  lo emplea exclusivamente en la copia temporal si su SHA es el original exacto;
- preserva el `TFT_eSPI` local, exige los SHA exactos de `TFT_eSPI.cpp/.h`,
  y verifica que la copia temporal final tiene el SHA parcheado esperado;
- si la fuente oficial no está disponible o el SHA difiere, detiene G4 con
  `BACKEND_INIT_UNRECOGNIZED_PINNED_VARIANTS_STOP` y evidencia diagnóstica;
  no adopta ningún archivo productivo.

Esto es una recuperación delimitada del entorno de mantenimiento, **no una
relajación de la seguridad del firmware**. No se reabren G1, G2, G3 ni
TFT-CLOSURE. No se crea un nuevo F-ID sin causa raíz corroborada.

## Resultado esperado

```text
STATUS=PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE
PRODUCT_DIRTY_FILES=3
NEW_TFT_ARCHIVE_SHA256=<hash calculado en el equipo>
NEXT_GATE=A13_G4_CLOSURE_AFTER_REVIEW
```

No realizar `git pull/reset/checkout/clean` sobre el producto adoptado después de PASS. El hito se cerrará mediante un commit productivo posterior, con autorización, más checklist/documentación. No abrir gates cerrados G1/G2/G3/TFT-CLOSURE.

`OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`, `TFT_NEW_FEATURES=OUT_OF_SCOPE`.

### Segunda ejecución y corrección F111 (2026-10-10)

El intento R2 encontró `BACKEND_INIT_SHA256_ACTUAL` y `BACKEND_UPSTREAM_SHA256_ACTUAL` iguales a `e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10`, que es el mismo valor pinneado en TFT-CLOSURE. **La constante de G4 estaba truncada; no existía drift del backend local**. El ejecutor ahora copia el hash correcto y compara los dos pins del init con el gate TFT histórico antes de ejecutar la receta. Se registra `F111=HARNESS`; el próximo fallo a numerar será F112. No hay firmware compilado/subido ni cambios productivos por estos intentos.
