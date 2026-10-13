# Alpha13 G10-P1G — S2A local PASS y S2B1 Client/Server C++ source-backed

Fecha: 2026-10-13 (Perú). Rama:
`v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre local S2A

El operador ejecutó el gate en Windows con checkout limpio,
`HEAD=d40b7ae25e6dc5e4b54034766abaa13b11bd7dba`:

```text
G10_P1G_C2_S2A_POSITIVE_NATIVE=PASS_REAL_SOCKET_BEGIN_LEASE
G10_P1G_C2_S2A_NEGATIVE_REMOVE_SOCKET_BEGIN_GUARD=PASS_REJECTED
G10_P1G_C2_S2A_NEGATIVE_REMOVE_SOCKET_LEASE=PASS_REJECTED
G10_P1G_C2_S2A_NEGATIVE_KEEP_LEGACY_LISTENER_MAP=PASS_REJECTED
G10_P1G_C2_S2A_NEGATIVE_SKIP_HARDWARE_OPEN=PASS_REJECTED
G10_P1G_C2_S2A_REAL_METHODS=1
G10_P1G_C2_S2A_NEGATIVES=4
G10_P1G_C2_S2A_ARCHIVES_FROZEN=5
G10_P1G_C2_S2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2A_STATUS=REAL_SOCKET_BEGIN_LEASE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2A_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2a_20261013_113859_460838be
```

```text
G10_P1G_C2_S2A_WINDOWS_LOCAL=CLOSED_PASS
G10_P1G_C2_S2A_CI=PASS_WINDOWS_LINUX_RUN_38422090537
```

El cierre corresponde al **lease en socketBegin** en overlay, no
a las APIs de red completas.

## 2. S2B1: alcance ejecutado

Se versionaron:

- `tools/alpha13/gates/a13_g10_p1g_c2_s2b1_client_server_lease.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2b1_client_server_lease.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2b_client_server_lease_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2b1-client-server.yml`

Y se añadió al **candidato aislado C1** el helper
`invalidateSocketLocked(token)`, que invalida el lease
solo cuando su propietario es vigente y el mutex SPI
está tomado. No cambia el core ni el package productivo.

El generador S2B1 crea overlay read-only de tres archivos
del package, dejando el original intacto:

| Archivo real | Cambio **solo en overlay** |
|---|---|
| `JWPLC_W5x00_Ethernet.h` | Constructor `EthernetClient(uint8_t)` captura token bajo guard; añade campo privado `_jwplcSocketToken` |
| `EthernetClient.cpp` | `cancelStopAsync()` conserva handle bajo BUS_BUSY y valida lease antes de cancelar o cerrar |
| `EthernetServer.cpp` | `begin()` registra listener con token; `available()/accept()` hacen escaneo y devolución bajo el mismo guard, con filtro de listener válido |

La copia estándar `EthernetClient copy=original` conserva
el token (copia por valor de la estructura), de modo que
una copia vieja NO tiene autorización para cerrar
la asignación nueva del mismo índice.

### Pruebas ejecutadas

El harness compila **cuatro cuerpos C++ reales del overlay**
(`EthernetClient::cancelStopAsync` y los tres de Server)
y **un constructor extraído del header del overlay**.
Utiliza C1 W5500 lease real y mock de hardware/SPI;
el `socketBegin` dentro de este harness es un mock
alineado al contrato de S2A (S2A comprobó su método
real en gate separado).

Escenarios:

1. `Server.available()` devuelve Client con token,
   y la copia del handle no puede cerrar un socket
   reciclado.
2. `Server.accept()` devuelve Client con token;
   `cancelStopAsync()` bajo BUS_BUSY conserva handle
   para reintento; después cierra una vez.
3. Una segunda copia tras el cierre ya no puede cerrar
   nuevamente el socket anterior.
4. Reset verdadero invalida lease y el antiguo Client
   deja de ser autorizado para cerrar.
5. Escaneo Server y captura de token ocurren bajo
   un guard W5500 compartido, sin segunda toma
   del mutex de core.

Cuatro negativos nativos:

- `CTOR_NO_TOKEN` — no captura token en constructor;
- `CANCEL_NO_LEASE_CHECK` — permite cierre con lease inválido;
- `SERVER_AVAILABLE_UNLOCKED` — escaneo pierde
  el guard y ya no reconoce al Client válido;
- `BUS_BUSY_DROPS_HANDLE` — pierde handle por
  contención SPI.

Las primeras CI fallaron por anchors y expectativa
del arnés de negativos; la **CI definitiva** pasó Windows
y Linux sin modificar producto:

https://github.com/JW-Control/platform-jwplc/actions/runs/38433524896

```text
G10_P1G_C2_S2B1_CI=PASS_WINDOWS_LINUX_RUN_38433524896
G10_P1G_C2_S2B1_CI_HEAD=0acb927cfebe43460ae2a758c8002c17d1454a16
G10_P1G_C2_S2B1_REAL_METHODS=4
G10_P1G_C2_S2B1_REAL_HEADER_CTORS=1
G10_P1G_C2_S2B1_OVERLAY_FILES=3
G10_P1G_C2_S2B1_NEGATIVES=4_PASS
G10_P1G_C2_S2B1_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2B1_PRODUCT_FILES=UNCHANGED
G10_P1G_C2_S2B1_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2B1_HARDWARE=NOT_TOUCHED
G10_P1G_C2_S2B1_STATUS=CLIENT_SERVER_TOKEN_CANCEL_SOURCE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

## 3. Pendientes antes de adoptar

**C2-S2B1 no resuelve A13-006 todavía.** En particular:

- `EthernetClient::beginConnectAsync/pollConnectAsync`
  saliente no captura ni verifica lease.
- `beginStopAsync()/pollStopAsync()/stop()` y caminos
  de flush/write siguen pendientes. Un SPI_BUSY
  no puede representar un cierre o desconexión real.
- `connected()/available()/read()/write()/operator==`
  necesitan semántica coherente con token; el slice
  C2-S1 había devuelto `connected()==0` bajo BUS_BUSY,
  **no aceptable como política definitiva**.
- `EthernetServer::server_port[]` sigue existiendo;
  faltan integración y pruebas de invalidación real
  ante reset físico y los caminos de reconfiguración.
- UDP, DHCP, DNS y operaciones W5100/Socket
  no están integralmente cubiertas.
- Falta Arduino CLI/IDE baseline vs candidate,
  perfil de latencia/fairness SD/FRAM/TFT,
  y validación física controlada.
- GPIO4 `RST-ETH` permanece deshabilitado por
  defecto hasta contar con reset seguro y probado.

Ninguna conclusión sobre latencias Alpha12
puede extraerse de estos tests nativos.

## 4. Ejecución local en Windows

Con checkout limpio:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2b1_client_server_lease.bat
```

Esperado:

```text
G10_P1G_C2_S2B1_POSITIVE_NATIVE=PASS_REAL_CLIENT_SERVER_TOKEN_AND_CLOSE
G10_P1G_C2_S2B1_REAL_METHODS=4
G10_P1G_C2_S2B1_HEADER_CONSTRUCTOR=1
G10_P1G_C2_S2B1_OVERLAY_FILES=3
G10_P1G_C2_S2B1_NEGATIVES=4
G10_P1G_C2_S2B1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B1_STATUS=CLIENT_SERVER_TOKEN_CANCEL_SOURCE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

`ADOPTION_BLOCKED` es intencional. El gate exige
el manifest local C2-S2A con ID
`g10_p1g_c2_s2a_20261013_113859_460838be`.

Siguiente hito C2-S2B2: cliente saliente, close
cooperativo y manejo de BUS_BUSY sobre métodos
productivos reales, manteniendo prototipo en overlay.
