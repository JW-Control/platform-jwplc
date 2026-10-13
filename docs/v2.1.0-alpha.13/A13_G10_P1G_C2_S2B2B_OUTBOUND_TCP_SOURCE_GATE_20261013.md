# Alpha13 G10-P1G-C2-S2B2B — TCP saliente source-backed (corte experimental)

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre local previo S2B2A

El operador ejecutó con árbol de trabajo limpio y
`HEAD=e66021828971d82fa5b78e3319aaa44e977ef150`:

```text
G10_P1G_C2_S2B2A_POSITIVE_NATIVE=PASS_REAL_COOPERATIVE_STOP_METHODS
G10_P1G_C2_S2B2A_NEGATIVE_NO_BEGIN_GUARD=PASS_REJECTED
G10_P1G_C2_S2B2A_NEGATIVE_NO_OWNER_VALIDATION=PASS_REJECTED
G10_P1G_C2_S2B2A_NEGATIVE_BUSY_DROPS_HANDLE=PASS_REJECTED
G10_P1G_C2_S2B2A_NEGATIVE_UNBOUNDED_STOP_LIFECYCLE=PASS_REJECTED
G10_P1G_C2_S2B2A_REAL_CPP_METHODS=3
G10_P1G_C2_S2B2A_OVERLAY_FILES=3
G10_P1G_C2_S2B2A_NEGATIVES=4
G10_P1G_C2_S2B2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B2A_STATUS=COOPERATIVE_STOP_REAL_METHODS_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2B2A_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2b2a_20261013_121841_efdbfb69
```

```text
G10_P1G_C2_S2B2A_WINDOWS_LOCAL=CLOSED_PASS
G10_P1G_C2_S2B2A_CI=PASS_WINDOWS_LINUX_RUN_38436363316
```

## 2. C2-S2B2B — alcance exacto

Se crean los archivos del gate:

- `tools/alpha13/gates/a13_g10_p1g_c2_s2b2b_outbound_connect.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2b2b_outbound_connect.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2b2b_outbound_connect_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2b2b-outbound-connect.yml`

El gate genera bajo `tools/alpha13/results/` un overlay
de las tres unidades heredadas de S2B1 más los métodos
de cierre S2B2A. **No modifica** las fuentes
`JWPLC/2.1.0/` del package activo.

Sobre `EthernetClient.cpp` extrae, modifica y compila
en un ejecutable host los **cuatro cuerpos C++ reales**:

1. `beginConnectAsync(IPAddress,uint16_t)`
2. `pollConnectAsync()`
3. `connectAsyncInProgress()`
4. `cancelConnectAsync()`

El header overlay añade campos privados experimentales
`_jwplcDeferredConnect`, `_jwplcDeferredIp`,
`_jwplcDeferredPort`. Conserva los parámetros y firmas
públicos (no compila todavía el package completo).
El estado `_jwplcSocketToken` viene del corte S2B1.

### Comportamiento experimental probado

- El mutex del core SPI no recursivo se adquiere una
  sola vez por ámbito W5500; se comprueba ownership
  antes de `cancelWriteAsync()` porque esa función
  puede emitir `socketClose()`.
- Ante `SPI_BUSY` al comenzar, se conserva IP/puerto
  en estado diferido; `pollConnectAsync()` puede
  reintentar cuando vuelva a estar disponible el bus.
  No se reporta `-1` por simple contención.
- Después de `socketBegin()` se captura y verifica
  lease con generación, asignación y propietario
  antes de emitir `Sock_CONNECT`.
- Si un objeto viejo ya no posee el socket, no
  puede ejecutar DISCONNECT ni CLOSE de otro dueño.
- En `cancelConnectAsync()`, `SPI_BUSY` conserva
  el handle; una cancelación posterior puede cerrar
  la asignación legítima.
- `pollConnectAsync()` con socket ya asignado
  y SPI temporalmente ocupado devuelve pendiente,
  no desconexión. El reset verdadero invalida token;
  polling de token viejo no toca hardware.

Se compilan también `JWPLC_W5500_SafeAccess.cpp`
y un arnés nativo de SPI/W5500 simulados. La asignación
`socketBegin` de este arnés es mock, con el contrato
compilado en S2A por separado. **No** se compiló en esta
batería el `EthernetClient.cpp` completo integrado con
todos los includes de ESP32 y Arduino.

### Cinco pruebas negativas

El gate compila las mutaciones sobre los cuerpos
modificados, ejecuta los binarios y requiere falla
observable para:

- `REMOVE_BEGIN_GUARD` — cancelar escritura sin
  exclusión W5500.
- `REMOVE_NEW_TOKEN` — apertura sin capturar lease.
- `NO_DEFER_ON_BUSY` — pérdida de conexión pendiente.
- `REMOVE_CANCEL_OWNER` — cancelar un handle obsoleto.
- `POLL_BUSY_FALSE_DISCONNECT` — interpretar
  ocupación SPI como caída del enlace.

La primera CI falló porque un negativo eliminaba el
objeto `bus` y no compilaba; se corrigió para mantener
un `bool bus` que **compila** pero no adquiere
el mutex, de modo que el negativo prueba un fallo de
ejecución. Posteriormente se amplió el harness para
ejercitar BUS_BUSY con un socket ya asignado.

## 3. Evidencia CI final

[GitHub Actions Windows + Linux, run 38440505069](https://github.com/JW-Control/platform-jwplc/actions/runs/38440505069)

```text
G10_P1G_C2_S2B2B_CI=PASS_WINDOWS_LINUX_RUN_38440505069
G10_P1G_C2_S2B2B_CI_HEAD=da4fca4effa0580510ec6d102c9c05b4831c8b22
G10_P1G_C2_S2B2B_REAL_CPP_METHODS=4
G10_P1G_C2_S2B2B_OVERLAY_FILES=3
G10_P1G_C2_S2B2B_NEGATIVES=5_PASS
G10_P1G_C2_S2B2B_ARCHIVES=5_FROZEN
G10_P1G_C2_S2B2B_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2B2B_STATUS=OUTBOUND_CONNECT_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2B2B_PRODUCT_MODIFICATIONS=NONE
G10_P1G_C2_S2B2B_ARDUINO_BUILD=NOT_RUN
G10_P1G_C2_S2B2B_HARDWARE=NOT_TOUCHED
G10_P1G_C2_S2B2B_GPIO4_ENABLED=NO
```

## 4. Riesgos todavía pendientes — bloqueo obligatorio de adopción

El pase del slice **NO autoriza A13-006 ni release**.

1. `connect(IPAddress)` Arduino legacy es bloqueante.
   Si al vencer timeout `cancelConnectAsync()`
   no obtiene SPI, su petición o el socket pueden
   seguir vivos. Falta política determinista y
   test de limpieza ulterior sin afectar usuario.
2. `connect(const char*)` utiliza DNS y maneja
   una sesión previa por índice sin lease. Todavía
   no se protege esa ruta completa.
3. `cancelWriteAsync()` puede emitir CLOSE y
   modificar `_sockindex` dentro del scope.
   La invalidación de lease y consistencia con
   send/flush/close deberán probarse sobre todos
   los caminos TX/RX. **No** se asume resuelto.
4. `connected()`, `available/read/write`,
   `socket.cpp` completo, `Server`, UDP,
   DHCP/DNS, listeners y reset requieren gate de
   integración; el uso aislado de cuatro métodos
   no demuestra cobertura global.
5. `SPI_BUSY` no equivale a link off, reset
   ni cierre físico. La política de reintento
   debe preservar límites de tiempo y fairness
   de Display/SD/FRAM/RTC.
6. Se exige un overlay **Arduino IDE/CLI compilable**
   con todos los periféricos normales antes de
   considerar un cambio productivo.
7. GPIO4 reset físico continúa deshabilitado
   (`JWPLC_ETH_RESET_PIN=255`) hasta validación
   de interlock y fairness. No se ha hecho upload.

## 5. Reproducción Windows local

El gate exige como preflight el manifest local PASS
de S2B2A `g10_p1g_c2_s2b2a_20261013_121841_efdbfb69`.

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2b2b_outbound_connect.bat
```

Salida final esperada:

```text
G10_P1G_C2_S2B2B_POSITIVE_NATIVE=PASS_REAL_OUTBOUND_LIFECYCLE
G10_P1G_C2_S2B2B_REAL_CPP_METHODS=4
G10_P1G_C2_S2B2B_OVERLAY_FILES=3
G10_P1G_C2_S2B2B_NEGATIVES=5
G10_P1G_C2_S2B2B_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B2B_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B2B_STATUS=OUTBOUND_CONNECT_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

## 6. Próximo gate recomendado

`C2-S2C`: formalizar semántica legacy `connect()`/DNS
y limpieza diferida por contención, además de
`cancelWriteAsync()` / `write/read`. No iniciar
integración productiva antes de confirmar en nativo
los caminos completos y ejecutar la compilación Arduino
baseline vs overlay (todos los periféricos, sin quitarlos
del autoload).

Estado de `A13-006=ADOPTION_BLOCKED`,
`G10_FINAL=NOT_CLOSED`, Alpha13 sigue abierta.
