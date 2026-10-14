# Alpha13 — G10 integrado: I / H / C (decisión aprobada)

**Fecha:** 2026-10-13
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`
**Decisión:** abandonar la secuencia manual de microgates. Se adoptan
**tres hitos con fases internas automatizadas**: `G10-I`, `G10-H`, `G10-C`.

Esta decisión modifica el **flujo de pruebas**, NO representa
adopción del candidato Ethernet ni autorización de upload.
Conforme a `docs/JWPLC_COLLABORATION_WORKFLOW.md`, cada
hito tiene una preparación, un ejecutor, criterios de parada
y un cierre. No repetir gates históricos que ya constan como PASS.

## Estado de entrada (evidencia existente)

| Evidencia | Estado | Qué demuestra |
|---|---|---|
| C2-S1 | Windows + CI PASS | Primeros ocho métodos W5500/Modbus de mutex SPI, solo slice |
| C2-S2A | Windows + CI PASS | Lease de `socketBegin` aislado |
| C2-S2B1 | Windows + CI PASS | Constructor/copias Client, Server begin/available/accept, cancelStop |
| C2-S2B2A | Windows + CI PASS | Begin/poll/stop TCP cooperativo |
| C2-S2B2B | Windows + CI PASS | Outbound TCP asíncrono |
| C2-S2C1 | Windows + CI PASS | `cancelWriteAsync` y cierre TX |
| C2-S2C2A | Windows + CI PASS | `connect(IPAddress)`, host pre-DNS con DNS mock |
| C2-S2C2B1 | Windows + CI PASS | Seis cuerpos UDP reales de lifecycle/send; cinco negativos |

La última evidencia local aportada por el operador:

```text
HEAD=c25a1201e7ead17475b06c3dd0f7fa3554c94757
RUN=g10_p1g_c2_s2c2b1_20261013_135948_9b7cb66e
STATUS=UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED
NEGATIVES=5_PASS_REJECTED
ARCHIVES=5_FROZEN
PRODUCT_CHANGED=NO
```

Los PASS nativos no se suman para afirmar un PASS del producto.
Los arneses anteriores compilan subconjuntos de cuerpos reales
con dependencias simuladas, no todo `JWPLC_Ethernet`.
Se conservarán sus fuentes, manifiestos y workflows para
trazabilidad/regresión, **sin ejecutarlos de nuevo uno a uno**
como requisito conversacional.

## G10-I — integración técnica (hito actual)

**Objetivo:** un único overlay reproducible de Ethernet/Modbus
y acceso W5500 que supere pruebas técnicas globales.

Fases internas del **ejecutor G10-I**, no gates humanos distintos:

1. **PREP**: preflight de branch, ancestor, tree tracked,
   SHA de cinco precompilados, procedencia de gates PASS;
   componer sobre una sola base las modificaciones aisladas
   de SPI, `socketBegin`, Client, Server, Modbus y UDP.
   Guardar `overlay/`, SHA origen/destino, `SUMMARY.log`
   y `MANIFEST.json` **solo bajo** `tools/alpha13/results/`.
   Conservar la lista de brechas como `BLOCKERS.md`.
2. **DESIGN/REVIEW**: cerrar los riesgos de semántica
   `SPI_BUSY`, `connected()`, `socketStatus()`,
   timeout/cancel/reintento automático, origen/copia de token,
   reciclaje de slots y listener con epoch. Proteger rutas reales
   `read/write/parse/flush/close`, DNS, DHCP, multicast si se usa.
   No sostener mutex SPI durante esperas cooperativas/red;
   reservar tiempos suficientes para SD, FRAM, TFT y RTC.
3. **NATIVE-REGRESSION**: una sola campaña automatizada
   para propietario/epoch, bloqueos anidados, carreras de cierre,
   busy, reuso, reset y listeners. Mutaciones negativas seleccionadas
   como **suite**, no secuencias de aprobación separadas.
   Fallo de cualquier caso = `REVIEW_STOP`.
4. **ARDUINO-INTEGRATION**: en **copia temporal** reproducible
   del package, compilar baseline vs candidato con
   Arduino CLI e IDE según corresponda, con autoload habitual:
   Display, Ethernet, SD, FRAM, RTC, botonera, RS-485,
   Modbus RTU, TCA/I/O y Modbus TCP.
   Exigir identidad fuente/objeto/archive/link de los cinco
   precompilados; ninguno se reemplaza para ocultar el problema.
   Verificar que los caminos reales enlazan el candidato,
   no un archive obsoleto. No habilitar GPIO4.
5. **RESULT**: un resultado por hito con desglose por fase
   y clasificación `PRODUCT/HARNESS/HARDWARE/ENVIRONMENT/PRECONDITION`.
   Registrar baseline, hashes, tiempos, alcance y bloqueo residual.

**Criterio para solicitar intervención del usuario:** solo
cuando exista un candidato integrado coherente, un
ejecutor único versionado y CI interna PASS. La
fase PREP por sí sola **no cierra G10-I** ni autoriza
cambiar producto.

## G10-H — verificación física (pendiente, requiere autorización)

Un solo gate físico, después de G10-I:
- verificar placa/variante, puerto COM, sketch/hashes,
  Ethernet W5500, link, TCP, UDP, DNS y Modbus TCP;
- concurrencia real con SD, FRAM, Display, RTC y el resto
  del autoload (botonera, RS-485, Modbus RTU, TCA/I/O);
- retiros/reconexiones de cable, reuso de socket,
  power-cycle, contención SPI y tiempos de respuesta;
- contrastar con baseline de Alpha12/Alpha13 y los criterios
  de robustez; logs serial y observación humana cuando
  sea necesaria;
- cualquier operación de flasheo y cualquier cambio a
  `RST-ETH` GPIO4 necesitan **autorización expresa**.
  GPIO4 queda `255` hasta demostrar interlock y fairness.

No atribuir PASS a hardware si se probó únicamente mock.

## G10-C — decisión y cierre

Una sola actualización de estado y checklist:
- reconciliar todo lo anterior sin copiar docenas de
  bloques cronológicos nuevos;
- decidir `A13-006=FIXED` solo con evidencia,
  o `DEFERRED/ADOPTION_BLOCKED` con motivos;
- revisar APIs Arduino, cinco archives, periféricos,
  logs de CLI/IDE, comportamiento físico y rollback;
- preparar PR/cierre Alpha13 y notas en español
  **solo cuando corresponda**. Sin auto-publicación.

## Bloqueos ya conocidos al iniciar G10-I

1. `connected()` y `socketStatus()` hacen falsas
   inferencias de desconexión/cierre al no adquirir SPI.
2. Rutas directas TCP RX/TX y UDP RX/TX/parse,
   DNS real y DHCP sin protección de lease end-to-end.
3. `stop()` o `connect()` pueden agotar timeout con
   socket todavía vivo si otro task posee SPI.
4. `server_port[]`, listener epoch/owner y reset
   siguen sin gate integrado real.
5. Compilación de fuentes completas Arduino,
   enlace/link de precompilados y fairness física pendientes.

```text
HITO=G10-I
ETAPA=PREP_IN_PROGRESS
NEXT_GATE=G10-I_INTEGRATOR
G10-H=PENDING_AUTHORIZATION
G10-C=PENDING
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
PRODUCT_MUTATION=NO
ARCHIVES_FROZEN=5
GPIO4=255
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

## Ejecutor único versionado (preparación)

```text
tools/alpha13/gates/a13_g10_integrated.py
tools/alpha13/gates/run_a13_g10_integrated.bat
.github/workflows/alpha13-g10-integrated.yml
```

La **versión inicial** compone los overlays en un
solo árbol temporal y emite `COMPOSED_REVIEW_BLOCKED`
mientras haya brechas. Su CI PASS significará **únicamente**
que la preparación de fuentes es reproducible,
**no** `G10-I=PASS`. Ampliar este mismo ejecutor
con fases de integración y compilación, en vez de crear
otros `S2C2B2/R1/R2` y un BAT por función.

**Límite importante:** Alpha13 tiene otros trabajos
paralelos (por ejemplo TFT-CLOSURE) con estado local
intencionalmente modificado. Nunca ejecutar `git pull`
o `git reset` sobre un worktree sucio; el preflight
detiene la ejecución hasta conciliar explícitamente
esa situación. G10-I no modifica esos tres archivos TFT.

No repetir de forma manual los gates cerrados.


## Actualización verificable: compilación Arduino del overlay (2026-10-13)

**CI integrada definitiva:** [run 38457826687](https://github.com/JW-Control/platform-jwplc/actions/runs/38457826687), `HEAD=5a82cdd9a3b9315e2c619b65dc096fee2350b26f`.

```text
G10-I-PREP-WINDOWS=PASS
G10-I-PREP-LINUX=PASS
G10-I-SAFEACCESS-H-CPP=INCLUDED_IN_ISOLATED_PLATFORM
G10-I-ARDUINO-CORE=IMMUTABLE_BOOTSTRAP_2.1.0-alpha.10
G10-I-ARDUINO-BRANCH-CORE-HEADERS=ADDITIONAL_INCLUDE_ONLY
G10-I-BASELINE-MODBUS-TCP-SERVER=COMPILE_PASS
G10-I-BASELINE-ETHERNET-SPI-COEXISTENCE=COMPILE_PASS
G10-I-CANDIDATE-MODBUS-TCP-SERVER=COMPILE_PASS
G10-I-CANDIDATE-ETHERNET-SPI-COEXISTENCE=COMPILE_PASS
G10-I-ETHERNET-SOURCE-ORIGIN=EXPLICIT_SELECTED_PATHS_VERIFIED_IN_LOG
G10-I-SAFEACCESS-COMPILE-OBJECT=PRESENT_UNIQUE
G10-I-ELF-AND-OBJECT-DEPENDENCY-PROVENANCE=NOT_FULLY_VERIFIED
G10-I-FULL-ALPHA13-PLATFORM-BUILD=NOT_RUN
G10-I-ARDUINO-IDE=NOT_RUN
G10-I-PHYSICAL-HARDWARE=NOT_RUN
G10-I-STATUS=ARDUINO_LIBRARIES_COMPAT_PASS_INTEGRATION_INCOMPLETE
G10-H=NOT_STARTED
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

La evidencia corresponde a **dos sketches, dos variantes**
y biblioteca fuente seleccionada explícitamente desde el árbol
de la rama o el package candidato temporal. Se conservó la
instalación del core bootstrap sin reescribir
`boards.txt/platform.txt`; este PASS no equivale al
package completo Alpha13 ni a la confirmación del autoload
físico en hardware. Todavía se debe comprobar el objeto
que produce el compilador y su símbolo en el ELF/mapa real,
además de las ocho brechas B01–B08.

### Problemas detectados y correcciones, sin microgates nuevos

1. **CI 38456681071:** al superponer una plataforma distinta
   encima del core Arduino ya instalado, el baseline se detuvo
   con `The instance is no longer valid and needs to be
   reinitialized`. Clasificación: `ENVIRONMENT`.
   Se evitó sustituyendo solo las bibliotecas seleccionadas
   por Arduino CLI mediante rutas explícitas, sin cambiar el
   core instalado.
2. **CI 38457151457:** baseline bloqueado por
   `jwplc_modbus_motor.h`, disponible en el core de la rama
   pero no en el bootstrap de Alpha10. Clasificación:
   `ENVIRONMENT/BOOTSTRAP_COMPATIBILITY`.
   Se añadió una ruta de include explícita de la rama a
   esta prueba, sin modificar el core instalado.
3. **CI 38457463479:** baseline PASS para ambos sketches;
   candidato falla al compilar
   `JWPLC_W5500_SafeAccess.cpp` porque
   `freertos/task.h` requiere cargar primero
   `freertos/FreeRTOS.h` en el entorno IDF del toolchain.
   Clasificación: `CANDIDATE_BUILD`.
   Corrección limitada al candidato aislado
   `tools/alpha13/candidates/g10_p1g_c/JWPLC_W5500_SafeAccess.cpp`:
   include condicionado a Arduino/ESP-IDF, compatible
   con stubs host.
4. **CI 38457826687:** ambos baseline y ambos candidatos
   compilan con Arduino CLI; las regresiones host C1,
   S2A y S2B1 pasaron Windows/Linux tras el cambio.
   Clasificación: `PASS_LIMITED_LIBRARY_COMPILATION`.

No se adoptó código en `JWPLC/2.1.0`; los cinco
archives permanecen congelados. La falla de baseline
o de la CI experimental no se presenta como fallo
de Ethernet en una placa real.

**Siguiente acción en el mismo G10-I:** consolidar
la semántica SPI_BUSY, el cierre y lease de todas las
rutas TCP/UDP/DNS/DHCP; verificar procedencia objeto
→ ELF/mapa con toolchain actual y compilación del
package completo con su core correspondiente.
No pedir un BAT por método ni avanzar a G10-H.


## Control de alcance: cuántas validaciones restan (2026-10-13)

El avance de G10-I **no** se divide otra vez en microgates. Se han
agrupado las tareas restantes en **cuatro campañas de validación**
y una revisión de cierre. Son unidades planificadas,
**no una garantía de solo cuatro ejecuciones**: un fallo puede
obligar a repetir la campaña afectada.

| Campaña | Hito | Criterio de salida |
|---|---|---|
| 1. SPI/socket/TCP/UDP/DNS/DHCP | G10-I | Concurrencia, BUS_BUSY distinto de desconexión, propietario y epoch de cada acceso, sin regresión de API |
| 2. Regresión integrada | G10-I | Reuso/retarget, stop y connect tras timeout, cierre diferido, reset seguro, pruebas negativas y estrés de máquinas de estado |
| 3. Compilación/enlace real | G10-I | Baseline vs candidato del **core completo Alpha13** y todos los periféricos autoload; objetos/.d/ELF/mapa atribuibles al código candidato; cinco archives idénticos |
| 4. Validación física | G10-H | Ethernet/Modbus y coexistencia real con Display, SD, FRAM, RTC, RS-485, botonera y TCA/I/O; autorizada por operador |

G10-C es la **decisión de cierre y documentación**, no otra
campaña técnica. Previstas **dos intervenciones manuales**
del operador si las validaciones internas y CI pasan:
ejecución integrada en Windows y prueba de hardware
autorizada. No pedir un BAT por cada método.

### Integración de evidencia de enlazado

En el **mismo workflow** `alpha13-g10-integrated.yml`
se incorporó revisión de procedencia tras compilación Arduino:
bibliotecas elegidas por `--library`, archivo de
dependencias GCC `.d` para `JWPLC_W5500_SafeAccess.cpp`,
un solo objeto, y símbolos `enter()/leave()` en el
`.elf` del sketch compilado. Si no aparecen, la CI
debe fallar en vez de anunciar una falsa integración.

**No se considera resuelta la campaña 3** hasta usar
el core Alpha13 completo, el autoload normal, objeto/ELF
y un baseline comparable. Por el momento el core de la
CI es el bootstrap Alpha10 compatible con headers
seleccionados desde la rama.

### Brechas funcionales críticas — no marcar como PASS

- `EthernetClient::connected()` devuelve `0` en
  el overlay si `BusScope(20)` falla por contención.
  El caller puede interpretarlo como desconexión.
- `EthernetClass::socketStatus()` devuelve
  `SnSR::CLOSED` en la misma circunstancia; otros
  callers pueden descartar su handle. No existe
  estado tri-valuado adoptado o una garantía de
  estatus cacheado coherente por token.
- `EthernetClient::stop()` retorna tras timeout
  sin cerrar el socket cuando sigue `SPI_BUSY`.
  Conservar el handle no equivale a garantizar
  un reintento futuro automático.
- `cancelConnectAsync()` también puede
  conservar un socket abierto en `SPI_BUSY`.
- DNS/DHCP y UDP RX/TX no están integrados
  de punta a punta bajo el ownership del socket.

**Decisión:** no sustituir estos retornos por
`CONNECTED` o `CLOSED` arbitrarios para forzar un
PASS. Diseñar un resultado de sondeo distinguible
(`VALID/BUS_BUSY/STALE`) para los consumidores
internos, conservando compatibilidad de las APIs
Arduino, y probarlo antes de habilitar adopción.

```text
G10-I=IN_PROGRESS
REMAINING_VALIDATION_CAMPAIGNS=4_PLANNED
G10-C=DECISION_NOT_TEST_CAMPAIGN
G10-I-ARDUINO_LINK_PROVENANCE=PASS_2_CANDIDATE_ELFS_RUN_38484339095
G10-I-ARDUINO_LINK_PROVENANCE_HEAD=1438fb883f83cddc1b839fb2961f5f9b4a335feb
G10-I-ARDUINO_BASELINE=2_OF_2_PASS
G10-I-ARDUINO_CANDIDATE=2_OF_2_PASS
G10-I-CANDIDATE_MMD=2_OF_2_PASS_CORRECT_SAFEACCESS_SOURCE
G10-I-CANDIDATE_ELF=2_OF_2_PASS_ENTER_LEAVE_SYMBOLS
G10-I-FULL_ALPHA13_CORE=NOT_VERIFIED
A13-006=ADOPTION_BLOCKED
G10-H=NOT_STARTED
G10_FINAL=NOT_CLOSED
GPIO4=255
FROZEN_ARCHIVES=5
```


**Verificación CI definitiva de procedencia Arduino:**
[run 38484339095](https://github.com/JW-Control/platform-jwplc/actions/runs/38484339095)
— los tres jobs PASS; dos baselines y dos candidatos compilan;
`.d` comprueba que el objeto procede del `SafeAccess.cpp` del
overlay y el ELF contiene `enter()` y `leave()` en ambos
candidatos. Este PASS **no cierra B06 ni G10-I** porque se usa
bootstrap Alpha10 con include del core de rama y no se compila
todo el package actual. B01/B04 continúan abiertos.


## G10-I — avance de diseño B01/B04 (2026-10-13)

**B01 — lectura de status SPI sin falsificar estado.** Se añadió
`StatusReadOutcome::{Read,BusBusy,Stale}` a la capa
**experimental**
`tools/alpha13/candidates/g10_p1g_c/JWPLC_W5500_SafeAccess.h/.cpp`,
y `readStatusIfCurrent(token,fn,context,timeout,&status)`.
La función usa `BusScope` y comprueba `SocketToken`
**antes** de leer; no sobrescribe `statusOut` en
`BusBusy` ni en `Stale`. Requiere prueba host
del scope anidado, CLOSED auténtico, contención,
reutilización de socket y reset de epoch. La regresión
corre como **fase interna G10-I**, no como microgate.

**Importante:** introducir una primitiva triestado **NO
resuelve automáticamente B01**. El overlay actual de
`EthernetClient::connected()` todavía devuelve `0`
y el de `EthernetClass::socketStatus()` devuelve
`SnSR::CLOSED` si `BusScope(20)` falla. Antes de
adoptar debe propagarse `BusBusy` a los consumidores
que destruyen/cambian sesiones (Modbus TCP, Server,
otros TCP y DNS), manteniendo la API Arduino binaria.
No usar `CONNECTED` como valor inventado.

**B04 — closeSession pierde referencia al socket con
SPI ocupado.** En
`JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp`
la implementación real contiene:

```cpp
if ((bool)_tcpClient && acquireBus(20))
{
    _tcpClient.cancelConnectAsync();
    releaseBus();
}
_tcpClient = EthernetClient();
```

`operator bool()` para `EthernetClient` devuelve
`_sockindex < MAX_SOCK_NUM`. Si `acquireBus(20)`
no obtiene el mutex, no hay llamada a
`cancelConnectAsync()`, pero se reasigna el objeto,
perdiendo el handle anterior. Es un **riesgo
source-backed**, no un fallo de campo confirmado.
La corrección debe preservar un ticket de cierre
asíncrono reintentable hasta que un servicio posterior
confirme `Closed` o `Stale`. No limitarse a
conservar el objeto local si su vida termina; evitar
cierre por índice sin token. La política de errores
y timeouts debe impedir solicitudes huérfanas.

```text
G10-I-B01=TRISTATE_PRIMITIVE_INTRODUCED_NOT_WIRED_ALL_CALLERS
G10-I-B04=MODBUS_TCP_CLOSE_SESSION_HANDLE_DROP_CONFIRMED_IN_SOURCE
G10-I-B04-DEFERRED_QUEUE=NOT_DESIGNED_NOT_VALIDATED
G10-I-PRODUCT_CHANGED=NO
G10-I-ADOPTION=BLOCKED
```


### Resultado de CI del contrato triestado — 2026-10-13

[GitHub Actions, run 38485428889](https://github.com/JW-Control/platform-jwplc/actions/runs/38485428889) ejecutado sobre
`456890bb3ace659841a59db4cabba57cf06e3f31`:

```text
G10-I-COMPOSE-WINDOWS=PASS
G10-I-COMPOSE-LINUX=PASS
G10-I-NATIVE-STATUS-PRIMITIVE-WINDOWS=PASS
G10-I-NATIVE-STATUS-PRIMITIVE-LINUX=PASS
G10-I-STATUS-PROBE=READ_BUS_BUSY_STALE
G10-I-OVERLAY-FILES=10
G10-I-ARCHIVES=5_FROZEN
G10-I-ARDUINO-BASELINE-MODBUS-TCP-SERVER=PASS
G10-I-ARDUINO-BASELINE-ETHERNET-SPI=PASS
G10-I-ARDUINO-CANDIDATE-MODBUS-TCP-SERVER=PASS
G10-I-ARDUINO-CANDIDATE-ETHERNET-SPI=PASS
G10-I-CANDIDATE-SOURCE-MMD-ELF=PASS_2_OF_2
G10-I-CURRENT_BLOCKERS=8
G10-I-CLIENT_CONNECTED_BUSY_SEMANTICS=NOT_FIXED
G10-I-MODBUS_TCP_CLOSESESSION_BUSY_LEAK=NOT_FIXED
G10-I-STATUS=COMPOSED_REVIEW_BLOCKED
A13-006=ADOPTION_BLOCKED
```

La prueba usa el core bootstrap `2.1.0-alpha.10` con
headers adicionales de la rama; **no** es el build
del package Alpha13 definitivo. Un PASS de esta
campaña parcial significa exclusivamente que la
primitiva triestado nativa, las bibliotecas seleccionadas,
el objeto y ELF candidato cumplen sus invariantes.
Las interfaces públicas Arduino y las fuentes productivas
permanecen intactas. **No autoriza G10-H ni upload**.

**Próximo trabajo B01/B04:** conectar el resultado
triestado a callers internos, establecer política
de cierre diferido con ciclo de vida garantizado
y probar regresión global de sockets, DNS y DHCP.
Se conserva un solo G10-I, no se abren microgates.


### G10-I B01/B04 — mitigación integrada experimental (2026-10-13)

**CI del mismo integrador:**
[run 38499744270](https://github.com/JW-Control/platform-jwplc/actions/runs/38499744270),
`HEAD=f95710240a57a64224975b8f195c36327ea5b723`.
Los tres jobs pasaron: composición Windows/Linux con nativo
y compilación Arduino CLI en Ubuntu.

**Diferencia respecto al anterior PASS triestado:** ya no se limita
a declarar el resultado `Read/BusBusy/Stale`. El overlay de
`EthernetClient` incorpora `jwplcProbeSocketStatus(uint8_t&)`
y el de `JWPLC_ModbusTCPClient` lo consulta dentro de
`serviceConnection()` y `serviceResponse()`, para no
descartar la sesión por BUS_BUSY ni usar hardware tras
un token obsoleto. No se cambió `connected()` público:
sus semánticas antiguas siguen pendientes para otros consumers.

En `closeSession()` se cambió el overlay para
**conservar `_tcpClient` y su token** cuando no es posible
cancelar, y añadir `_jwplcClosePending` y
`serviceDeferredClose()`. `busy()` refleja cierre pendiente
y `task()` lo atiende antes de cualquier nueva transacción.
`begin()` y `startRequest()` rechazan nueva actividad
mientras persiste limpieza pendiente. `cancelStopAsync()`
proviene del corte source-backed anterior y comprueba el lease
bajo mutex anidable. Todo es **overlay**, no package activo.

El arnés integrado ahora compila desde las fuentes del overlay
los cuerpos reales `busy()`, `closeSession()` y
`serviceDeferredClose()`, y verifica: cierre no destructivo
con SPI_BUSY, reintento posterior, rechazo de token viejo tras
reutilización de índice, e idempotencia. **Tres mutaciones
negativas** producen fallos nativos observables. Los tests
de contrato `Read/BusBusy/Stale` previos siguen pasando.

```text
G10-I-OVERLAY-COMPOSE-WINDOWS=PASS
G10-I-OVERLAY-COMPOSE-LINUX=PASS
G10-I-MODBUS-CLEANUP_NATIVE=PASS_3_REAL_CPP_METHODS_3_NEGATIVES
G10-I-TRISTATE_NATIVE=PASS_WINDOWS_LINUX
G10-I-ARDUINO_BASELINE=2_PASS
G10-I-ARDUINO_CANDIDATE=2_PASS
G10-I-CANDIDATE_MMD_AND_ELF=2_PASS
G10-I-FROZEN_ARCHIVES=5
G10-I-PRODUCT_MODIFIED=NO
G10-I-FULL_ALPHA13_PACKAGE=NOT_BUILT
G10-I-B01=PARTIALLY_MITIGATED_IN_MODBUS_CLIENT_ONLY
G10-I-B04=COOPERATIVE_RETRY_PROTOTYPE_NOT_END_TO_END
G10-I-GLOBAL_BLOCKERS=8_OPEN
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

**Limitaciones que impiden cerrar B01/B04:**
el comportamiento de `EthernetClient::connected()` público,
`EthernetClass::socketStatus()` con contención y la ruta
Modbus TCP Server todavía no tienen tratamiento uniforme.
El reintento de cierre depende de que el llamador siga
ejecutando `task()` mientras `busy()` sea verdadero.
Faltan pruebas integradas de reconexión, timeout simultáneo,
`JWPLC_Ethernet.isReady()` falso sin reset verdadero,
retarget, destructor/ciclo de vida, DNS/DHCP y fairness real.
Una prueba nativa no garantiza por sí misma que el hardware
real cierre el socket ni que el programa de usuario ejecute
el servicio con suficiente frecuencia.

**Cierre del bloque:** avance de diseño y compilación PASS;
**B01/B04 permanecen OPEN**, G10-I no se considera cerrado
y no se autoriza flash. Mantener el ejecutor único.


## Mantenimiento GitHub Actions — Node.js 24 / Ubuntu 24.04 (2026-10-14)

**Objetivo:** eliminar la dependencia de Node.js 20 de las acciones oficiales de GitHub, fijar Ubuntu contra migraciones imprevistas y **no alterar** el firmware ni el toolchain Arduino por una limpieza cosmética.

**Cambio de workflow:** `94f4c0326537c10668c0d8b57f730fd0ba180da5`.

| Acción/runner | Antes | Actual |
|---|---|---|
| actions/checkout | v4 | v5 |
| actions/setup-python | v5 | v6 |
| actions/upload-artifact | v4 | v6 |
| actions/cache | v4 | v5 |
| runner Ubuntu | ubuntu-latest | ubuntu-24.04 |

**Evidencia:** [GitHub Actions #38508580631](https://github.com/JW-Control/platform-jwplc/actions/runs/38508580631), ejecutado sobre el commit `94f4c032`:

```text
G10-I-CI-NODE24-UBUNTU24=PASS_5_OF_5_JOBS
G10-I-COMPOSE-UBUNTU24=PASS
G10-I-COMPOSE-WINDOWS=PASS
G10-I-ARDUINO-BOOTSTRAP-BASELINE-VS-CANDIDATE=PASS
G10-I-FULL-ALPHA13-BASELINE=PASS
G10-I-FULL-ALPHA13-CANDIDATE=PASS
G10-I-FULL-CANDIDATE-SOURCE_MMD_ELF=PASS_BOTH_SKETCHES
G10-I-PRODUCT_CHANGED=NO
G10-I-A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

**Advertencia residual explícita:** `arduino/setup-arduino-cli@v2` aún está basada en Node.js 20. GitHub la ejecuta temporalmente con Node.js 24 y el job **terminó PASS**, pero su warning `Node.js 20 is deprecated` persiste. No existe evidencia de una versión oficial `@v3` recomendada en las instrucciones de Arduino consultadas en esta revisión. No reemplazar por un instalador externo ni cambiar versión del CLI durante este mantenimiento sin validar hashes, distribución y paridad baseline/candidato. Las alertas por cambio de `ubuntu-latest` dejan de aplicar a los jobs ahora fijados en 24.04.

**Alcance:** este PASS certifica la compatibilidad del workflow actualizado con los cinco trabajos existentes; no prueba hardware, Arduino IDE ni cierra el algoritmo de concurrencia SPI/Modbus. Mantener los cinco archivos `.a` congelados y GPIO4=255 deshabilitado.


## G10-I — revisión source-backed B03: cierres UDP en DNS y DHCP (2026-10-14)

**Hallazgo confirmado en fuentes del producto** (no fallo físico observado):
`EthernetUDP::stop()` conserva el handle si no obtiene `BusScope(20)`
en el overlay G10-I. La conservación es necesaria para no cerrar el
socket de otro propietario, pero el llamador **debe reintentar** ese
cierre antes de abandonar la instancia UDP. Hoy no existe esa
garantía en todos los consumidores.

| Método fuente real | Secuencia que puede romper el contrato |
|---|---|
| `DNSClient::finishResolveAsync()` | `iUdp.stop()` → marca `iAsyncActive=false` sin confirmar liberación |
| `DNSClient::cancelResolveAsync()` | `iUdp.stop()` → borra estado y finaliza aun con SPI ocupado |
| `DNSClient::beginResolveAsync()` | Cancela anterior y comienza otro `iUdp.begin()` sin comprobar cierre |
| `DhcpClass::finishAsync()` | `_dhcpUdpSocket.stop()` → pone `_asyncActive=false` |
| `DhcpClass::beginWithDHCPAsync()` | `stop()` seguido inmediatamente de `begin(DHCP_CLIENT_PORT)` |

**Contrato pendiente de implementar:** una respuesta terminal
de DNS/DHCP debe distinguir *protocolo completado* de *socket
liberado*; no debe perderse un `SocketToken` pendiente ni
reabrirse o reciclarse indebidamente el mismo índice. Considerar
un mecanismo de cierre diferido que sobreviva al ciclo de vida
de la instancia y tenga cola/tickets acotados y política segura
ante starvation de SPI. No resolverlo simplemente con
`delay()`, con un `CLOSED` falso, con invalidación prematura
del token, ni con reintentos infinitos dentro del mutex.

Antes de adoptar, exigir pruebas de **cancelación con SPI_BUSY,
DNS timeout, DHCP renew/rebind, fin con enlace caído, reuso
de slot, vencimiento de ticket y pérdida/restitución de ownership**,
más compilación Arduino y gate físico posterior. DNS/UDP
`parsePacket()/read()/write()` también necesitan protección
de RX/TX y separación entre contención y falla de transporte.

### Ampliación de matriz de compilación

El mismo workflow G10-I incorpora **once sketches reales por
variante** para cubrir APIs de E/S, botonera, RTC, FRAM, SD,
RS-485, Modbus RTU, Display, DHCP y Modbus TCP servidor/cliente
con core Alpha13 aislado; se conservan las comprobaciones
de plataforma, librerías y `.d`/ELF del candidato.

La ejecución iniciada tras el cambio es
[GitHub Actions #38510083279](https://github.com/JW-Control/platform-jwplc/actions/runs/38510083279)
sobre `8b121b8b411d7f0646c8d048125b312bd691b7fa`.
**No afirmar PASS hasta confirmar sus trabajos baseline y
candidato.** Esta matriz valida compatibilidad de compilación,
no condiciones eléctricas ni autoload concurrente físico.

```text
G10-I-B03=UDP_DNS_DHCP_CLOSE_LIFETIME_CONTRACT_OPEN
G10-I-B03-SOURCE_RISK=CONFIRMED
G10-I-REPRESENTATIVE_SKETCHES_PER_VARIANT=11_CI_RUNNING
G10-I-UDP_DNS_DHCP_RUNTIME_TEST=NOT_RUN
G10-I-ARDUINO_IDE=NOT_RUN
G10-H=NOT_STARTED
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```
