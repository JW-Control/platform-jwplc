# G10-P1G-C2-S1 — Primer overlay funcional SPI sobre métodos reales

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Procedencia C1 y cierre

El operador ejecutó C1 con `HEAD=7750917312bfb46cf2b7a1c719abc1b7f8fa5fe4`:

```text
G10_P1G_C1_POSITIVE_NATIVE=PASS_BUS_NESTING_SOCKET_LEASE_RESET
G10_P1G_C1_NEGATIVE_NESTED_LOCK=PASS_REJECTED
G10_P1G_C1_NEGATIVE_REUSE_OWNER=PASS_REJECTED
G10_P1G_C1_NEGATIVE_RESET_EPOCH=PASS_REJECTED
G10_P1G_C1_NEGATIVE_CONTENTION=PASS_REJECTED
G10_P1G_C1_NATIVE_COMPILER=g++.exe
G10_P1G_C1_NEGATIVES=4
G10_P1G_C1_FROZEN_ARCHIVES=5
G10_P1G_C1_INTEGRATED_PRODUCT=NO
G10_P1G_C1_STATUS=ISOLATED_BUS_LEASE_PROTOTYPE_NATIVE_PASS_ADOPTION_BLOCKED
G10_P1G_C1_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c1_20261013_092038_8703b3da
```

El fallo previo de `compile.log` tenía `EXIT=1` sin
stdout/stderr. Es compatible con que GCC UCRT64 estuviera
fuera de PATH, pero no identifica de manera concluyente
el proceso auxiliar que falló. Con PATH temporal inyectado,
`g++.exe` compiló el candidato correctamente.

**C1 local: CLOSED_PASS**. No se modificaron firmware, .a ni hardware.

## 2. Primer bloque de integración C2-S1

Se generó **únicamente en** `tools/alpha13/results/` un overlay
de cinco archivos productivos originales, con inclusión de
`JWPLC_W5500_SafeAccess.h` del candidato y ocho métodos
modificados de forma acotada:

| Archivo | Métodos fuente extraídos y compilados |
|---|---|
| `JWPLC_Ethernet.cpp` | `acquireBus()`, `releaseBus()` |
| `JWPLC_ModbusTCP.cpp` | `acquireBus()`, `releaseBus()` |
| `JWPLC_ModbusTCP_Client.cpp` | `acquireBus()`, `releaseBus()` |
| `EthernetClient.cpp` | `connected()` |
| `socket.cpp` | `socketStatus()` |

**Política del slice:** los tres wrappers Ethernet/Modbus
entran y salen por `jwplc_w5500_candidate::enter/leave`.
`EthernetClient::connected()` y `socketStatus()` utilizan
`BusScope` con RAII, admitiendo anidamiento si ya hay un
propietario de W5500. El mutex SPI compartido **del core no
se modifica** y continúa siendo no recursivo.

Los cuerpos C++ usados en test nativo se **extraen del
overlay fuente**, no son una imitación escrita a mano.
Se usan stubs host para FreeRTOS, SPI, W5100 y el conjunto
de declaraciones mínimas de esas clases.
El gate prueba:

- API directa `connected()` y `socketStatus()` toma lock
  externo al llamar desde sketch;
- llamada desde Modbus Client, Modbus Server y runtime
  ya protegidos no toma el mutex FreeRTOS por segunda vez;
- `CLOSE_WAIT` llama `available()` dentro del lock
  de `connected()`;
- otra tarea que ocupa SPI no permite efectuar la
  transferencia W5500 durante BUS_BUSY;
- al salir del último scope, release único.

Cuatro mutaciones negativas de cuerpos C++ reales:
`NO_RAW_CLIENT_SCOPE`,
`NO_SOCKET_SCOPE`,
`MB_CLIENT_RAW_SPI_LOCK`,
`MB_SERVER_RAW_RELEASE`.
El último negativo detecta un acceso SPI **fuera del mutex**
después de liberar antes de tiempo, no necesariamente un
segundo release.

## 3. Evidencia CI

GitHub Actions definitiva:
https://github.com/JW-Control/platform-jwplc/actions/runs/38414490706

```text
G10_P1G_C2_S1_CI=PASS_WINDOWS_LINUX
G10_P1G_C2_S1_CI_HEAD=5c3da4b10c2f22d8ee9241b8f7503cc6b01d08f6
G10_P1G_C2_S1_REAL_PATCHED_METHODS=8
G10_P1G_C2_S1_OVERLAY_FILES=5
G10_P1G_C2_S1_NEGATIVES=4_PASS_REJECTED
G10_P1G_C2_S1_FROZEN_ARCHIVES=5
G10_P1G_C2_S1_LOCAL_WINDOWS=NOT_YET_RUN
G10_P1G_C2_S1_STATUS=REAL_SOURCE_MUTEX_SLICE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S1_PRODUCT_MODIFIED=NO
G10_P1G_C2_S1_ARDUINO_BUILDS=NOT_RUN
G10_P1G_C2_S1_PHYSICAL=NOT_RUN
```

Los primeros intentos CI fallaron por el arnés nativo
(`MAX_SOCK_NUM` no declarado) y luego por la expectativa
del mensaje de error en un negativo (el SPI sin mutex se
detectó antes del hipotético doble release). Se corrigió
solo el arnés; CI definitiva Windows/Linux pasó.

## 4. Bloqueos explícitos de adopción: C2 todavía no resuelto

1. `EthernetClient::connected()` retorna `0` si
   el bus está temporalmente ocupado. **Eso no prueba
   desconexión** y no es una semántica final aceptada
   para los usuarios TCP genéricos. Necesita decisión.
2. El mecanismo aún NO cubre `EthernetClient::read/write/stop`,
   `EthernetServer`, `EthernetUDP`, DHCP/DNS, todo
   `socket.cpp` ni caminos privados W5100.
3. El token `{epoch,allocation,owner}` de C1 **no está conectado**
   a los handles reales ni a `server_port[]`. No hay
   protección adoptable ante reutilización de índice.
4. La ruta `probeHardware()/resetHardwareIfNeeded()`
   sigue fuera de este slice. GPIO4 **no está habilitado**
   y su política de fairness SPI es pendiente.
5. Faltan compilación Arduino CLI baseline/candidato con
   **todos** los periféricos y pruebas de integración física.
6. La introducción eventual de la cabecera C1 en el package
   requiere comprobar includes reales, FreeRTOS/atomic,
   RAM y reglas de dependencia; extraer ocho cuerpos
   nativos no equivale a esa compilación.

**Conclusión:** `C2-S1` es **PASS de un slice de ocho
métodos reales**, con `ADOPTION_BLOCKED`. No renombrar
a `A13-006 FIXED` ni hacer commit de sus overlays al
árbol `JWPLC/2.1.0/libraries`.

## 5. Próximo bloque C2-S2

Desarrollar cobertura de operaciones TCP/UDP reales en
overlay, registrar y resolver ambigüedad `BUS_BUSY`
en API raw, y conectar la identidad de asignación/sockets
sin carreras TOCTOU. Agregar tests de cierre diferido tras
reset y reuso del mismo índice, retener la procedencia de
cuerpos reales. No introducir un mutex recursivo global
ni eliminar periféricos del autoload.

Antes de adopción de cualquier C2:
1. `C2` nativo completo PASS incluyendo negativos;
2. compilación Arduino baseline/candidato;
3. gate fairness/no deadlock con SD, FRAM y TFT;
4. gate físico previamente acordado.

## 6. Prueba local

Ejecutar desde la raíz del repositorio en PowerShell:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s1_real_spi_slice.bat
```

El BAT ya llama `python.exe` para evitar
`C:\Windows\System32\python` sin extensión, y el gate
utiliza el C++ host MSYS2 UCRT64 localizado durante C1.

Esperado:

```text
G10_P1G_C2_S1_POSITIVE_NATIVE=PASS_REAL_EIGHT_METHODS
G10_P1G_C2_S1_NEGATIVES=4
G10_P1G_C2_S1_METHODS_REAL_PATCHED=8
G10_P1G_C2_S1_OVERLAY_FILES=5
G10_P1G_C2_S1_FROZEN_ARCHIVES=5
G10_P1G_C2_S1_STATUS=REAL_SOURCE_MUTEX_SLICE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

La salida `INCOMPLETE_ADOPTION_BLOCKED` es **esperada**:
no permite flashear ni adoptar el overlay.
