# Alpha13 G10 — P0 Windows PASS y P1A reproducción A13-006

**Fecha:** 2026-10-12. **Alpha13:** `IN_PROGRESS`; G9 cerrado.
**Estado P0:** `BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN`, confirmado
por consola Windows y CI [38278669640](https://github.com/JW-Control/platform-jwplc/actions/runs/38278669640).
**Estado P1A:** reproducción fuente-backed preparada; la prueba
en hardware **no está solicitada** y no se han adoptado cambios
productivos.

## P0 local reportado

```text
G10_P0_HEAD=bd6787711c1480f9abf2fe79ca16352e03ad4e02
G10_P0_A13_006_SOCKET_DROP_PATTERN=True
G10_P0_A13_007_DHCP_PARTIAL_PATTERN=True
G10_P0_A13_008_UDP_UNBOUNDED_WAIT_PATTERN=True
G10_P0_A13_015_TCP_SMOKE_PRESENT=False
G10_P0_A13_015_RTU_MASTER_SMOKE_PRESENT=False
G10_P0_A13_016_GIT_EPOCH_POLICY_PRESENT=True
G10_P0_A13_018_BIN_SUM_PRESENT=True
G10_P0_FROZEN_ARCHIVES=5
G10_P0_STATUS=BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN
G10_P0_RUN_ID=g10_p0_20261012_114921_34102c6b
```

La existencia de estos patrones **no implica** que todo
comportamiento de red esté roto; demuestra que los riesgos
canónicos indicados requieren resolución. El último
RUN_ID es evidencia local del operador, no manifest
descargado desde su PC.

## Diagnóstico de A13-006 contra fuentes activas

La auditoría identifica
`JWPLC_ModbusTCP_Client.cpp::closeSession()`.
La ruta actual es:

```cpp
if ((bool)_tcpClient && acquireBus(20))
{
    _tcpClient.cancelConnectAsync();
    releaseBus();
}
_tcpClient = EthernetClient();
```

Cuando `acquireBus(20)` falla, no se llama
`cancelConnectAsync()` pero se descarta el valor
del identificador TCP. El destructor público de
`EthernetClient` está vacío y no libera el socket.
Por tanto, la pérdida de ownership es una posibilidad
real de la implementación, no una advertencia
puramente estilística.

La ruta de servidor
`JWPLC_ModbusTCP.cpp::dropClient()` presenta
el mismo patrón con `_clientActive` y
`_client.stop()` bajo lock SPI; tras fallar,
se descarta `_client`. Se incluye en el
diagnóstico, pero **no implica autorización
automática para modificar dos subsistemas a la vez**.

Dependencias verificadas: `begin()` y `end()`
invocan `closeSession()`; `fail(error,true)`
también; `task()` retorna cuando no hay
operación ocupada y, si Ethernet no está READY,
descarta el cliente anterior. Una corrección
local al `if` que conserve el handle sin
revisar esos caminos puede dejar un socket sin
reintento o mezclar conexión nueva y anterior.

## P1A — Reproducción determinista sin hardware

Script versionado:
`tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py`.

El script extrae los cuerpos de las funciones
**desde las fuentes actuales** y los compila como
C++ host con `EthernetClient`, bus SPI y estado
W5500 simulados. Se comprueba:

1. Cliente: lock SPI fallido deja socket abierto
   sin handle, detectando la reproducción del defecto.
2. Cliente: lock SPI obtenido permite cancelar el
   socket; no queda huérfano.
3. Cliente: ausencia de socket no debe tomar lock.
4. Servidor: lock SPI fallido pierde handle del
   cliente TCP sin cerrar socket.
5. Servidor: lock obtenido cierra y libera.
6. Servidor: sin cliente activo no toma lock.

**Límite de evidencia:** se ejecuta el cuerpo C++
real pero el hardware y las implementaciones de
socket/bus son mocks; no mide consumo de sockets
reales W5500 ni confirma una solución. Si pasa,
el marcador debe indicar `BASELINE_DEFECT_REPRODUCED_NOT_FIXED`.

La CI usa C++ nativo Windows/Linux. El BAT
`tools/alpha13/gates/run_a13_g10_p1a_a13_006_repro.bat`
realiza solo extracción, fingerprint y procedencia
contra el G10-P0 local; no necesita compilador host
en Windows y **no equivale a PASS del ejecutable
nativo de CI**.

## Decisión siguiente después de los dos gates

A13-006 debe permanecer `REVIEW_OPEN` hasta
concluir contrato de corrección y adoptar/validar.
La estrategia candidata debe preservar el identificador
hasta que el cierre sea posible y bloquear su reutilización
antes del cierre; también debe definir funcionamiento
de `begin`, `end`, `task`, `fail` y reset de Ethernet.
La semántica del listener/servidor se decidirá
por separado o como gate hermano, con su propia evidencia.

No se debe cerrar con un retry infinito o lock
SPI bloqueante que perjudique el servicio industrial.

```text
G9=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0=PASS_READONLY_WINDOWS
G10_P1A_A13-006=REPRO_IN_PREPARATION
G10_P1A_PRODUCT_CHANGE=NONE
G10_P1A_HARDWARE_ACCESS=NONE
G10_A13-007=REVIEW_OPEN
G10_A13-008=REVIEW_OPEN
G10_A13-015_016_018=ADOPT_PENDING
G10_FINAL=IN_PROGRESS
F120_ROOT_CAUSE=UNKNOWN_PENDING
RELEASE_MERGE_PR_PUBLICATION=NO
```
