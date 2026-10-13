# Alpha13 G10-P1G — cierre C2-S1 local y avance C2-S2A

Fecha: 2026-10-13. Rama: `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. C2-S1 — evidencia local

El operador validó con checkout inicialmente limpio y
`HEAD=edbb0082f4277b1ab651da370328788ed7206d23`:

```text
G10_P1G_C2_S1_POSITIVE_NATIVE=PASS_REAL_EIGHT_METHODS
G10_P1G_C2_S1_NEGATIVE_NO_RAW_CLIENT_SCOPE=PASS_REJECTED
G10_P1G_C2_S1_NEGATIVE_NO_SOCKET_SCOPE=PASS_REJECTED
G10_P1G_C2_S1_NEGATIVE_MB_CLIENT_RAW_SPI_LOCK=PASS_REJECTED
G10_P1G_C2_S1_NEGATIVE_MB_SERVER_RAW_RELEASE=PASS_REJECTED
G10_P1G_C2_S1_METHODS_REAL_PATCHED=8
G10_P1G_C2_S1_OVERLAY_FILES=5
G10_P1G_C2_S1_NEGATIVES=4
G10_P1G_C2_S1_FROZEN_ARCHIVES=5
G10_P1G_C2_S1_PRODUCT_CHANGED=NO
G10_P1G_C2_S1_STATUS=REAL_SOURCE_MUTEX_SLICE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S1_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s1_20261013_101730_2d1e241a
```

```text
G10_P1G_C2_S1_WINDOWS_LOCAL=CLOSED_PASS
G10_P1G_C2_S1_CI=PASS_WINDOWS_LINUX_RUN_38414490706
```

S1 valida solo ocho métodos de mutex/SPI; no soluciona
lifecycle/owner, reset físico, BUS_BUSY genérico ni todos
los caminos TCP/UDP.

## 2. C2-S2A — unión experimental al socketBegin real

Se amplió **únicamente el candidato aislado** bajo
`tools/alpha13/candidates/g10_p1g_c/`:

- `allocateSocketLockedAuto(socket)`: asigna un cookie nuevo
  y lease con `{chipEpoch,allocation,ownerCookie,socketIndex}`
  dentro del scope W5500, sin pasar ownership todavía
  a `EthernetClient` ni `EthernetUDP`.
- `currentSocketTokenLocked(socket)`: permite extraer
  el token solo si el task posee el guard y la asignación
  está vigente. No concede ownership por sí mismo.
- La API compartida `jwplc_spi_bus.h` productiva **no se modifica**.

El nuevo gate:

```text
tools/alpha13/gates/a13_g10_p1g_c2_s2a_socket_begin_lease.py
tools/alpha13/gates/run_a13_g10_p1g_c2_s2a_socket_begin_lease.bat
```

Genera un overlay **sin tocar**
`JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp`,
extrae `EthernetClass::socketBegin(uint8_t,uint16_t)` modificado
y lo compila realmente en host Windows/Linux junto al
`JWPLC_W5500_SafeAccess.cpp` candidato.

Comprobaciones nativas:

1. `socketBegin()` directo se bloquea bajo el mutex SPI
   compartido, sin adquisición anidada FreeRTOS cuando
   el propio W5500 ya tiene el scope.
2. Solo después del `Sock_OPEN` efectivo se registra el
   lease; al reabrir el mismo índice, cambia asignación
   y deja obsoleto el token antiguo.
3. El `server_port[s]` legacy se limpia al iniciar una
   nueva asignación; no conserva el listener anterior.
4. Intento de cierre diferido con token anterior se rechaza
   por `closeIfCurrent()`, sin alcanzar el callback hardware.
5. `BUS_BUSY` de otro task no realiza transacciones W5500,
   no abre socket ni genera token.
6. `resetAndInvalidate()` invalida token y listener incluso
   si se intenta usar el handle anterior después del reset.

Cuatro negativos contra el **método real en overlay**:
`REMOVE_SOCKET_BEGIN_GUARD`,
`REMOVE_SOCKET_LEASE`,
`KEEP_LEGACY_LISTENER_MAP`,
`SKIP_HARDWARE_OPEN`.
Todos deben compilar y fallar la prueba de comportamiento
esperada si se altera la ruta.

CI definitiva:
https://github.com/JW-Control/platform-jwplc/actions/runs/38422090537

```text
G10_P1G_C2_S2A_CI=PASS_WINDOWS_LINUX
G10_P1G_C2_S2A_CI_HEAD=83f0c874ab339e26a5560056551d72fdaff6cd2c
G10_P1G_C2_S2A_SOURCE_METHODS=1
G10_P1G_C2_S2A_NATIVE_NEGATIVES=4_PASS_REJECTED
G10_P1G_C2_S2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2A_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2A_PRODUCT_MUTATION=NONE
G10_P1G_C2_S2A_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2A_HARDWARE=NOT_TOUCHED
G10_P1G_C2_S2A_STATUS=REAL_SOCKET_BEGIN_LEASE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

## 3. Límites estrictos

**S2A no corrige todavía A13-006**:

- Los tokens asignados por `socketBegin()` todavía NO
  se conservan en los objetos `EthernetClient`,
  `EthernetServer` ni `EthernetUDP`. Tampoco tienen
  validación de identidad los `stop/cancel` reales.
- El `server_port[]` sigue siendo el mapa productivo;
  falta asociar puerto con token y limpiar cuando
  efectivamente cambia generación de W5500.
- `socketBeginMulticast()` y el resto de llamadas
  Ethernet/DHCP/DNS no están en este slice.
- Ninguna operación destructiva del producto está aún
  sincronizada con `closeIfCurrent()`.
- `BUS_BUSY` y el estado de `connected()` tienen
  semántica todavía no resuelta para la API directa.
- El reset GPIO4 no está habilitado, y la retención de
  bus durante ~90 ms sigue sin gate de fairness real.
- Falta compilar un overlay Arduino integrado. CI nativa
  con mocks FreeRTOS/SPI no sustituye Arduino CLI ni
  pruebas físicas SD, FRAM, TFT, RTC y Modbus.
- Estos gates no prueban RTOS scheduling real ni
  todos los call-sites. No extrapolar a libre de races.

## 4. Próximo corte S2B

Conectar el token a `EthernetClient` y copias devueltas
por `EthernetServer.available()/accept()`, preservando
source/API Arduino y evitando que una copia obsoleta
ejecute `CLOSE` sobre un socket reciclado.

Para `stop()/cancelStopAsync()`, la validación de
identidad y operación hardware deben ser atómicas bajo
un mismo scope. Ante SPI_BUSY conservar handle/pendiente
y reintentar, sin interpretar la contención como reset.
Añadir UDP por separado, incluyendo DHCP.

Se prohíbe activar GPIO4 por default o adoptar estos
overlays antes de cerrar todos los bloques y los gates
Arduino y físicos.

## 5. Reproducción local siguiente

Desde PowerShell en `platform-jwplc`:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2a_socket_begin_lease.bat
```

Salida esperada:

```text
G10_P1G_C2_S2A_POSITIVE_NATIVE=PASS_REAL_SOCKET_BEGIN_LEASE
G10_P1G_C2_S2A_NEGATIVES=4
G10_P1G_C2_S2A_REAL_METHODS=1
G10_P1G_C2_S2A_ARCHIVES_FROZEN=5
G10_P1G_C2_S2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2A_STATUS=REAL_SOCKET_BEGIN_LEASE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

Solo ejecutar el BAT después de verificar checkout limpio.
El gate exige manifest `C2-S1` local aportado
por el operador (run `g10_p1g_c2_s1_20261013_101730_2d1e241a`).
