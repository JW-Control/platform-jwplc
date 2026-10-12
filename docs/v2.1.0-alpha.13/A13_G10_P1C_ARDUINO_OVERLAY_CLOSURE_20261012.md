# Alpha13 G10-P1C — Cierre de compilación real Arduino CLI (2026-10-12)

**Estado:** `CLOSED_PASS_BUILD_ONLY_PRODUCT_UNCHANGED`.  
**Alpha13:** `IN_PROGRESS`. **A13-006:** `FIX_CANDIDATE_NOT_ADOPTED`.

## Evidencia reportada por el operador en Windows

Rama `v2.1.0-alpha.13/feature/cleanup-robustness`, HEAD local
`802c6260`, Arduino CLI instalado en entorno del operador.
Dos ejecuciones del gate corregido aparecen en consola:

1. Un intento interrumpido luego de `BASELINE_SERVER` y
   `BASELINE_CLIENT` con compilaciones EXIT 0. Se informó
   `History restored` y no hay marcador final: **no
   considerarlo un PASS integral**.
2. Una segunda ejecución **completa y aprobada**:

```text
G10_P1C_PHASE=PREFLIGHT_PASS
G10_P1C_PHASE=OVERLAY_PASS
G10_P1C_COMPILE_BASELINE_SERVER_EXIT=0
G10_P1C_SOURCE_PROVENANCE_BASELINE_SERVER=PASS_OBJECT_DEPENDENCY_MMD
G10_P1C_BASELINE_SERVER=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_COMPILE_BASELINE_CLIENT_EXIT=0
G10_P1C_SOURCE_PROVENANCE_BASELINE_CLIENT=PASS_OBJECT_DEPENDENCY_MMD
G10_P1C_BASELINE_CLIENT=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_COMPILE_CANDIDATE_SERVER_EXIT=0
G10_P1C_SOURCE_PROVENANCE_CANDIDATE_SERVER=PASS_OBJECT_DEPENDENCY_MMD
G10_P1C_CANDIDATE_SERVER=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_COMPILE_CANDIDATE_CLIENT_EXIT=0
G10_P1C_SOURCE_PROVENANCE_CANDIDATE_CLIENT=PASS_OBJECT_DEPENDENCY_MMD
G10_P1C_CANDIDATE_CLIENT=PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS
G10_P1C_PHASE=ARDUINO_BUILDS_PASS
G10_P1C_PHASE=FINAL_AUDIT_PASS
G10_P1C_STATUS=ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED
```

**Run ID exitoso:** `g10_p1c_20261012_140152_a7211c08`.
Carpeta indicada por operador:
`tools/alpha13/results/g10_p1c_20261012_140152_a7211c08/`
(los logs locales no fueron descargados por esta sesión).
El gate también comprobó hashes de cinco archives congelados,
librería efectiva y sus dependencias GCC `-MMD`, dos
objetos C++ por sketch, Git limpio e integridad del overlay.

## Matriz de cumplimiento

| Ejemplo | Librería efectiva | Compilación | Procedencia objeto |
|---|---|---|---|
| `01.ModbusTCP_Server` | Original | PASS | PASS `.o/.d` |
| `02.ModbusTCP_Client` | Original | PASS | PASS `.o/.d` |
| `01.ModbusTCP_Server` | Overlay candidato | PASS | PASS `.o/.d` |
| `02.ModbusTCP_Client` | Overlay candidato | PASS | PASS `.o/.d` |

**F129:** clasificado y corregido como falso negativo del
harness anterior que dependía del texto verbose. La CI
[38293904677](https://github.com/JW-Control/platform-jwplc/actions/runs/38293904677)
pasó Windows y Linux; con este segundo run local
queda `F129=RESOLVED_CI_AND_LOCAL_RETEST`.

## Limitaciones y condiciones de avance

Este PASS demuestra **compatibilidad de compilación y
enlace Arduino Basic**, no seguridad funcional ante SPI
denegado repetidamente, reset W5500, dirección de
reapertura, end/begin, socket reusado o recuperación
conexión. Los mocks P1B actuales ejecutan únicamente
los cuerpos de `closeSession()` y `dropClient()`;
no prueban realmente `task()`, `begin()`,
`startRequest()` ni `startWriteMultipleRequest()`.

**No adoptar el candidato** mientras esos caminos no
se prueben con negativos fiables y revisión de
ciclo de vida. Especial atención a:

- `_closePending` puede sobrevivir a un cambio
  `Ethernet.isReady()==false`; al volver READY,
  llamar `cancelConnectAsync()` sobre el mismo
  número de socket podría afectar un socket
  reinicializado/reasignado. Debe existir contrato
  explícito de invalidación por generación Ethernet.
- `_dropPending` servidor tiene la misma tensión
  entre retener ownership durante contención y
  soltar handles tras reset.
- Cierre diferido exige continuidad de `task()/poll()`
  de acuerdo con el runtime; no prometer cierre
  automático si la aplicación no lo llama.
- Prueba física con W5500 real debe planificarse
  por separado si se adopta implementación TCP;
  COM4/COM14 conservan diagnósticos G9/G8
  según última evidencia.

```text
G10_P1C=PASS_4_OF_4_ARDUINO_BUILD_AND_PROVENANCE
F129=RESOLVED_CI_AND_LOCAL_RETEST
A13-006=ADOPT_CONFIRMED_CANDIDATE_NOT_ADOPTED
G10_NEXT=P1D_NEGATIVE_LIFECYCLE_AND_ETHERNET_RESET_TESTS
G10_A13-007=REVIEW_OPEN
G10_A13-008=REVIEW_OPEN
G10_A13-015_016_018=ADOPT_PENDING
G10_PRODUCT_COMMIT=NONE
G10_PHYSICAL=NONE
G10_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_PR_MERGE=NO
```
