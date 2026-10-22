# Alpha13 G9 — Cierre Git productivo A13-012/A13-014

**Fecha:** 2026-10-12.  
**Estado:** `G9_GIT_PRODUCT=CLOSED_PASS`; `G9_FUNCTIONAL=IN_PROGRESS_PHYSICAL_SMOKE_PENDING`.  
**Alpha13:** `IN_PROGRESS`; sin PR, merge, tag ni release.

## Evidencias comprobadas

El operador ejecutó `tools/alpha13/gates/run_a13_g9_git_close.bat`
después de sincronizar la rama
`v2.1.0-alpha.13/feature/cleanup-robustness`
a `4f2bfa47cde3b79d0c8df2e55ed1660f7b9a08dc`.
Resultado de consola:

```text
G9_GIT_UNSTAGED_PATHS=JWPLC_GlobalPeripherals.cpp;JW_SD.cpp;libJW_SD.a
G9_GIT_INDEX_FLAGS=H;H;H
G9_GIT_PREFLIGHT=PASS
G9_GIT_STAGED_AUDIT=PASS
G9_GIT_PRODUCT_COMMIT=63e0291a7b1480af846e955559ab175aa78e7300
G9_GIT_STATUS=PRODUCT_COMMIT_PUSH_PASS_G9_RECONCILIATION_PENDING
G9_GIT_COMMIT_PUSH=PASS
G9_GIT_RUN_ID=g9_git_20261012_102731_5f38ca7a
```

Manifiesto reportado en PC operador:
`tools/alpha13/results/g9_git_20261012_102731_5f38ca7a/`.
No declarar el `MANIFEST.json` local adjunto o publicado:
solo fue reportada su ubicación en consola.

**Verificación independiente GitHub:** el commit
[`63e0291a7b1480af846e955559ab175aa78e7300`](https://github.com/JW-Control/platform-jwplc/commit/63e0291a7b1480af846e955559ab175aa78e7300)
es HEAD remoto de la rama verificada y modifica **exactamente**
las tres rutas esperadas, sin cambios de configuraciones,
autoload, APIs o periféricos ajenos:

```text
JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp
JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp
JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a
```

Diff textual remoto verificado:
- `JW_SD.cpp`: comparación `_path[0]` con literal NUL real
  se expresa como `'\0'`, sin cambiar la condición.
- `JWPLC_GlobalPeripherals.cpp`: elimina exclusivamente
  `g_rtcEnabled` siempre verdadero y dos comprobaciones
  redundantes; mantiene `JWPLC_RTC.begin()`,
  `JWPLC_HAS_RTC`, `rtc == nullptr`, callbacks de RTC y
  `g_buttonRefreshThrottleMs` en **25 ms**, ahora `constexpr`.
- `libJW_SD.a`: archive de biblioteca precompilada
  reemplazado junto a su fuente; SHA-256 evidenciado
  por runner de candidato/adopción/Git del operador:
  `58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e`.
  El API remoto confirma la ruta binaria en el commit,
  **no una descarga o recálculo independiente del SHA binario**.

## Gates completados

| Gate | Evidencia | Resultado |
|---|---|---|
| G9-P0 baseline | `g9_p0_20261011_234245_6727f0b0` | PASS |
| G9-P1 A13-017 doc | README y comentarios actualizados | PASS |
| G9-P2/P3 candidato | `g9_p2p3_20261012_000449_23842b4f` | PASS |
| Archive SD | compilación `JW_SD.cpp.o`, miembro extraído SHA = objeto | PASS |
| Candidato regresiones | BasicReadWrite, Global SD, RTC, Buttons, 4/4 | PASS |
| Adopción producto | `g9_adopt_20261012_101458_1e395a6e`; 3 rutas exactas | PASS |
| Producto regresiones | paquete normal 4/4; `-lJW_SD` y ruta archive | PASS |
| Git close | commit SHA + push y alcance Git remoto | PASS |
| CI contrato candidato | [38205406022](https://github.com/JW-Control/platform-jwplc/actions/runs/38205406022), Windows/Linux | PASS |
| CI adopción | [38209971080](https://github.com/JW-Control/platform-jwplc/actions/runs/38209971080), Windows/Linux | PASS |
| CI cierre Git | [38270082029](https://github.com/JW-Control/platform-jwplc/actions/runs/38270082029), Windows/Linux | PASS |
| Hardware de SD/RTC/botonera **con nuevo producto** | No ejecutado | PENDIENTE |

## Decisión de cobertura y riesgo

**No cerrar aún la parte física de G9.** Los cambios son
semánticamente conservadores y las pruebas anteriores dan
confianza de compilación/enlace/procedencia, pero
`JW_SD` es una biblioteca de periférico con archive
reconstruido. El flujo de release JWPLC recomienda
prueba física cuando se tocan periféricos.

Se requiere un **smoke físico único, acotado, en banco**
con `JWPLC Basic` usando el paquete ya commiteado:
lectura SD sin escritura innecesaria, consulta RTC, y
evento de al menos un botón. Verificar que autoload normal
permanece integrado y que no se activan salidas/relés.
No hay necesidad de repetir el gate Modbus RS-485 G8.
No declarar `PHYSICAL_PASS` sin la salida del equipo.

Si el banco no tiene una microSD instalada, decidir
y registrar `SD_PHYSICAL_BLOCKED_NO_CARD` o diferir
explícitamente la comprobación física a integración
G10 antes de release; **no simular un PASS SD**.

```text
G9_GIT_PRODUCT=CLOSED_PASS_COMMIT_63e0291a
A13-012=SOURCE_ARCHIVE_COMMITTED_BUILD_PASS_PHYSICAL_PENDING
A13-014=GLOBAL_CPP_COMMITTED_BUILD_PASS_PHYSICAL_PENDING
A13-017=CLOSED_PASS_DOC
G9_FUNCTIONAL=IN_PROGRESS_PHYSICAL_SMOKE_PENDING
G9_NEXT=HARDWARE_AVAILABILITY_DECISION_THEN_PHYSICAL_SMOKE
ALPHA13=IN_PROGRESS
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN_PENDING
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```
