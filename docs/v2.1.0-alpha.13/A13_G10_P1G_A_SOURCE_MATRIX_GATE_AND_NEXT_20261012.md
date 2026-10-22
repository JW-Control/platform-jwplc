# Alpha13 — G10-P1G-A: matriz reproducible de acceso SPI y siguiente candidato

Fecha de ejecución CI: 2026-10-12 (Perú) / 2026-10-13 (UTC).

## 1. Estado

```text
G10_P1G_A_CI=PASS_WINDOWS_LINUX
G10_P1G_A_CI_RUN=38339370374
G10_P1G_A_CI_HEAD=355572d39f6001c82b7044ed4dcf0daee1894f7e
G10_P1G_A_WINDOWS_LOCAL=PENDING
G10_P1G_A_PRODUCT_CHANGE=NONE
G10_P1G_A_HARDWARE=NOT_TOUCHED
G10_P1G_A_ADOPTION=BLOCKED
A13-006=REVISION_REQUIRED
G10_FINAL=OPEN
```

[Ver CI Windows/Linux](https://github.com/JW-Control/platform-jwplc/actions/runs/38339370374).

El gate `tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py`:

- inspecciona 8 `.cpp` productivos e incluye headers,
  mutex FreeRTOS, boards.txt e init periféricos como contratos;
- verifica 19 métodos operativos con mutex explícito de
  la capa runtime/Modbus;
- detecta 81 definiciones inferiores que tienen referencia
  directa a backend/SPI sin adquisición **local**;
- valida rutas positivas Modbus Server/Client y rutas directas
  EthernetClient/Server/UDP;
- detecta el problema de reset físico antes del lock,
  el valor default de firmware 255 frente a GPIO4 de hardware,
  y el mutex FreeRTOS no recursivo;
- conserva hashes de cinco archives definidos por el gate P1F;
- escribe `CALL_PATHS.csv` + `MANIFEST.json`;
- ejecuta cuatro negativos read-only:
  cambio de tipo de mutex, cambio a GPIO4, cambio de orden
  de reset y adición local no coordinada de mutex en Client.

No se manipula la placa ni se reemplazan los fuentes de producto.

## 2. Interpretación de los recuentos

```text
HIGH_LEVEL_LOCAL_ACQUIRE=11_4_4
LOWER_DIRECT_WITHOUT_LOCAL_ACQUIRE=24_5_14_20_18
DETECTED_DIRECT_TOTAL=81
TRUE_UNPROTECTED_PATHS=NOT_YET_CLASSIFIED
```

La matriz CSV no es un analizador semántico completo de C++:
no resuelve llamadas virtuales, funciones libres, headers inline,
destructores o encadenamientos recursivos. Cuenta métodos con
operaciones **directas** y no clasifica automáticamente carreras.
La cobertura de llamadas indirectas es objeto obligatorio de P1G-B.

No confundir `EthernetClient::operator bool()`, que
solo inspecciona `_sockindex`, con `connected()`,
que alcanza `Ethernet.socketStatus()`.

## 3. Próxima fase P1G-B: grafo efectivo y candidato aislado

### Bloque B0 — Call-graph y orden de locks

Construir para caminos `JWPLC_Ethernet` / `JWPLC_ModbusTCP`
y API Arduino-compatible directa:

```text
task/loop -> wrapper actual -> acquire SPI -> EthernetClient/Server/UDP
          -> Ethernet.socket* -> W5100 -> SPI transaction
```

Cobertura mínima: DHCP initial/renew, DNS, UDP, Client
connect/read/write/flush/stop, Server begin/available/accept/stop
(si existe), RX INT y operaciones de reset/reconfigure.

Revisar mutex no recursivo y cualquier camino donde se intente
tomar otra vez mientras el mismo task lo posee. Antes de elegir
mecanismo reentrante o barrera W5500, medir coste y comparar
todas las rutas. No sustituir globalmente la política del SPI
compartido por intuición.

### Bloque B1 — Identidad de sockets y generación

Un token `{chip_epoch, socket_allocation_token, owner}`
debe proteger contra:

1. reasignación del índice en la misma generación;
2. reasignación después de reset real;
3. cierre `stop/cancel` diferido durante contención;
4. listeners `server_port[]` obsoletos;
5. confundir `!isReady()` (por LINK_OFF / mutex)
   con reset físico/lógico;
6. acceso a un socket que pierde ownership entre comprobación
   y operación destructiva (TOCTOU).

Tanto la comprobación de identidad como la operación
destructiva deben quedar bajo la misma exclusión efectiva.

### Bloque B2 — Reset GPIO4

Hardware real: `RST-ETH=GPIO4`. Firmware actual:
`JWPLC_ETH_RESET_PIN=255`, sin override en
`boards.txt` del Basic normal. No fijar `4` hasta
que exista mecanismo de quiescencia/interlock validado,
incluyendo interacciones con `JWPLC_Display`, FRAM y SD.

Comparar barrera específica W5500 frente a mantener el
mutex SPI durante el reset (aprox. 90 ms), sin presumir
que la primera es segura por sí sola: se debe demostrar
ausencia de lock inversion y de acceso directo no registrado.

### Bloque B3 — Reproducciones y aceptación

- pruebas negativas que eliminen epoch/owner/interlock;
- pruebas nativas basadas en métodos C++ modificados reales,
  no solo modelos semánticos;
- baseline/candidato Arduino builds y procedencia exacta;
- validación de latencias SPI y fairness, no solo throughput;
- ningún GPIO4, firmware ni reflash antes de gate físico
  acordado independientemente.

## 4. Comando local de confirmación

En PowerShell, con checkout limpio y sin modificaciones
productivas inesperadas:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_a_mutex_callpaths.bat
```

Esperado:

```text
G10_P1G_A_SELFTEST=PASS_4_NEGATIVES
G10_P1G_A_TOTAL_HIGH_LEVEL_LOCAL_GUARDS=19
G10_P1G_A_LOWER_DIRECT_TOTAL_NOT_BUG_COUNT=81
G10_P1G_A_RESET_HW_GPIO=4
G10_P1G_A_RESET_FIRMWARE_DEFAULT=255
G10_P1G_A_ARCHIVES_FROZEN=5
G10_P1G_A_STATUS=CALLPATH_BASELINE_CAPTURED_ADOPTION_BLOCKED
```

El estado `ADOPTION_BLOCKED` es esperado: PASS confirma
caracterización, no corrección implementada ni adoptada.
