## Resultado posterior: CI nativa P1D PASS como prueba de riesgo (2026-10-12)

La [CI 38296384365](https://github.com/JW-Control/platform-jwplc/actions/runs/38296384365)
terminó **PASS Windows y Linux**, con fuentes candidatas P1B
reproducidas y seis cuerpos C++ reales ejecutados bajo mocks de
sockets y SPI. El test incluye dos contraejemplos explícitos
de **reutilización de índice tras reset simulado**,
uno para cliente y otro para servidor.

```text
G10_P1D_SOURCE_CONTRACT=PASS_SIX_REAL_CANDIDATE_METHODS
G10_P1D_NATIVE=PASS_SOURCE_BACKED_LIFECYCLE_AND_RESET_COUNTEREXAMPLES
G10_P1D_STATUS=RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED
```

**Interpretación:** `CI_SUCCESS` solo significa que el harness
demostró lo esperado. La implementación candidata **NO superó
el requisito de seguridad de generaciones de socket**, por lo
que se **BLOQUEA su adopción**. Se probó con escenario
de interleaving controlado, no ocurrió sobre hardware real.
El primer intento CI de P1D falló por constantes de
defaults omitidas en el shim C++ nativo; se ajustó solo
el shim y se añadieron diagnósticos de compilador.
La CI de retest de arriba es la evidencia válida.

**Próximo trabajo de diseño:** antes de nuevos candidatos
se requiere identificar el ciclo de generación/reset del
runtime `JWPLC_Ethernet`, evaluar si el reinicio observado
es realmente una invalidación de todos los sockets y
diferenciar pérdida temporal de `isReady()` frente a
reset real del W5500. No asumir que todos los
`NOT_READY` invalidan handles. Aplicar un token de
generación privado o política conservadora equivalente
que no permita cerrar un índice de otro owner.

```text
G10_P1D_CI=PASS_WINDOWS_LINUX_RUN_38296384365
G10_P1D_CANDIDATE_ASSESSED=BLOCKED_BY_RESET_REUSE_COUNTEREXAMPLE
G10_P1D_WINDOWS_LOCAL_SOURCE_CONTRACT=NOT_YET_RUN
A13-006=ADOPT_CONFIRMED_FIX_DESIGN_REVISION_REQUIRED
G10_PRODUCT_ADOPTION=NONE
ALPHA13=IN_PROGRESS
```

---

# Alpha13 G10-P1D — Pruebas negativas de ciclo de vida A13-006

**Fecha:** 2026-10-12. **Estado inicial:** `NATIVE_CI_PENDING`; **candidato productivo NO adoptado**.

## Punto de partida verificado

El operador validó G10-P1C en Windows después de reparar F129:

```text
HEAD=802c6260
G10_P1C_RUN=g10_p1c_20261012_140152_a7211c08
G10_P1C_BASELINE_SERVER=PASS
G10_P1C_BASELINE_CLIENT=PASS
G10_P1C_CANDIDATE_SERVER=PASS
G10_P1C_CANDIDATE_CLIENT=PASS
G10_P1C_FINAL_AUDIT=PASS
G10_P1C_STATUS=ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED
```

El primer intento interrumpido con `History restored`
no constituye PASS: el segundo sí terminó. F129 quedó
`RESOLVED_CI_AND_LOCAL_RETEST`.

## Hallazgo de revisión P1D: socket pendiente frente a reinicio W5500

El candidato P1B conserva el índice del socket y registra
`_closePending` (cliente) o `_dropPending` (servidor)
cuando la adquisición SPI falla. Ambos `task()` usan
`JWPLC_Ethernet.isReady()` para abstenerse de cerrar
cuando Ethernet está deshabilitado o reiniciándose.

**Posible interleaving peligroso:**

1. Un cliente TCP usa índice de socket `2`.
2. Se intenta cerrar, pero `acquireBus(20)` falla;
   el índice queda retenido correctamente.
3. El W5500 se reinicia, invalidando generaciones de
   socket; Ethernet reporta `NOT_READY`.
4. Otro subsistema obtiene el índice `2` en la nueva
   generación antes del reintento del cliente original.
5. Ethernet pasa a `READY`; `task()` reintenta el cierre
   con el índice antiguo y puede cerrar una sesión
   **ajena**. La misma posibilidad aplica al índice
   del socket servidor.

Esta es una **hipótesis de concurrencia y reutilización**
derivada del código. No afirmar que esa asignación entre
generaciones haya sido observada en hardware.
Un test nativo con propiedad/generación controlada
de sockets permitirá comprobar si la lógica candidata
tiene protección suficiente contra ese interleaving.

## Contrato del gate

Herramientas:
```text
tools/alpha13/gates/a13_g10_p1d_a13_006_lifecycle.py
tools/alpha13/gates/run_a13_g10_p1d_a13_006_lifecycle.bat
.github/workflows/alpha13-g10-p1d-a13-006-lifecycle.yml
```

La prueba carga el generador de P1B, genera **las mismas
fuentes candidatas**, extrae seis cuerpos C++ reales:
`client.closeSession()`, `client.begin()`, `client.end()`,
`client.task()`, `server.dropClient()` y `server.task()`.
Se compilan con sockets/bus SPI/estado de red simulados.
Los casos cubren:

- cliente: no perder handle con SPI bloqueado;
- `begin()`: no configurar/reabrir con cierre pendiente;
- `task()`: drenaje cuando SPI vuelve, aunque el estado
  ya no sea `busy()`;
- `end()`: limpiar pendiente al volver a invocar `task()`;
- servidor: retener identificador y no atender conexiones
  nuevas mientras SPI está bloqueado;
- servidor: reintento exitoso cuando SPI se libera;
- contraprueba de **reset/reuso** en cliente y servidor,
  con identificador reocupado por propietario diferente.

No altera `JWPLC_ModbusTCP`, `JWPLC_Ethernet`, core,
archives, Git ni COM4/COM14. La CI se ejecuta con
compilador C++ nativo Windows/Linux. El BAT previsto del
operador es **solo de fuente**, y contrasta el candidato
contra los manifests P0/P1A/P1B/P1C locales.

## Semántica deliberada del resultado

Si se reproduce el cierre de un socket de otro propietario,
la prueba nativa devolverá `EXIT=0` porque **detectó
satisfactoriamente el riesgo previsto**. El marcador de
estado **debe indicar explícitamente BLOQUEO de adopción**:

```text
G10_P1D_SOURCE_CONTRACT=PASS_SIX_REAL_CANDIDATE_METHODS
G10_P1D_NATIVE=PASS_SOURCE_BACKED_LIFECYCLE_AND_RESET_COUNTEREXAMPLES
G10_P1D_STATUS=RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED
```

Ese resultado **NO equivale a FIX PASS**; significa que
el candidato actual debe corregirse y revalidarse
con al menos dos generaciones de hardware antes de
adoptarse. Si no se reproduce, mantener estado REVIEW
e investigar el entorno simulado antes de concluir seguro.

## Decisión esperada si el riesgo se reproduce

- Rechazar el candidato P1B tal cual para adopción.
- Diseñar invalidación del handle pendiente por
  *generación Ethernet*; investigar qué señal concreta
  de reinicio W5500 está disponible para las librerías
  sin tocar APIs públicas.
- Contrastar reset frente a caída DHCP/link: `!isReady()`
  **no es una prueba universal de reinicio físico**.
- Reintentar cierre solo cuando la identidad/generación
  del socket sigue siendo válida; asegurar que no se
  conserva un handle indefinidamente sin políticas de
  recuperación.
- Ejecutar negativos posteriores con dos owners, estado
  READY intermitente, `end/begin/task/fail` y regresión
  física si el producto TCP se modifica.

```text
G9=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P1C=CLOSED_PASS_BUILD_ONLY
A13-006=FIX_CANDIDATE_NOT_ADOPTED
G10_P1D=CANDIDATE_NEGATIVE_TEST_PENDING
G10_A13-007=REVIEW_OPEN
G10_A13-008=REVIEW_OPEN
G10_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
PR_MERGE_RELEASE=NO
```
