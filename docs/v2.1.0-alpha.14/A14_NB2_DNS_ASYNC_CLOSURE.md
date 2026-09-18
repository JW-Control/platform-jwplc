# Alpha14 — NB2: DNS cooperativo

## Objetivo

Eliminar la espera activa prolongada de la resolución DNS del runtime Ethernet manteniendo compatibilidad con la API Arduino legacy.

NB2 no intenta todavía eliminar el bloqueo de `socketSendUDP()`; esa dependencia pertenece a NB3.

## Hallazgo inicial — NB2-A

La implementación original presentaba:

```text
DNS_DEFAULT_TIMEOUT_MS=5000
DNS_WAIT_ATTEMPTS_MAX=3
DNS_NOMINAL_WORST_WAIT_MS=15000
DNS_POLL_SLEEP_MS=50
CLIENT_CONNECTION_TIMEOUT_CONTROLS_DNS=NO
```

Por tanto, una resolución por hostname podía bloquear nominalmente hasta ~15 s, y `EthernetClient::_timeout` no gobernaba esa fase.

NB2-A cerró:

```text
NB2_DNS_BLOCKING_WAIT=CONFIRMED_BY_SOURCE
NB2_CLIENT_TIMEOUT_NOT_FORWARDED_TO_DNS=CONFIRMED
A14_NB2_DNS_BLOCKING_SOURCE_AUDIT=PASS
```

## Motor cooperativo — NB2-B

Se añadió a `DNSClient`:

```cpp
beginResolveAsync(...)
pollResolveAsync()
resolveAsyncInProgress()
cancelResolveAsync()
```

El método legacy:

```cpp
getHostByName(...)
```

se conserva y actúa como wrapper bloqueante sobre el mismo motor.

La ruta `pollResolveAsync()` no contiene `delay()`.

Se conservaron las tres ventanas legacy de espera y el timeout por defecto de 5000 ms para no cambiar silenciosamente el contrato de compatibilidad.

NB2-B compiló:

- raw candidate completo;
- API probe dedicado;
- librería Ethernet del repositorio confirmada;
- 4 binarios en cada compilación;
- sin upload.

Marcadores:

```text
NB2_DNS_ASYNC_ENGINE=ADDED
NB2_DNS_ASYNC_POLL_DELAY=ABSENT
NB2_DNS_LEGACY_GETHOST=WRAPPED_OVER_ASYNC_ENGINE
NB2_DNS_WAIT_WINDOWS=3_PRESERVED
NB2_DNS_RESPONSE_TIMEOUT_MS=5000_DEFAULT_PRESERVED
NB2_DNS_UDP_SEND_SYNC_DEPENDENCY=PENDING_NB3
A14_NB2_DNS_ASYNC_APPLY_COMPILE=PASS
```

## Validación física — NB2-C

Se usó un servidor DNS local controlado en la PC.

Caso válido:

```text
jwplc.test -> 10.20.30.40
```

Caso silencioso:

```text
timeout.jwplc.test -> sin respuesta
timeout por ventana = 150 ms
ventanas = 3
```

Resultado formal:

```text
RESULT_CODE=1
PROBE_FAILED=NO

SUCCESS_RESULT_IP=10.20.30.40
SUCCESS_DURATION_MS=0
SUCCESS_POLL_COUNT=2
SUCCESS_POLL_PENDING_COUNT=1

TIMEOUT_DURATION_MS=453
TIMEOUT_POLL_COUNT=7141
TIMEOUT_POLL_PENDING_COUNT=7140

DNS_BEGIN_HOLD_MAX_US=6033
DNS_POLL_HOLD_MAX_US=369
LOOP_GAP_MAX_US=2383
SPI_LOCK_ERRORS=0

DNS_VALID_QUERY_COUNT=1
DNS_TIMEOUT_QUERY_COUNT=1
DNS_OTHER_QUERY_COUNT=0
VISUAL_SPI_EVENTS=0
```

Conclusiones:

- la resolución válida se completó correctamente;
- el timeout silencioso permaneció cooperativo durante ~453 ms;
- cada poll individual quedó muy por debajo de 5 ms;
- el loop continuó ejecutándose durante toda la espera;
- no hubo diagnóstico visual SPI.

## Frontera NB2 / NB3

`DNS_BEGIN_HOLD_MAX_US=6033` no se atribuye al wait cooperativo DNS.

`beginResolveAsync()` debe enviar primero el datagrama DNS. Actualmente:

```text
EthernetUDP::endPacket()
 -> EthernetClass::socketSendUDP()
 -> espera síncrona de SEND_OK/TIMEOUT
```

Ese backend UDP síncrono queda explícitamente pendiente para NB3.

Por tanto:

```text
DNS_RESPONSE_WAIT=COOPERATIVE_PASS
UDP_SEND_BACKEND=PENDING_NB3
```

## Compatibilidad y uso

Las APIs legacy por hostname se mantienen bloqueantes deliberadamente como wrappers de conveniencia.

El runtime de producción no debe depender de esas rutas cuando necesite mantener el scan cooperativo. El camino recomendado para código nuevo es el motor async.

No se cambia aquí la API pública de Modbus TCP/RTU.

## Cierre NB2-D

NB2-D verificó el mismo candidato probado físicamente:

```text
DNS_ASYNC_POLL_DELAY_COUNT=0
DNS_ASYNC_POLL_WHILE_COUNT=0
LEGACY_GETHOST_BEGIN_ASYNC_COUNT=1
LEGACY_GETHOST_POLL_ASYNC_COUNT=1

CLIENT_LEGACY_HOSTNAME_DNS_COUNT=1
UDP_LEGACY_HOSTNAME_DNS_COUNT=1

TRACKED_DIRTY_COUNT_FINAL=6
STAGED_COUNT_FINAL=0
```

Marcadores finales:

```text
NB2_DNS_ASYNC_SOURCE_CONTRACT=PASS
NB2_DNS_ASYNC_POLL_BLOCKING_DELAY=ABSENT
NB2_DNS_LEGACY_GETHOST=WRAPPER_PRESERVED
NB2_LEGACY_CLIENT_HOSTNAME_PATH=PRESERVED
NB2_LEGACY_UDP_HOSTNAME_PATH=PRESERVED
NB2_DNS_PHYSICAL_VALID_RESOLUTION=PASS
NB2_DNS_PHYSICAL_TIMEOUT_PENDING=PASS
NB2_DNS_POLL_HOLD_MAX_US=369_EVIDENCE
NB2_DNS_LOOP_GAP_MAX_US=2383_EVIDENCE
NB2_DNS_BEGIN_HOLD_US=6033_DEFERRED_TO_NB3_UDP_SEND
NB2_OVERALL=CLOSED_PASS
A14_NB2_DNS_CLOSURE=PASS
```

## Estado final

```text
NB2_SOURCE_AUDIT=PASS
NB2_ASYNC_ENGINE=PASS
NB2_PHYSICAL_VALID_RESOLUTION=PASS
NB2_PHYSICAL_TIMEOUT=PASS
NB2_POLL_COOPERATIVE=PASS
NB2_UDP_SEND_DEPENDENCY=DEFERRED_TO_NB3
NB2_OVERALL=CLOSED_PASS
```
