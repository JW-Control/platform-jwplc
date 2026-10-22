# Alpha13-G9 — Ejecución de candidatos P2/P3 SD + GlobalPeripherals

**Estado:** `CANDIDATE_RUNNER_PREPARED_CI_CONTRACT_PASS`.  
**Alpha13:** `IN_PROGRESS`. **G9:** `IN_PROGRESS`.

## Evidencia de entrada

El operador pasó G9-P0 (solo lectura) en el Windows real:

```text
G9_P0_RUN_ID=g9_p0_20261011_234245_6727f0b0
G9_P0_HEAD=93cbd1e7e04ac4f5aedac95bf35fa29453733c38
G9_P0_SOURCE_SD_NUL_COUNT=1
G9_P0_SOURCE_SD_NUL_OFFSETS=[28732]
G9_P0_SD_ARCHIVE_SHA256=1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
G9_P0_GLOBAL_RTC_REFS=3
G9_P0_GLOBAL_THROTTLE_REFS=2
G9_P0_README_OUTDATED_ALPHA11=False
G9_P0_STATUS=BASELINE_CAPTURED_READ_ONLY
```

El cliente Windows también mantiene el manifiesto en
`tools/alpha13/results/g9_p0_20261011_234245_6727f0b0/MANIFEST.json`.
El nuevo gate exige comprobar **ese manifiesto exacto**, los
hashes de las fuentes y archive iniciales y la ascendencia
Git del HEAD P0 antes de construir.

## Cambios candidatos controlados

**A13-012 — SD:** cambiar solo el carácter NUL incrustado
dentro de un literal de `JW_SD.cpp` por `'\0'`
textual; validar reversibilidad **en el offset original**
y diferencia de +1 byte. No cambiar DataLog, nombres
ni parámetros públicos. `JW_SD` usa `precompiled=full`,
por lo que se recompila el objeto real y se crea un
`libJW_SD.a` candidato desde ese objeto, con extracción
y paridad del miembro.

**A13-014 — GlobalPeripherals:** RTC sigue activado
cuando `JWPLC_HAS_RTC=1`; eliminar el flag interno
`g_rtcEnabled` que siempre era true y el guard
inalcanzable. Mantener el guard `rtc==nullptr`.
Conservar el refresh throttle de exactamente **25 ms**,
expresándolo como `constexpr`. No alterar
`JWPLC_RTC.begin()`, `jwplcSystemForceDisplayRefresh()`,
`JWPLC_SD.serviceDataLogs()`, API, autoload ni
acceso a otros periféricos.

Para este archivo el generador respeta `CRLF` de
Windows y `LF` de Linux, sin normalizar el resto
del contenido. La librería GlobalPeripherals **no es
precompilada**; validar su objeto `.cpp.o` desde fuente.

## Pruebas

CI de transformaciones exactas, sintaxis y no mutación
de checkout: [38205406022](https://github.com/JW-Control/platform-jwplc/actions/runs/38205406022),
**PASS Windows y Linux**. Las incidencias de
portabilidad de la herramienta F125-F127 están
registradas y cerradas en FAILURES. Esto no equivale
todavía a compilar Arduino en el Windows del operador.

**Gate oficial de build de candidato:**

```powershell
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g9_p2p3_candidate.bat
```

El ejecutor:

1. Verifica `arduino-cli.exe` 1.5.1 y su SHA,
   FQBN `jwplc_local:esp32:jwplcbasic`, Git limpio,
   los archives congelados (Core/TFT/FRAM/RTU/SD)
   y el P0 local exacto.
2. Genera candidatos aislados en `tools/alpha13/results`
   y bibliotecas temporales en `%TEMP%`, sin
   editar `JWPLC/2.1.0/libraries`.
3. Construye `JW_SD.cpp.o` de fuente real,
   junto con `JWPLC_GlobalPeripherals.cpp.o`
   candidato, preservando lib discovery.
4. Crea archive SD candidato usando el mismo
   toolchain y verifica miembro extraído =
   objeto compilado (SHA-256).
5. Compila desde el paquete normal con el archive
   SD candidato y GlobalPeripherals candidata,
   sin overlay de otros periféricos, los ejemplos
   **BasicReadWrite, microSD, RTC y Buttons**.
   Comprueba `-lJW_SD`, ruta exacta del archive
   y objeto GlobalPeripherals desde fuente.
6. Audita todos los hashes originales, ruta de
   selección de librerías, Git sin cambios,
   resultado y archivos de log.

**Resultado esperado solo si todos pasan:**

```text
G9_CANDIDATE_PHASE=PREFLIGHT_PASS
G9_CANDIDATE_PHASE=SOURCE_CONTRACT_PASS
G9_CANDIDATE_PHASE=SOURCE_OVERLAY_PASS
G9_CANDIDATE_PHASE=SOURCE_BUILD_PASS
G9_CANDIDATE_ARCHIVE_MEMBER_BYTE_PARITY=PASS
G9_CANDIDATE_PHASE=ARCHIVE_REBUILD_PASS
G9_CANDIDATE_PHASE=CANDIDATE_REGRESSIONS_PASS
G9_CANDIDATE_PHASE=FINAL_AUDIT_PASS
G9_CANDIDATE_STATUS=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G9_CANDIDATE_PRODUCT_CHANGE=NONE
G9_CANDIDATE_HARDWARE_ACCESS=NONE
```

Si aparece `REVIEW_STOP`, no se realiza adopción productiva,
Git commit/push ni upload. Corregir solamente la fase fallida,
sin descartar la evidencia del P0.

## Próximo paso DESPUÉS del candidato PASS

Preparar por separado la adopción controlada de
`JW_SD.cpp`, `libJW_SD.a` y
`JWPLC_GlobalPeripherals.cpp` con backup,
rollback, regressions productivas, auditoría de
scope exacto y cierre Git. No modificar productos
hasta validar candidatos; **ningún gate de G9
contempla upload en esta etapa**.

```text
G9_P0=CLOSED_PASS_READ_ONLY
G9_P1=A13-017_DOC_ONLY_PASS
G9_P2=A13-012_CANDIDATE_NOT_YET_BUILD_ON_WINDOWS
G9_P3=A13-014_CANDIDATE_NOT_YET_BUILD_ON_WINDOWS
G9_FINAL=OPEN
G10=PENDING
RELEASE_PR_MERGE=NO
```
