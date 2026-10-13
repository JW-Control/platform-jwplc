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
