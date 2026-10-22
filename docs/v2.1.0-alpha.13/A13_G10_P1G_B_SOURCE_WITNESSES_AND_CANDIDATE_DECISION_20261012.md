# Alpha13 G10-P1G — cierre P1G-A Windows y P1G-B CI; contrato de candidato

Fecha: 2026-10-12 (hora Perú; GitHub Actions reporta UTC 2026-10-13).

## 1. Evidencia local de P1G-A

El operador ejecutó:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_a_mutex_callpaths.bat
```

Resultado aportado en conversación:

```text
G10_P1G_A_SELFTEST=PASS_4_NEGATIVES
G10_P1G_A_HEAD=e53b0e4d93ca9cd921a313d5e37b246d02856d4c
G10_P1G_A_RUNTIME_LOCAL_GUARDS=11
G10_P1G_A_MODBUS_SERVER_LOCAL_GUARDS=4
G10_P1G_A_MODBUS_CLIENT_LOCAL_GUARDS=4
G10_P1G_A_TOTAL_HIGH_LEVEL_LOCAL_GUARDS=19
G10_P1G_A_RAW_CLIENT_DIRECT_NO_LOCAL_GUARD=24
G10_P1G_A_RAW_SERVER_DIRECT_NO_LOCAL_GUARD=5
G10_P1G_A_RAW_UDP_DIRECT_NO_LOCAL_GUARD=14
G10_P1G_A_SOCKET_DIRECT_NO_LOCAL_GUARD=20
G10_P1G_A_BACKEND_DIRECT_NO_LOCAL_GUARD=18
G10_P1G_A_LOWER_DIRECT_TOTAL_NOT_BUG_COUNT=81
G10_P1G_A_RESET_HW_GPIO=4
G10_P1G_A_RESET_FIRMWARE_DEFAULT=255
G10_P1G_A_ARCHIVES_FROZEN=5
G10_P1G_A_MATRIX_ROWS=257
G10_P1G_A_STATUS=CALLPATH_BASELINE_CAPTURED_ADOPTION_BLOCKED
G10_P1G_A_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_a_20261012_231402_0953c0ca
```

```text
G10_P1G_A_WINDOWS_LOCAL=CLOSED_PASS
G10_P1G_A_CI=PASS_WINDOWS_LINUX_RUN_38339370374
A13-006=ADOPTION_BLOCKED
```

## 2. Gate versionado P1G-B: testigos de flujo real

Archivos:
- `tools/alpha13/gates/a13_g10_p1g_b_callgraph_contract.py`
- `tools/alpha13/gates/run_a13_g10_p1g_b_callgraph_contract.bat`
- `.github/workflows/alpha13-g10-p1g-b-callgraph.yml`

CI definitivo:
https://github.com/JW-Control/platform-jwplc/actions/runs/38346137383

```text
G10_P1G_B_CI=PASS_WINDOWS_LINUX
G10_P1G_B_CI_HEAD=92549d497c4460871815a3d9c87c4bcf081cb89e
G10_P1G_B_SOURCE_WITNESSES=19
G10_P1G_B_NEGATIVES=5
G10_P1G_B_FROZEN_ARCHIVES=5
G10_P1G_B_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_B_PRODUCT_CHANGE=NONE
G10_P1G_B_NATIVE_CANDIDATE=NOT_STARTED
G10_P1G_B_HARDWARE=NOT_TOUCHED
```

La primera CI `38346055573` y la segunda `38346097828`
fallaron por arnés: los negativos `REMOVE_MB_CLIENT_LOCK` y
`LISTENER_OWNERSHIP_DRIFT` modificaban solo una ocurrencia de
condiciones repetidas. La corrección muta todas las ocurrencias
pertinentes y CI `38346137383` pasa Windows/Linux. No se atribuyen
estos errores del arnés al firmware.

### Testigos cubiertos (19)

Rutas protegidas desde llamador:
1. Modbus Server RX -> EthernetServer.available.
2. Modbus Server listener -> EthernetServer.begin.
3. Modbus Client connect -> EthernetClient.beginConnectAsync.
4. Modbus Client connected -> EthernetClient.connected.
5. L2 refresh -> EthernetUDP temporal bajo lock del runtime.

Entradas genéricas sin adquisición JWPLC propia:
6. EthernetClient.connected -> Ethernet.socketStatus -> W5100.
7. EthernetServer.begin -> socketBegin/socketListen.
8. EthernetUDP.begin/stop -> socketBegin/socketClose.
9. EthernetClient.stop -> beginStopAsync/pollStopAsync.

Riesgos conocidos:
10. Modbus Client closeSession descarta handle tras mutex fallido.
11. Modbus Server dropClient descarta handle tras mutex fallido.
12. Reset físico ocurre antes del mutex en probeHardware.
13. server_port[] mantiene mapa de listeners sin epoch/owner.
14. !isReady() no certifica reset y descarta handles Modbus.

Restricciones:
15. Mutex core no recursivo.
16. GPIO4 cableado frente al default firmware 255.
17. W5100.init usa guard static initialized.
18. Client/Server/UDP forman parte de API pública documentada.
19. `EthernetClient::operator bool` solo consulta índice de socket.

Este gate **NO** es un analizador C++ completo de grafo de llamadas
y no demuestra condiciones de carrera bajo concurrencia real.
Es una red de testigos source-backed para guiar diseño y
detectar drift. Los tests negativos mutan copias en memoria.

## 3. Próximo paso P1G-C — diseño experimental de solución

### Contrato mínimo obligatorio

El candidato debe incorporar un mecanismo que:
- coordine las entradas directas `EthernetClient/Server/UDP`
  con los wrappers ya protegidos, sin segundo acquire de
  mutex FreeRTOS no recursivo;
- no deje `SPI.beginTransaction` sin ownership efectivo
  en caminos públicos, DHCP/DNS y helpers de socket;
- preserve la semántica de legacy `connect/stop/write/read`,
  y las extensiones cooperativas;
- registre lease de socket con generación real W5500,
  contador de **asignación** y propietario,
  compatible con copias `EthernetClient` retornadas
  por `Server.available/accept`;
- impida comandos de cierre a un socket reasignado
  luego de reset o dentro de la misma generación;
- invalide la tabla `server_port[]` al reset efectivo;
- NO interprete LINK_OFF, !isReady o contention SPI
  como reset físico demostrado;
- resuelva `W5100.init` guard si se produce reset;
- tenga interlock real entre reset GPIO4 y operaciones
  W5500; el default GPIO4 no se promueve antes de validar;
- preserve Display/SD/FRAM y la latencia/fairness del
  SPI compartido; no introducir locks recursivos globales
  sin análisis de memoria, inversión y regresión.

### Orden de adopción

P1G-C1: diseño detallado y candidato temporal source-first;
C2: nativo C++ sobre métodos reales, escenarios negativos
contra epoch, owner, reutilización de socket y lock inversion;
C3: compilación Arduino baseline/candidato y
precompilados/periféricos; C4: autorización y validación física
separada; C5: adopción o rechazo registrado.

Sin cambios productivos en P1G-A ni P1G-B. No declarar
`A13-006 FIXED` por resultados de caracterización.

## 4. Reproducción Windows del siguiente gate

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_b_callgraph_contract.bat
```

Salida final esperada:

```text
G10_P1G_B_SELFTEST=PASS_FIVE_NEGATIVES
G10_P1G_B_WITNESSES=19
G10_P1G_B_KNOWN_HAZARDS=5
G10_P1G_B_ARCHIVES_FROZEN=5
G10_P1G_B_STATUS=CALLGRAPH_WITNESSES_CAPTURED_ADOPTION_BLOCKED
```

`ADOPTION_BLOCKED` se espera porque el código de producto
no está modificado ni validado. Nunca solicitar upload para
este gate. El preflight exige el manifest local P1G-A
aportado por el operador.
