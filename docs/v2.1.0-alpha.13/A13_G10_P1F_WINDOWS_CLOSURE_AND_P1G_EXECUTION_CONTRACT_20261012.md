# Alpha13 G10 — cierre P1F Windows y contrato de ejecución P1G
Fecha: 2026-10-12. Rama: v2.1.0-alpha.13/feature/cleanup-robustness.

## 1. Cierre del gate P1F

El operador ejecutó el BAT versionado tras confirmar git status limpio y
fast-forward 37366d10 -> f5d8d323. Evidencia textual aportada el 2026-10-12:

~~~text
G10_P1F_HEAD=f5d8d32369ecbc64163e65ed3e52e0a4f88733f6
G10_P1F_RESET_OCCURS_BEFORE_SPI_LOCK=True
G10_P1F_RESET_IS_GPIO_AND_OPTIONAL=True
G10_P1F_READY_IS_BOOLEAN_NOT_EPOCH=True
G10_P1F_INIT_USES_STATIC_INITIALIZED_GUARD=True
G10_P1F_PHYSICAL_PROBE_STILL_ENTERS_BACKEND=True
G10_P1F_LISTENER_SERVER_PORT_IS_STATIC=True
G10_P1F_LISTENER_RECORDS_SOCKET_INDEX=True
G10_P1F_LISTENER_QUERIES_LEGACY_MAPPING=True
G10_P1F_MODBUS_SERVER_REBUILDS_LISTENER_ON_READY=True
G10_P1F_TCP_CLIENT_STILL_DROPS_HANDLE_ON_NOT_READY=True
G10_P1F_TCP_SERVER_STILL_DROPS_HANDLE_ON_NOT_READY=True
G10_P1F_NO_GENERATION_EXPOSED_IN_RUNTIME=True
G10_P1F_ARCHIVES_FROZEN=5
G10_P1F_STATUS=SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1F_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1f_20261012_162335_c18278de
~~~

CI P1F: https://github.com/JW-Control/platform-jwplc/actions/runs/38299316198
Windows/Linux PASS (12 patrones de fuente, 2 negativos de contrato).
El log local no muestra por separado una llamada a --selftest;
los 2 negativos se atribuyen a CI, no a la ejecución local.

Decisión:
~~~text
G10_P1F_WINDOWS=CLOSED_PASS_READONLY_SOURCE_AND_FROZEN_ARCHIVES
G10_P1F_PRODUCT_FIX=NOT_PROVEN
G10_P1F_PRODUCT_CHANGE=NONE
G10_P1F_PHYSICAL_UPLOAD=NONE
A13-006=REVISION_REQUIRED_ADOPTION_BLOCKED
G10_P1G=NEXT_CANDIDATE_DESIGN_AND_NATIVE_NEGATIVES
G10_FINAL=OPEN
ALPHA13=IN_PROGRESS
~~~

## 2. P1G: tres riesgos inseparables

1. **Reset fuera del lock**: probeHardware() ejecuta
   resetHardwareIfNeeded() ANTES de acquireBus(500). El GPIO puede
   invalidar sockets mientras hay operaciones con otro dueño. No
   aprobar un contador que se publique solo tras el reset.
2. **Handle antiguo**: P1B retiene el índice con _closePending /
   _dropPending tras SPI ocupado, pero P1D reprodujo en nativo la
   posibilidad de cerrar un índice reasignado luego de reset.
   El número del socket no demuestra identidad ni ownership.
3. **Listener antiguo**: EthernetServer::server_port[] es un mapa
   estático socket->puerto sin generación, potencialmente obsoleto
   tras reset y reuso. No basta proteger solo Client/Server Modbus.

isReady() retorna el booleano _ready; LINK_OFF y otras situaciones de
not-ready no equivalen a reset. W5100.init() tiene un guard estático;
su invocación por sí sola tampoco detecta reinicios posteriores.
Una inicialización/soft-reset efectivo y resets externos no detectables
requieren cobertura o limitación explícita.

## 2A. Hallazgo complementario — APIs Ethernet directas y doble capa de bloqueo

Inspección read-only posterior al cierre P1F:

- `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp`:
  `EthernetClass::socketBegin()` y `socketDisconnect()` usan
  `SPI.beginTransaction(SPI_ETHERNET_SETTINGS)`, pero esos
  cuerpos **no llaman** `jwplcSPI_acquire()`.
- `EthernetClient.cpp`: `EthernetClient::beginStopAsync()`,
  `pollStopAsync()` y `cancelStopAsync()` operan vía
  `Ethernet.socketStatus/socketDisconnect/socketClose`;
  `localPort()` y `remoteIP()` usan directamente
  `SPI.beginTransaction()`, sin adquirir el mutex JWPLC
  dentro de esos métodos.
- `cores/jwcontrol/peripherals/src/jwplc_spi_bus.cpp`:
  `jwplcSPI_acquire()` usa un mutex FreeRTOS creado por
  `xSemaphoreCreateMutex()` y adquirido con
  `xSemaphoreTake()`.
  No existe evidencia en estos métodos de que
  `SPI.beginTransaction()` tome **ese mismo mutex JWPLC**.

Consecuencia delimitada: los consumidores que envuelven
correctamente las llamadas en `jwplcSPI_acquire/release`
pueden estar protegidos entre sí, pero un usuario directo
de `EthernetClient`, `EthernetServer` o de helpers socket
sin ese wrapper **no queda demostrado** como coordinado
por el mutex JWPLC. No afirmar que ocurrió una colisión
física, ni negar posibles locks internos propios de
SPIClass: esos locks no equivalen automáticamente al
contrato de ownership JWPLC.

**Nuevo bloqueo de diseño P1G:** antes de adoptar A o B,
definir el perímetro real de acceso W5500 para API Arduino
directa y consumidores internos, sin meter adquisiciones
anidadas de un mutex NO recursivo: envolver de nuevo
`socketBegin()` desde una llamada que ya tiene el lock
podría bloquear. Examinar un punto de sincronización
común o un lease explícito que reconozca ownership
y evite doble adquisición. No tocar la implementación
SPI global de Arduino ni modificar producto por inercia.

Este hallazgo aumenta la importancia de la cobertura
negativa `raw EthernetClient/Server`, del test de
`nested_lock_deadlock` y de la medición de fairness.
P1G permanece en diseño; ninguna implementación nueva
se acepta todavía.

## 3. Criterios para un candidato de producto, no para un modelo

- **Identidad**: conservar en cada socket un lease interno con epoch
  efectivo del chip y owner. Epoch sin ownership no protege el
  reuso del mismo índice dentro de una generación.
- **Sin TOCTOU**: comprobar lease + epoch bajo la misma
  exclusión que protege el comando destructivo al W5500.
  Prohibido comparar antes de lock y ejecutar stop/cancel luego.
- **Invalidación**: en reset/reinicialización efectiva invalidar los
  leases anteriores. Si el handle es de generación vieja, limpiar
  solo software sin enviar stop() o cancelConnectAsync().
- **Contención**: un acquireBus fallido NO libera ni reasigna handle
  de generación vigente; reintentar sin monopolizar task(), sin bucles
  de espera indefinidos y sin perder temporizadores de usuario.
- **Ciclo de vida**: incluir client begin/end/task/fail/request/connected
  y server begin/task/dropClient/ensureServerListening/RX INT, además
  de EthernetServer::server_port y clientes TCP/UDP no Modbus.
- **Enlace vs reset**: LINK_OFF simple no aumenta epoch ni libera un
  socket por mera suposición de reset. DHCP timeout tampoco.
- **Integridad**: source-first, compile baseline/candidato, Arduino
  CLI/IDE, autoload de todos los periféricos, archivos .a congelados
  hasta la aprobación específica del producto afectado.
- **Transparencia**: ninguna API pública nueva ni upload automático;
  no atribuir resultados de mocks a hardware físico.

### Interlock de reset a decidir con evidencia

Alternativa A: barrera exclusiva W5500 de transición/quiescencia.
Debe bloquear toda operación del chip ANTES de GPIO reset, permitir
FRAM/SD usar SPI durante los 90 ms de espera y evitar lectores SPI
W5500 no registrados. Sin prueba de cobertura completa no es segura.

Alternativa B: adquirir el mutex SPI compartido antes de GPIO reset
y mantenerlo durante todo el pulso y espera. Simplifica exclusión,
pero retiene SPI aprox. 90 ms y puede dañar fairness, FRAM/SD, RTC o
timings del autoload. No promover sin cuantificar hold/latencias
y probar contra workloads reales.

Ninguna alternativa queda seleccionada por P1F. Medir y comparar
es parte de P1G. Antes de mutar producto, incluir también lock
consumido por EthernetServer y por los otros paths W5x00.

## 4. Secuencia integrada del hito P1G

1. Releer firmas reales (runtime Ethernet, core SPI mutex, socket.cpp,
   EthernetServer, client/server Modbus TCP, UDP/DHCP) y congelar
   huellas de entrada. Validar si el interlock cubre **todas** las
   rutas del chip; si no, parar como REVIEW_DESIGN.
2. Generar candidato de fuentes SOLO en overlay temporal. La
   política seleccionada debe incluir epoch/owner, listener e
   interlock (no solo uno de los tres). Mantener rollback y
   producto byte a byte intacto.
3. Reutilizar arnés P1D para extraer y compilar métodos C++ reales
   modificados. Ampliar con dos propietarios, reset/reuso,
   mismo índice en misma generación, LINK_OFF sin reset,
   contención persistente y recovery, operaciones del listener,
   TOCTOU de reset vs stop y ausencia de wrongOwnerCloses.
4. Incorporar negativos que deliberadamente eliminen guard
   de epoch, guard de owner e interlock; el gate debe FALLAR
   frente a esas mutaciones. Solo un modelo abstracto Python
   no sirve como aceptación del fix.
5. Si nativo pasa, habilitar P1H (build Arduino baseline/candidato,
   procedencia de objetos y hashes, SD/FRAM/autoload). Si falla,
   clasificar FAIL_CANDIDATE o REVIEW_HARNESS y corregir.
6. P1I (adopción y prueba física) SOLO tras aceptación técnica,
   respaldo y autorización independiente. Nada de push
   productivo/flash como efecto colateral del experimento.

### Clasificación y bloqueos

~~~text
G10_P1G_DESIGN_SOURCE_REVIEW=OPEN
G10_P1G_CANDIDATE_NATIVE=NOT_RUN
G10_P1G_ARDUINO_BUILD=NOT_RUN
G10_P1G_PRODUCT_ADOPTION=NO
G10_P1G_HARDWARE_FLASH=NO
A13-007_DHCP=REVIEW_OPEN
A13-008_UDP=REVIEW_OPEN
A13-015_016_018=PENDING
F120=UNKNOWN_PENDING
COM4_DIAGNOSTIC=NOT_RESTORED
COM14_DIAGNOSTIC=NOT_RESTORED
RELEASE_PR_MERGE=NO
~~~

Este documento **no** prueba la solución. Formaliza el cierre P1F y
las condiciones de seguridad exigidas antes de escribir el próximo
candidato A13-006.
