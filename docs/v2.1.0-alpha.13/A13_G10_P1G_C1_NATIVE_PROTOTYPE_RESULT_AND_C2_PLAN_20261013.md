# Alpha13 G10-P1G — cierre B local y candidato C1 nativo

Fecha de documentación: 2026-10-13 UTC. Rama:
`v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre P1G-B Windows local

El operador ejecutó sobre `HEAD=6ef3a7fa5b621affff20a3e3673a9b5d601ea71e`:

```text
G10_P1G_B_SELFTEST=PASS_FIVE_NEGATIVES
G10_P1G_B_WITNESSES=19
G10_P1G_B_KNOWN_HAZARDS=5
G10_P1G_B_ARCHIVES_FROZEN=5
G10_P1G_B_STATUS=CALLGRAPH_WITNESSES_CAPTURED_ADOPTION_BLOCKED
G10_P1G_B_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_b_20261012_234850_ab1c26f8
```

```text
G10_P1G_B_LOCAL=PASS_CLOSED
G10_P1G_B_CI_WINDOWS_LINUX=PASS_RUN_38346137383
G10_P1G_B_PRODUCT_CHANGE=NONE
```

El gate B se cerró como inventario fuente, no como
corrección ni prueba física.

## 2. C1 — primera implementación C++ experimental aislada

Se creó fuera del package activo, en
`tools/alpha13/candidates/g10_p1g_c/`:

- `JWPLC_W5500_SafeAccess.h`
- `JWPLC_W5500_SafeAccess.cpp`
- `native_safe_access_test.cpp`

El prototipo usa el mutex compartido **sin modificar**
`jwplc_spi_bus.cpp`, cuyo lock FreeRTOS sigue siendo
no recursivo. Gestiona una capa anidada **específica del W5500**
por propietario/task, sin adquirir nuevamente el mutex SPI
cuando el propietario W5500 ya lo posee. Si el mutex está
ocupado por SD/FRAM u otro usuario del SPI en el mismo task
pero NO dentro de un scope W5500, la entrada falla de
forma conservadora; no se declara protección implícita.

Incluye `SocketToken` con:
`chipEpoch`, `allocation`, `ownerCookie`, `socketIndex`.

El token se valida bajo el lock **en la misma ventana**
que la operación `closeIfCurrent()` que acciona hardware.
Al no conseguir lock, devuelve `BusBusy` sin cerrar ni
descartar software. Los tokens viejos tras reciclaje
del índice o tras reset devuelven `Stale` y no emiten
CLOSE sobre el nuevo propietario. Listener tiene
port mapping protegido por el mismo lease.

`resetAndInvalidate()` es una variante experimental
conservadora tipo B: toma el mutex SPI completo,
invalida tokens y mantiene la exclusión durante todo
el callback de reset. **Puede retener el bus ~90 ms**
al conectarse a `resetHardwareIfNeeded()`. Es una
prueba de corrección/concurrencia, NO una política
de fairness aceptada para el JWPLC Basic v2.

`LINK_OFF` o `!isReady()` por sí solos NO
incrementan `chipEpoch`.

### CI C1

[GitHub Actions Windows/Linux, run 38349405593](https://github.com/JW-Control/platform-jwplc/actions/runs/38349405593)

```text
G10_P1G_C1_CI=PASS_WINDOWS_LINUX
G10_P1G_C1_NATIVE_POSITIVE=PASS
G10_P1G_C1_NEGATIVE_MUTATIONS=4_OF_4_REJECTED
G10_P1G_C1_PRECOMPILED_ARCHIVES=FROZEN_5
G10_P1G_C1_FIRMWARE_PRODUCT=UNCHANGED
G10_P1G_C1_ARDUINO_COMPILE=NOT_RUN
G10_P1G_C1_HARDWARE_TEST=NOT_RUN
G10_P1G_C1_GPIO4_ENABLED=NO
G10_P1G_C1_STATUS=ISOLATED_BUS_LEASE_PROTOTYPE_NATIVE_PASS_ADOPTION_BLOCKED
```

Cuatro negativos compilados, ejecutados y rechazados:

1. `NESTED_LOCK`: elimina el bypass de adquisición
   anidada; el test nativo detecta el fallo.
2. `REUSE_OWNER`: elimina la comprobación de asignación
   y propietario; el test detecta un token antiguo válido.
3. `RESET_EPOCH`: no incrementa generación de W5500;
   la prueba detecta que el token anterior sigue vigente.
4. `CONTENTION`: pierde el resultado distinguible BUS_BUSY;
   la prueba exige conservar posibilidad de reintento.

Son **pruebas nativas del código C++ candidato real**,
con stubs de FreeRTOS/SPI (no firmware Arduino
completo). Los mecanismos no están conectados a
las clases `EthernetClient`, `EthernetServer`,
`EthernetUDP`, DHCP, DNS ni socket.cpp.
No se afirma ningún fix productivo.

## 3. Plan C2 — integración en overlay source-first

Antes de adoption:

1. **Conectar el guard a los wrappers**:
   `JWPLC_EthernetClass::acquireBus/releaseBus` y los
   wrappers Modbus Server/Client deben usar el guard
   W5500. En caso contrario, una llamada que ya tomó
   el mutex del core no es reconocida como scope W5500
   y las APIs raw anidadas pueden bloquear.
2. **Entrada pública + backend**:
   guard para rutas directas `Client/Server/UDP`,
   también `EthernetClass`, DHCP/DNS y `socket.cpp`,
   sin mantener un lock durante esperas cooperativas.
   Los métodos legacy bloqueantes deben liberar entre
   iteraciones y respetar timeout.
3. **Lease integrado**:
   asignar token solo después de éxito real de
   `socketBegin()`; capturarlo en
   `EthernetClient` y en `EthernetServer.available/accept`.
   Las copias de `EthernetClient` comparten identidad
   de asignación mientras la asignación siga válida.
   Comprobar owner y serial también en reuso del mismo
   índice sin reset; validar antes de stop/cancel.
4. **Listener**:
   reemplazar o versionar `server_port[]`
   sin perder puertos 502 y custom; invalidar
   al reset efectivo.
5. **Reset GPIO4**:
   sigue deshabilitado por defecto. El gate C2 debe
   comparar B (~90 ms SPI retenido) contra barrera
   W5500 A sin generar inversión de locks y
   proteger actualizaciones de epochs.
6. **Pruebas**:
   compilar métodos C++ productivos modificados en
   arnés de concurrencia, mutar/negative,
   compilar baseline/candidato Arduino IDE/CLI y
   proteger FRAM/SD/Display/RTC; chequear
   provenance de .a y no promover 26 MHz/POLLING_C0.

No modificar el package activo hasta superar pruebas
nativas y de compilación del overlay.
El empleo directo de W5100 por código no público
queda fuera del contrato de usuario básico; se
debe inventariar igualmente para proteger rutas
internas.

## 4. Comando local C1

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c1_native_prototype.bat
```

Salida final esperada:

```text
G10_P1G_C1_POSITIVE_NATIVE=PASS_BUS_NESTING_SOCKET_LEASE_RESET
G10_P1G_C1_NEGATIVES=4
G10_P1G_C1_FROZEN_ARCHIVES=5
G10_P1G_C1_INTEGRATED_PRODUCT=NO
G10_P1G_C1_STATUS=ISOLATED_BUS_LEASE_PROTOTYPE_NATIVE_PASS_ADOPTION_BLOCKED
```

El BAT exige manifest local P1G-B con ID
`g10_p1g_b_20261012_234850_ab1c26f8`.
NO flashear, NO conectar GPIO4 todavía.
