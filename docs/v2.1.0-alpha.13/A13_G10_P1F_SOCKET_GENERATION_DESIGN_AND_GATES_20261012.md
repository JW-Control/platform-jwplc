## P1F — CI fuente/negativos PASS en Windows y Linux

El [run 38299316198](https://github.com/JW-Control/platform-jwplc/actions/runs/38299316198)
terminó **PASS Windows y Linux**. Su snapshot examinó
12 patrones relevantes a reset GPIO, mutex SPI, listener
`server_port[]`, backend y rutas Modbus TCP; ejecutó
2 casos negativos de contrato y comprobó SHA de
5 archives congelados. La CI **no probó un candidato
corregido**, ni hardware ni Arduino build.

```text
G10_P1F_CI=PASS_WINDOWS_LINUX_RUN_38299316198
G10_P1F_SELFTEST=PASS_TWELVE_PATTERNS_TWO_NEGATIVES
G10_P1F_RESET_OCCURS_BEFORE_SPI_LOCK=True
G10_P1F_LISTENER_SERVER_PORT_IS_STATIC=True
G10_P1F_READY_IS_BOOLEAN_NOT_EPOCH=True
G10_P1F_NO_GENERATION_EXPOSED_IN_RUNTIME=True
G10_P1F_ARCHIVES_FROZEN=5
G10_P1F_CI_STATUS=SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1F_LOCAL_WINDOWS=NOT_YET_EXECUTED
G10_PRODUCT_MODIFICATION=NONE
```

BAT local versionado:
`tools/alpha13/gates/run_a13_g10_p1f_socket_epoch_baseline.bat`.
Solo lee fuentes, cinco archives y manifiesto P1E local;
no compila, no sube firmware y no modifica Git.

---

# Alpha13 G10-P1F — Diseño del contrato de generación y ownership de sockets W5500

**Fecha:** 2026-10-12.  
**Estado:** `DESIGN_READ_ONLY_PENDING_LOCAL_GATE`; no adopción productiva.
**Hallazgo:** A13-006 confirmado, candidato P1B **bloqueado**.
**Alphas:** Alpha12 CLOSED_PUBLISHED; Alpha13 IN_PROGRESS.

## 1. Punto de partida y evidencia

El operador confirmó G10-P1E local:

```text
G10_P1E_HEAD=37366d10858864e6cdee9e9467191b858a939ee3
G10_P1E_RUN_ID=g10_p1e_20261012_142935_e37850cd
G10_P1E_ARCHIVES_FROZEN=5
G10_P1E_STATUS=RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED
```

P1C Arduino CLI real: cuatro builds baseline/candidato PASS.
P1D nativo [38296384365](https://github.com/JW-Control/platform-jwplc/actions/runs/38296384365)
reprodujo con mocks dos cierres de sockets reasignados
tras reset. P1E CI [38297416611](https://github.com/JW-Control/platform-jwplc/actions/runs/38297416611)
confirmó diez patrones de fuente, sin editar producto.

**No equiparar:** pruebas host con mocks, compile PASS y
validación sobre un W5500 físico. F120 sigue
`UNKNOWN_PENDING`; COM4 conserva diagnóstico G9,
COM14 diagnóstico G8 según última evidencia.

## 2. Tres superficies de riesgo verificadas en fuente

**2.1 Hardware y runtime Ethernet**

- `JWPLC_EthernetClass::probeHardware()` llama
  `resetHardwareIfNeeded()` **antes** de
  `acquireBus(500)`. Con pin válido efectúa LOW 10 ms,
  HIGH 80 ms. Si otras tareas usan SPI simultáneamente,
  el reset físico no está necesariamente protegido
  por la adquisición posterior del mutex.
- `JWPLC_EthernetClass::service()` pasa READY→LINK_OFF
  sin demostrar reset; `isReady()` solo devuelve `_ready`.
  Por tanto **no** incrementar epoch ni invalidar todos
  los sockets por una simple transición `!isReady()`.
- Las rutas `Ethernet.begin()` y
  `beginDHCPAsync()` llaman `W5100.init()`;
  `W5100Class::init()` usa `static bool initialized`
  y puede omitir inicialización adicional. Una generación
  incrementada **solo** ahí no observa los resets GPIO
  posteriores. La detección inicial puede ejecutar
  `softReset()` del chip y necesita cobertura diferenciada.
- Una caída desconocida o reset externo no detectado
  todavía no está cubierto por un contador nuevo: debe
  permanecer un límite declarado.

**2.2 Cliente Modbus TCP**

- `closeSession()` y servidor `dropClient()` podían
  descartar handles con SPI ocupado (A13-006).
- El candidato P1B retiene handle y encola reintento,
  pero el mock P1D demuestra posible cierre de socket
  **de otra generación/owner**.
- La fuente original también descarta el cliente en
  `task()` con Ethernet NOT_READY y en la ruta
  `serviceConnection()`. Esas rutas deberán
  distinguir invalidación **confirmada** de contención,
  link caído y conexión remota realmente cerrada.

**2.3 Servidor/listener y tabla global**

- `EthernetServer::begin()` registra
  `server_port[sockindex] = _port`.
- `EthernetServer::available()`, `accept()` y
  `operator bool()` consultan la tabla y los estados
  actuales del hardware, pero no tienen epoch.
- Tras un reset, la tabla puede contener asociaciones
  anteriores y un mismo índice podría reasignarse.
  **Hipótesis a probar**, no afirmar fallo observado en
  hardware. El contrato de generación debe incluir
  también el listener, sin reiniciarlo ciegamente cada
  vez que cae el enlace.

## 3. Invariantes obligatorias del candidato revisado

| Invariante | Condición verificable |
|---|---|
| S1 Identidad | Cada handle incluye el epoch efectivo de su generación; no basta el índice numérico |
| S2 SPI transitorio | Denegación de lock conserva ownership y permite retry acotado, sin liberar handle |
| S3 Reset real | Antes de cualquier stop/cancel, epoch anterior se declara inválido y **no se escribe al socket por índice antiguo** |
| S4 Link OFF | Una caída de cable sin reset no invalida sockets por sí sola; preservar o cerrar bajo bus según política explícita |
| S5 Exclusión reset | No efectuar reset GPIO del W5500 mientras otro dueño usa ese chip; coordinar acceso y marcar generación de forma coherente |
| S6 Cliente | `begin/end/task/poll/fail/requests` no reabren ni reusan handles pendientes y no bloquean indefinidamente |
| S7 Servidor | `task/dropClient/ensureServerListening`, estado RX INT y listener no confunden sockets antiguos y nuevos |
| S8 Otros consumidores | DHCP, UDP y usuarios directos Ethernet no son cerrados por una referencia Modbus TCP obsoleta |
| S9 Sin regresiones | Arduino IDE/CLI, autoload integral, APIs de usuario y archives sin cambios ajenos al alcance |
| S10 Evidencia | Una CI mock puede probar invariantes, pero hardware requiere opt-in y evidencia por separado |

## 4. Arquitectura candidata: a evaluar, no aprobada

**Contador de generación interno del chip** con lectura coherente
por los consumidores. Debe incrementarse al invalidar sockets
por reset real del W5500 o reinit efectivo que destruya su
estado; no en cada `isReady()==false`, fallo DHCP,
LINK_OFF o contención SPI.

**Crítico:** dado que `resetHardwareIfNeeded()` hoy pulsa
GPIO antes de adquirir SPI, una generación publicada al final
del reset no evita una transacción concurrente al chip;
y un reset bajo un mutex mantenido 90 ms puede perjudicar
FRAM/SD. Deben compararse dos alternativas experimentalmente:

- A. Protección breve de fase de transición con guardia
  de reset/quiescencia de clientes antes del pulso,
  verificando que las operaciones de sockets se rechacen
  hasta terminar la reconstrucción. Examinar compatibilidad
  con mutex SPI existente y carreras entre tareas.
- B. Tomar mutex del W5500 durante todo el reset. Es
  semánticamente más simple, pero puede introducir
  latencias SPI de ~90 ms o mayores: **no adoptar sin
  medir/perfilar FRAM, SD y otras tareas**.

El contador y la invalidación del listener se pueden
exponer mediante una interfaz **interna** sin cambiar
APIs públicas de sketches; el método exacto queda
pendiente de validar. Evitar modificar el backend
`W5100.init()` de manera aislada o asumir reset
por toda reconfiguración `Ethernet.begin()`.

La comprobación de epoch antes de `stop()` debe ser
atómica/coherente respecto a la transición de reset y
al mutex que protege los registros del chip. Comparar
un contador **fuera** de la exclusión mutua y cerrar
**después** sería una carrera TOCTOU.

## 5. Plan de gates P1F → P1G

**P1F (solo lectura, ahora):** fijar SHA de runtime
Ethernet, backend, W5100, EthernetServer, Client
Modbus TCP, Server Modbus TCP y cinco archives;
capturar patrones de reset, guardia init, tabla
`server_port` y riesgo de reset previo al mutex.
CI Windows/Linux + operador local usando
manifiesto P1E; no hardware, compilación ni producto.

**P1G (solo candidato + nativos negativos):**
elegir explícitamente una política de reset/quiescencia
con coste SPI medido/estimado y modelar sus carreras.
Generar overlay sin tocar producto. Exigir dos owners
cliente/servidor/UDP, dos generaciones, link OFF sin
reset, reset efectivo, acquireBus fallido repetidamente,
reintentos, listener rebind y ausencia de stop al owner
nuevo. Ejecutar funciones realmente extraídas del
candidato; no basta un modelo Python aislado.

**P1H (build real):** baseline y candidato Arduino CLI
para Client/Server TCP y regresión SD/FRAM/Core,
registrando rutas de objetos y hashes.

**P1I (adopción controlada):** solo si P1G/P1H demuestran
invariantes; backup/rollback, commit producto y
pruebas físicas separadas con autorización de upload.
Nunca considerar un PASS de diseño equivalente a
conformidad funcional.

A13-007 DHCP, A13-008 UDP, A13-015/016/018 y F120
permanecen pendientes independientes.

```text
G10_P1E=PASS_WINDOWS_AND_CI_READONLY
G10_P1F=DESIGN_AND_BASELINE_PREPARED
G10_P1G=NOT_STARTED
A13-006=ADOPT_CONFIRMED_REVISION_REQUIRED
A13-006_P1B_CANDIDATE=ADOPTION_BLOCKED
G10_PRODUCT_CHANGE=NONE
G10_HARDWARE_UPLOAD=NONE
G10_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
PR_MERGE_RELEASE=NO
```
