## P1E — CI readonly Windows/Linux PASS (2026-10-12)

El [run 38297416611](https://github.com/JW-Control/platform-jwplc/actions/runs/38297416611)
terminó **PASS Windows y Linux**, sin modificar código
productivo ni acceder al hardware. El snapshot examinó
las rutas de reset físico, `isReady()`, enlace,
`W5100.init()` y los dos consumidores Modbus TCP.
Los diez patrones de fuente y el caso negativo
de contrato se confirmaron; cinco archives permanecieron
con SHA-256 congelado.

```text
G10_P1E_CI=PASS_WINDOWS_LINUX_RUN_38297416611
G10_P1E_SELFTEST=PASS_READONLY_TEN_PATTERNS_AND_NEGATIVE
G10_P1E_RUNTIME_GENERATION_NOT_EXPOSED=True
G10_P1E_PROBE_CALLS_PHYSICAL_RESET=True
G10_P1E_BACKEND_INIT_STATIC_GUARD=True
G10_P1E_LINK_OFF_CAN_CLEAR_READY=True
G10_P1E_ARCHIVES_FROZEN=5
G10_P1E_CI_STATUS=RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1E_LOCAL_WINDOWS=NOT_YET_EXECUTED
G10_P1E_PRODUCT_ADOPTION=NO
```

El PASS representa una inspección de procedencia:
el diseño correctivo **todavía no existe**.
El script local comprobará además la cadena de
manifiestos P1C/P1D conservada por el operador.
Su BAT es
`tools/alpha13/gates/run_a13_g10_p1e_ethernet_generation_readonly.bat`.
No compila ni sube firmware.

---

# Alpha13 G10 — P1D local cerrado y diseño P1E de generación Ethernet

**Fecha:** 2026-10-12.  
**Estado P1D:** `LOCAL_SOURCE_CONTRACT_PASS_ADOPTION_BLOCKED`.  
**Estado A13-006:** `ADOPT_CONFIRMED_FIX_REVISION_REQUIRED`.  
**Estado Alpha13:** `IN_PROGRESS`.

## Evidencia de P1D local

El operador sincronizó `efd168d4` y ejecutó
`tools/alpha13/gates/run_a13_g10_p1d_a13_006_lifecycle.bat`:

```text
G10_P1D_SOURCE_CONTRACT=PASS_SIX_REAL_CANDIDATE_METHODS
G10_P1D_STATUS=SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN_ADOPTION_BLOCKED
G10_P1D_RUN_ID=g10_p1d_20261012_141926_035d46be
```

La prueba nativa independiente [38296384365](https://github.com/JW-Control/platform-jwplc/actions/runs/38296384365)
pasó Windows y Linux con
`RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED`.

**Lectura del resultado:** la CI ejecutó los seis cuerpos C++
del candidato P1B bajo mocks y reprodujo el riesgo de
cerrar un socket reasignado a otro owner después de reset
simulado. El BAT del operador validó la fuente/procedencia,
**no repitió la prueba nativa**. Ni el test nativo ni el
BAT tocan físicamente el W5500. El riesgo se deriva de la
intercalación controlada; no es incidente comprobado de campo.

El build de [P1C](A13_G10_P1C_ARDUINO_OVERLAY_CLOSURE_20261012.md)
sigue `PASS_4_OF_4`, pero compilar no permite adoptar el
candidato P1B.

## Inspección del ciclo real de reset — hallazgos de fuente

Archivos actuales del package `JWPLC/2.1.0`:

| Superficie | Observación de código | Consecuencia |
|---|---|---|
| `JWPLC_Ethernet.cpp::isReady()` | Retorna `_ready` | No indica generación de hardware |
| `JWPLC_Ethernet.cpp::task()` | Transiciona desde READY a LINK_OFF si `Ethernet.linkStatus()==LinkOFF` | `NOT_READY` puede ser cable/link, no reset confirmado |
| `JWPLC_Ethernet.cpp::probeHardware()` | Invoca `resetHardwareIfNeeded()` antes de adquirir SPI y después `Ethernet.begin()` | Una nueva prueba del hardware puede invalidar sockets |
| `JWPLC_Ethernet.cpp::resetHardwareIfNeeded()` | Si reset pin !=255: pulsa LOW, espera 10ms, HIGH y espera 80ms | Evento de reset físico que puede repetirse |
| `JWPLC_W5x00_Ethernet.cpp` | `begin()` y `beginDHCPAsync()` llaman `W5100.init()` | Ruta adicional de inicialización de backend |
| `utility/w5100.cpp::W5100Class::init()` | `static bool initialized` + early return | Un contador que se incremente **solo aquí** perdería los resets físicos posteriores |
| `JWPLC_ModbusTCP_Client.cpp` candidato P1B | Cierre diferido conserva índice, `task()` reintenta al READY | Si cambió generación, puede cerrar socket ajeno |
| `JWPLC_ModbusTCP.cpp` candidato P1B | Análogo con `_dropPending` | Mismo riesgo en servidor |

**No inferir** que todo `LinkOFF`, cada `NOT_READY`
o cada intento de `Ethernet.begin()` implique reset
físico efectivo. Inversamente, un reset físico cuando
`_resetPin!=255` sí debe invalidar los handles
anteriores incluso si la guarda de `W5100.init()`
retorna rápidamente.

Adicionalmente, `resetHardwareIfNeeded()` se invoca
**antes** de `acquireBus(500)` dentro de `probeHardware()`.
La secuencia reset/SPI compartido requiere revisión
de exclusión mutua y el tratamiento de otros dueños
de sockets, incluyendo DHCP y los otros consumidores
Ethernet. Este documento no aprueba modificar
esa sincronización sin prueba específica.

## Objetivos y controles de P1E

Primero capturar un snapshot reproducible de **solo lectura**
de estas superficies y relacionarlo con P1C/P1D local.
Después decidir diseño, todavía sin editar producto:

1. Definir una **generación efectiva de hardware/socket**
   monotónica cuya actualización esté ligada al reset
   físico/reinicialización pertinente (no a cada `NOT_READY`).
2. Tomar generación al adquirir socket cliente/servidor
   y observarla antes de cualquier operación posterior.
3. Si generación distinta: **invalidar solo el handle local
   en software, sin enviar comando de cierre al W5500**;
   jamás cerrar un índice que podría pertenecer a otro.
4. Si generación igual y falla SPI: **conservar** el
   identificador y reintentar sin bucle bloqueante;
   no admitir otro dueño ni otra conexión local.
5. Diferenciar reinicio de chip, cable/link OFF,
   timeout DHCP y SPI mutex ocupado.
6. Validar escenarios con al menos dos dueños y generaciones,
   éxito normal, fallos persistentes y recuperación,
   incluidos `begin/end/task/fail()` y server `task()`.
7. No introducir APIs públicas de usuario ni quitar autoload
   para resolver este defecto. Si se requiere señal interna
   del backend, definir un contrato privado explícito y
   analizar la compatibilidad con consumidores existentes.

### Condición de aceptación

El candidato corregido debe superar la misma prueba
`P1D`, pero esta vez el evento de socket reutilizado
**no puede terminar en `wrongOwnerCloses`**.
Además, no debe filtrar sockets permanentemente en
escenarios de fallo SPI sin reset. Probar los métodos
candidatos completos, no solo detección textual.
Después compilar baseline/candidato en Arduino,
registrar hashes y considerar adopción con rollback.
No acceder a hardware hasta tener contrato experimental
aceptado y autorización de upload independiente.

```text
G10_P1C=CLOSED_PASS_BUILD_ONLY
G10_P1D_LOCAL=CLOSED_PASS_SOURCE_CONTRACT
G10_P1D_NATIVE=CI_PASS_RISK_REPRODUCED
G10_P1D_DECISION=ADOPTION_BLOCKED
A13-006=REVISION_REQUIRED
G10_P1E=READ_ONLY_RESET_GENERATION_BASELINE_PENDING
G10_PRODUCT_CHANGE=NONE
G10_PHYSICAL_UPLOAD=NONE
A13-007=REVIEW_OPEN
A13-008=REVIEW_OPEN
F120=UNKNOWN_PENDING
ALPHA13=IN_PROGRESS
RELEASE_PR_MERGE=NO
```
