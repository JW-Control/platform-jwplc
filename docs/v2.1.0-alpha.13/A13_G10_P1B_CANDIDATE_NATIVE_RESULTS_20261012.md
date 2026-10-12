# Alpha13 G10-P1B — Candidato A13-006, resultado nativo

**Fecha:** 2026-10-12. **Estado:** `CANDIDATE_NATIVE_CI_PASS_PRODUCT_UNCHANGED`.

## Evidencia de entrada

```text
G10_P0_LOCAL_RUN=g10_p0_20261012_114921_34102c6b
G10_P0_STATUS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P1A_LOCAL_RUN=g10_p1a_20261012_120529_a174356e
G10_P1A_LOCAL_STATUS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
G10_P1A_CI=PASS_WIN_LINUX_RUN_38280631554
G10_P1A_BASELINE=DEFECT_REPRODUCED_NOT_FIXED
```

El baseline nativo A13-006 reprodujo la pérdida de
identificador al fallar el lock SPI, incluyendo
`JWPLC_ModbusTCPClientClass::closeSession()` y
`JWPLC_ModbusTCPClass::dropClient()`.

## Candidato controlado

Generador:
`tools/alpha13/gates/a13_g10_p1b_a13_006_candidate.py`.

Se generan **solo copias en**
`tools/alpha13/results/g10_p1b_*/candidate_sources/`
de los siguientes cuatro archivos:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h
```

En cliente, un fallo del lock SPI preserva el
`_tcpClient` y marca `_closePending` privado.
`task()` atiende el pendiente antes del
`busy()` y `begin()`/requests no pueden
avanzar mientras el socket anterior siga
pendiente de liberación. `end()` conserva su
firma void y no promete cierre síncrono si SPI
está ocupado.

En servidor, un fallo SPI deja `_client` y
`_clientActive` preservados, registra
`_dropPending`, no ejecuta stop ni INT
sin lock y atiende el cierre antes de
volver a aceptar clientes. `clientConnected()`
no afirma que la sesión pendiente siga operativa.

Se mantienen las APIs públicas, la selección normal
de librerías, el autoload y los periféricos. No se
modifica `EthernetClient`, el archive de SD ni el
core.

## CI

[GitHub Actions 38282694249](https://github.com/JW-Control/platform-jwplc/actions/runs/38282694249):
**PASS en Windows y Linux**.

La prueba utiliza los cuerpos `closeSession()` y
`dropClient()` extraídos de los candidatos, compilados
contra mocks nativos de `EthernetClient` y SPI.
Comprueba ocho marcadores de cobertura de cierre
en el cliente y servidor, más el estado final:

```text
G10_P1B_SOURCE_CONTRACT=PASS_FOUR_PRIVATE_CANDIDATES
G10_P1B_NATIVE=PASS_EIGHT_MARKERS_SOURCE_BACKED
G10_P1B_STATUS=CANDIDATE_NATIVE_PASS_PRODUCT_UNCHANGED
```

No equivale a compilación Arduino completa ni a prueba
W5500 física. Es crucial probar la ruta `task()`, el
intento de nueva solicitud, `begin/end`, reset de Ethernet,
eventos RX INT y prevención de reutilización del número
de socket bajo operación real. El reset del W5500 puede
invalidar el handle retenido; la política de recuperación
requiere validación adicional antes de adoptar el cambio.

## Siguiente puerta

Operador: gate fuente de solo lectura en su Windows,
con manifiestos locales G10-P0/P1A. **Todavía no
solicitar flash**.

```powershell
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1b_a13_006_candidate.bat
```

Esperado:

```text
G10_P1B_SOURCE_CONTRACT=PASS_FOUR_PRIVATE_CANDIDATES
G10_P1B_STATUS=CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
```

Luego se debe preparar un gate separado de build
real Arduino CLI, con overlay de la librería
`JWPLC_ModbusTCP`, compilación de ejemplos cliente
y servidor, source objects comprobados, archives
externos inmutables y tests de integración negativos.

```text
A13-006=ADOPT_CONFIRMED_REPRO_FIX_PENDING
G10_P1A=SOURCE_CONTRACT_LOCAL_PASS_NATIVE_CI_PASS
G10_P1B=CI_NATIVE_PASS_WINDOWS_LINUX
G10_P1B_LOCAL=NOT_YET_RUN
G10_P1B_PRODUCT_COMMIT=NONE
G10_P1B_PHYSICAL=NOT_EXECUTED
G10_P1C_ARDUINO_OVERLAY_BUILD=NOT_STARTED
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
G10_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_MERGE_PR_PUBLICATION=NO
```
