# Alpha13 — G10-P1G-C2-S2C1: cancelación TX bajo SPI y token

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre anterior confirmado por operador

El operador ejecutó `S2B2B` en Windows local con checkout limpio:

```text
G10_P1G_C2_S2B2B_POSITIVE_NATIVE=PASS_REAL_OUTBOUND_LIFECYCLE
G10_P1G_C2_S2B2B_NEGATIVE_REMOVE_BEGIN_GUARD=PASS_REJECTED
G10_P1G_C2_S2B2B_NEGATIVE_REMOVE_NEW_TOKEN=PASS_REJECTED
G10_P1G_C2_S2B2B_NEGATIVE_NO_DEFER_ON_BUSY=PASS_REJECTED
G10_P1G_C2_S2B2B_NEGATIVE_REMOVE_CANCEL_OWNER=PASS_REJECTED
G10_P1G_C2_S2B2B_NEGATIVE_POLL_BUSY_FALSE_DISCONNECT=PASS_REJECTED
G10_P1G_C2_S2B2B_HEAD=755a9b91b379685bd5404b2a448d9d59e32c888a
G10_P1G_C2_S2B2B_REAL_CPP_METHODS=4
G10_P1G_C2_S2B2B_OVERLAY_FILES=3
G10_P1G_C2_S2B2B_NEGATIVES=5
G10_P1G_C2_S2B2B_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B2B_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B2B_STATUS=OUTBOUND_CONNECT_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2B2B_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2b2b_20261013_124846_9fdc28d7
```

**S2B2B Windows local = CLOSED_PASS**, al igual que su CI
Windows/Linux (run `38440505069`). El alcance continúa
limitado a métodos reales en arnés host, **no** firmware.

## 2. Hallazgo de código productivo original

En `JWPLC_Ethernet/src/EthernetClient.cpp`:

```cpp
void EthernetClient::cancelWriteAsync()
{
    const bool sendWasIssued = _writeAsyncState == 2;
    // ... limpia estado software ...
    if (sendWasIssued && _sockindex < MAX_SOCK_NUM) {
        Ethernet.socketClose(_sockindex);
        _sockindex = MAX_SOCK_NUM;
    }
}
```

La función no valida generación/asignación/propietario
ni obtiene mutex W5500. Si una copia de `EthernetClient`
queda obsoleta, el índice podría pertenecer ya a otro
cliente. Además, `beginConnectAsync()` llama
`cancelWriteAsync()` antes de comenzar una conexión nueva.
Se considera **riesgo de código**; no se afirma que haya
ocurrido en campo ni que explique latencias Alpha12.

## 3. C2-S2C1 — gate aislado creado

Nuevos archivos:
- `tools/alpha13/gates/a13_g10_p1g_c2_s2c1_tx_cancel.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2c1_tx_cancel.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2c1_tx_cancel_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2c1-tx-cancel.yml`

El gate regenera el overlay desde las fuentes reales
de `EthernetClient` y el header/Server como S2B1,
añade los tres métodos de cierre S2B2A y cuatro métodos
salientes S2B2B. Extrae y **compila cinco cuerpos reales**
para este escenario: los cuatro métodos `begin/poll/
inProgress/cancelConnectAsync()` y el nuevo
`cancelWriteAsync()` modificado de forma temporal.

Comportamiento bajo prueba:
1. `cancelWriteAsync()` adquiere `BusScope` antes de
   actuar sobre socket asignado; la entrada W5500 es
   anidable con la llamada superior `beginConnectAsync()`.
2. Si `SPI_BUSY`, **no borra** `_writeAsyncState`, índice
   ni lease; mantiene el estado para un reintento posterior.
3. Si el lease pertenece a otra asignación, limpia
   únicamente el estado local y **no emite CLOSE**.
4. Con `SEND` ya emitido (estado 2), cierre del socket
   todavía propio e invalidación de su token suceden
   bajo la misma exclusión.
5. Si `SEND` no llegó a emitirse (estado 1), descarta
   el estado software sin destruir un socket vigente.
6. Retarget con envío pendiente puede cerrar el socket
   anterior y abrir otro, incluso **reutilizando el mismo
   índice**; debe obtener una nueva identidad de asignación.
   No se exige índice distinto, sino token antiguo inválido.

Pruebas nativas combinadas de cinco métodos incluyen
conexión saliente, contención en start/poll, retarget,
cancelación, reset, cancelación TX, copias obsoletas,
retry y estado sin SEND. `EthernetClass::socketBegin`
es mock en esta batería (fue compilado y probado
por separado en S2A).

### Mutaciones negativas

Los cinco negativos compilan y se ejecutan. Deben fallar
si se elimina:
- `NO_TX_GUARD`: exclusión W5500;
- `NO_TX_OWNER`: validación de propietario;
- `BUSY_DISCARDS_TX`: conservación de TX bajo BUS_BUSY;
- `NO_TX_LEASE_INVALIDATION`: invalidación tras CLOSE;
- `SKIP_TX_HARDWARE_CLOSE`: CLOSE cuando SEND ya ocurrió.

## 4. Evidencia CI

[CI Windows/Linux, run `38442717818`](https://github.com/JW-Control/platform-jwplc/actions/runs/38442717818).

```text
G10_P1G_C2_S2C1_CI=PASS_WINDOWS_LINUX_RUN_38442717818
G10_P1G_C2_S2C1_CI_HEAD=7d21d2b3ff7013ba86db6846619988da0ec75c80
G10_P1G_C2_S2C1_POSITIVE_NATIVE=PASS_REAL_TX_CANCEL_WITH_OUTBOUND
G10_P1G_C2_S2C1_REAL_CPP_METHODS=5
G10_P1G_C2_S2C1_OVERLAY_FILES=3
G10_P1G_C2_S2C1_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2C1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C1_STATUS=TX_CANCEL_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

Intentos previos:
- El primer fallo positivo no evidenciaba un bug
  firmware: el arnés asumía erróneamente un índice nuevo
  al volver a conectar. El W5500 puede reutilizar
  legítimamente el índice después de `CLOSE`.
- Se corrigió la prueba para exigir nuevo **lease**.
- En un negativo sin guard, `matchesLocked()` falla
  antes del `CLOSE`, por lo que el error real esperado
  es que no se emita el CLOSE solicitado. Se corrigió
  únicamente la expectativa del negativo.
- Se comprobó finalmente el PASS de ambas plataformas
  y los cinco negativos. No se cambió el producto.

## 5. Bloqueos todavía obligatorios

```text
S2C1=CI_PASS_NATIVE_SOURCE_SLICE
S2C1_WINDOWS_LOCAL=AWAITING_OPERATOR
S2C2=LEGACY_CONNECT_DNS_NOT_DONE
WRITE_POLL_AND_RECV=NOT_INTEGRATED
SERVER_UDP_DHCP_DNS=NOT_FULLY_GUARDED
SPI_BUSY_AUTOMATIC_RETRY=NOT_PROVEN_END_TO_END
ARDUINO_CLI_COMPILE=NOT_RUN
DISPLAY_SD_FRAM_RTC_HARDWARE=NOT_TESTED
GPIO4_RESET_DEFAULT=255
PRECOMPILED_ARCHIVES=5_FROZEN
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
```

El gate todavía **no** prueba `pollWriteAsync()`,
`write()`, `read()` ni el contrato bloqueante
`connect(IPAddress)`/`connect(host)` con DNS.
En particular, `SPI_BUSY` al cancelar puede requerir
un reintento futuro: no declarar cierre garantizado.
Los cambios temporales tampoco constituyen un
package Arduino completo ni un runtime real bajo FreeRTOS.

## 6. Prueba local en PowerShell

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2c1_tx_cancel.bat
```

Se exige manifest local PASS de S2B2B con run
`g10_p1g_c2_s2b2b_20261013_124846_9fdc28d7`.

Salida esperada:

```text
G10_P1G_C2_S2C1_POSITIVE_NATIVE=PASS_REAL_TX_CANCEL_WITH_OUTBOUND
G10_P1G_C2_S2C1_REAL_CPP_METHODS=5
G10_P1G_C2_S2C1_OVERLAY_FILES=3
G10_P1G_C2_S2C1_NEGATIVES=5
G10_P1G_C2_S2C1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C1_STATUS=TX_CANCEL_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
```

**Próximo paso:** S2C2 — seguridad de los métodos
Arduino `connect(IPAddress)` y `connect(const char*)`
y resolución DNS bajo mutex/lease, sin romper timeout,
manejo de error ni compatibilidad Arduino IDE.

No habilitar GPIO4 ni cargar firmware antes de un
overlay Arduino compilable y gates físicos adicionales.
