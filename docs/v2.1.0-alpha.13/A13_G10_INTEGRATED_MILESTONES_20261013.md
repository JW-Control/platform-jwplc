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
