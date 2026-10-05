# Alpha14 — Client timeout, cierre graceful y reconexión

Fecha: `2026-09-12`

## Objetivo

Validar físicamente que `JWPLC_ModbusTCPClient`:

1. detecta el timeout de una petición sin respuesta;
2. invalida la sesión TCP anterior;
3. notifica correctamente el cierre al peer;
4. puede abrir una nueva conexión sin repetir `begin()`;
5. conserva la secuencia de Transaction ID;
6. completa correctamente una petición posterior a la reconexión.

## Configuración

```text
JWPLC_IP=192.168.0.31
PC_IP=192.168.0.4
UNIT_ID=1
FUNCTION=FC03
FIRST_REQUEST_TIMEOUT_MS=600
RECONNECT_REQUEST_TIMEOUT_MS=3000
TEST_PORT_FINAL=15028
```

## Primera ejecución

El Client detectó correctamente el timeout y abrió una segunda conexión sin repetir `begin()`:

```text
FIRST_TID=1
FIRST_TIMEOUT_OBSERVED=YES
SESSION_CLOSED_AFTER_TIMEOUT=YES
SECOND_TID=2
SECOND_VALUE=9320
SESSION_CONNECTED=YES
CONNECTIONS=2
TX_FRAMES=2
RX_FRAMES=1
REQUESTS_OK=1
EXCEPTIONS=0
TIMEOUTS=1
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
A14_2_CLIENT_TIMEOUT_RECONNECT_RUNTIME=PASS
```

El PC confirmó una segunda conexión TCP real y la secuencia TID `1 -> 2`, pero inicialmente no observó EOF/RST en la primera conexión:

```text
CLIENT_CLOSED_CONNECTION_AFTER_TIMEOUT=FAIL
TCP_CONNECTIONS=2
```

## Hallazgo de backend

La revisión de `JWPLC_Ethernet` confirmó que `socketClose()` cierra inmediatamente el socket del W5x00 y puede dejar al peer sin conocer el cierre.

Para una sesión TCP ya establecida, el comportamiento correcto para este caso es iniciar un cierre graceful mediante `socketDisconnect()`.

También se detectó que `beginConnectAsync()` había añadido un rechazo temprano de `port == 0` que no existía en la semántica histórica de `EthernetClient::connect()`.

## Corrección aplicada

Se consolidó el siguiente criterio:

```text
ESTABLISHED / CLOSE_WAIT -> socketDisconnect()
otros estados activos    -> socketClose()
CLOSED                   -> no acción
```

Además se eliminó el rechazo añadido de `port == 0` en el backend async para conservar la semántica histórica de `EthernetClient`.

Commit de implementación:

```text
34e4762adfacecccf44f125daf8ae7dabc836ba5
fix(ethernet): cerrar sesiones async de forma graceful
```

## Regresión de compilación

Después del ajuste:

```text
SERVER_REGRESSION_EXIT=0
CLIENT_REGRESSION_EXIT=0
A14_2_GRACEFUL_CLOSE_PATCH_COMPILE=PASS
PORT0_HISTORIC_SEMANTICS_RESTORED=YES
A14_2_ETHERNET_BACKEND_COMPAT_REVIEW=PASS
```

## Retest físico final — PC

```text
TCP_ACCEPT_1=PASS
REMOTE_1=192.168.0.31:56856
REQ1_RX=00 01 00 00 00 06 01 03 00 00 00 01
REQ1_FC03_TID1=PASS

INTENTIONAL_NO_RESPONSE=YES
CLIENT_CLOSED_CONNECTION_AFTER_TIMEOUT=PASS

TCP_ACCEPT_2=PASS
REMOTE_2=192.168.0.31:56857
REQ2_RX=00 02 00 00 00 06 01 03 00 00 00 01
REQ2_FC03_TID2_AFTER_RECONNECT=PASS
RESP2_TX=00 02 00 00 00 05 01 03 02 24 68

TCP_CONNECTIONS=2
A14_2_GRACEFUL_CLOSE_RETEST_PC=PASS
```

Los puertos origen distintos (`56856` y `56857`) confirman que existió una nueva conexión TCP real.

## Retest físico final — JWPLC

```text
FIRST_TID=1
FIRST_TIMEOUT_OBSERVED=YES
SESSION_CLOSED_AFTER_TIMEOUT=YES
SECOND_TID=2
SECOND_VALUE=9320
SESSION_CONNECTED=YES
CONNECTIONS=2
TX_FRAMES=2
RX_FRAMES=1
REQUESTS_OK=1
EXCEPTIONS=0
TIMEOUTS=1
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
A14_2_GRACEFUL_CLOSE_RETEST_RUNTIME=PASS
```

`9320` decimal corresponde a `0x2468`.

## Resultado

```text
A14_2_CLIENT_TIMEOUT_DETECTION=PASS
A14_2_CLIENT_REMOTE_CLOSE_NOTIFICATION=PASS
A14_2_CLIENT_RECONNECT=PASS
A14_2_CLIENT_REQUEST_AFTER_RECONNECT=PASS
A14_2_CLIENT_TIMEOUT_RECONNECT=PASS_PHYSICAL
A14_2_GRACEFUL_CLOSE_RUNTIME=PASS_PHYSICAL
A14_2_ETHERNET_BACKEND_COMPAT_REVIEW=PASS
```

## Conclusión

El Client Modbus TCP recupera correctamente una petición expirada sin requerir una nueva llamada a `begin()`.

La sesión anterior se cierra de forma visible para el peer mediante `socketDisconnect()` cuando ya existe una conexión TCP establecida, mientras que las conexiones incompletas continúan abortándose de forma inmediata mediante `socketClose()`.

El backend Ethernet queda validado para continuar con el cierre de A14.2.
