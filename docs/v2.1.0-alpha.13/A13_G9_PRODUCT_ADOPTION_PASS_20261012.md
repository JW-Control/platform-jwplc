# Alpha13 G9 — Adopción productiva local PASS (2026-10-12)

**Gate:** `G9-P2/P3=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT`.  
**G9:** `IN_PROGRESS`; **Alpha13:** `IN_PROGRESS`.  
**Ámbito:** A13-012 (SD NUL) y A13-014 (GlobalPeripherals RTC/botonera).

## Evidencia del operador

El operador ejecutó el runner versionado `tools/alpha13/gates/run_a13_g9_adopt.bat`
en Windows tras `git pull --ff-only` hasta el HEAD
`54332580e560ddc0986027f782c7dfcf456c0bd0`.

```text
G9_ADOPT_RUN_ID=g9_adopt_20261012_101458_1e395a6e
G9_ADOPT_PHASE=PREFLIGHT_PASS
G9_ADOPT_INDEX_FLAGS=H_H_H
G9_ADOPT_PHASE=BACKUP_PASS
G9_ADOPT_ADOPTION_DIFF_PATHS=EXACT_THREE_PRODUCT_PATHS
G9_ADOPT_ADOPTION_BYTES_CHANGED=TRUE_TRUE_TRUE
G9_ADOPT_PHASE=PRODUCT_ADOPTION_PASS
G9_ADOPT_COMPILE_PRODUCT_SD_BASIC_EXIT=0
G9_ADOPT_ARCHIVE_LINK_SD_BASIC=PASS_EXACT_PATH_FLAG_SHA
G9_ADOPT_REGRESSION_SD_BASIC=PASS
G9_ADOPT_COMPILE_PRODUCT_GLOBAL_SD_EXIT=0
G9_ADOPT_ARCHIVE_LINK_GLOBAL_SD=PASS_EXACT_PATH_FLAG_SHA
G9_ADOPT_REGRESSION_GLOBAL_SD=PASS
G9_ADOPT_COMPILE_PRODUCT_GLOBAL_RTC_EXIT=0
G9_ADOPT_ARCHIVE_LINK_GLOBAL_RTC=PASS_EXACT_PATH_FLAG_SHA
G9_ADOPT_REGRESSION_GLOBAL_RTC=PASS
G9_ADOPT_COMPILE_PRODUCT_GLOBAL_BUTTONS_EXIT=0
G9_ADOPT_ARCHIVE_LINK_GLOBAL_BUTTONS=PASS_EXACT_PATH_FLAG_SHA
G9_ADOPT_REGRESSION_GLOBAL_BUTTONS=PASS
G9_ADOPT_PHASE=PRODUCT_REGRESSIONS_PASS
G9_ADOPT_FINAL_DIFF_PATHS=EXACT_THREE_PRODUCT_PATHS
G9_ADOPT_FINAL_BYTES_CHANGED=TRUE_TRUE_TRUE
G9_ADOPT_PHASE=FINAL_AUDIT_PASS
G9_ADOPT_STATUS=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT
G9_ADOPT_ARCHIVE_SHA256=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_ADOPT_GIT_COMMIT_OR_PUSH=NONE
G9_ADOPT_HARDWARE_ACCESS=NONE
```

## Rutas productivas exactas adoptadas, todavía SIN commit

```text
JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp
JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a
JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp
```

- A13-012: reemplazo de un NUL real por `'\0'` textual en
  fuente JW_SD, sin cambios de API ni otras operaciones SD.
  Por `precompiled=full`, se acompaña del archive compilado
  desde ese objeto, con correspondencia SHA verificada.
- A13-014: eliminación de flag privado de RTC siempre true;
  `JWPLC_HAS_RTC` y el guard de puntero nulo continúan
  activos; throttle de refresco de la botonera preservado
  en 25 ms como constante. Autoload normal intacto.
- Todos los cambios fueron primero candidatos aislados;
  las cuatro regresiones de package productivo pasaron
  **sin overrides** y comprobaron el `-lJW_SD`,
  la ruta efectiva y el hash del archive nuevo.

Backup local del operador:
`C:\Users\jeykc\AppData\Local\Temp\jwplc_a13_g9_adopt_20261012_101458_1e395a6e\originals`.

Resultados/manifiesto del operador:
`tools/alpha13/results/g9_adopt_20261012_101458_1e395a6e`.
**Ni backup ni manifiesto local están garantizados en GitHub.**
El gate Git de cierre exigirá esas evidencias locales;
no reproducir cambios manualmente si falta alguna.

## Cierre Git posterior, separado

Herramientas:
```text
tools/alpha13/gates/a13_g9_git_close.py
tools/alpha13/gates/a13_g9_git_close_selftest.py
tools/alpha13/gates/run_a13_g9_git_close.bat
.github/workflows/alpha13-g9-git-close.yml
```

El cierre Git no recompila ni rehace la adopción: compara manifests
P0/candidato/adopción, el backup original con HEAD previo,
los hashes fuente/archive, el índice y exactamente las
tres diferencias sin staging previo. Después audita los tres
blobs staged y el commit, haciendo push solo de esos tres archivos.

No ejecutar stage/commit manual, reset, rebase ni una segunda
adopción. La sincronización fast-forward de archivos de
herramienta/documentación es segura únicamente si el remoto
no modificó ninguna de las tres rutas productivas y el gate
valida todos sus invariantes.

## Límites del resultado

No se ha ejecutado prueba física microSD/RTC/botonera
con el nuevo firmware: se trata de un **PASS de compilación,
enlace y equivalencia de cambios controlados**. Los JWPLC
usados en G8 tienen sketches de diagnóstico cargados; este
gate no los reprogramó ni restauró aplicaciones. No
se ha hecho PR/merge/release. La deuda de cobertura
G10 y la causa F120 siguen pendientes.

```text
A13-012=PRODUCT_ADOPTED_BUILD_PASS_PENDING_COMMIT
A13-014=PRODUCT_ADOPTED_BUILD_PASS_PENDING_COMMIT
A13-017=DOC_ONLY_CLOSED_PASS
G9_CANDIDATE=CLOSED_PASS
G9_PRODUCT_ADOPTION=PASS_LOCAL
G9_PRODUCT_GIT=NOT_COMMITTED
G9_PHYSICAL=NOT_EXECUTED
G9_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_PR_MERGE=NO
```
