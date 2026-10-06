# Alpha14.3 — Reaccept envejecido y diagnóstico de disponibilidad IP

Fecha: 2026-09-13

## Objetivo

Repetir el problema de reconexión TCP sin reflashear ni resetear el JWPLC, dejando `FULL_RUNTIME_REALISTIC` activo durante 8 minutos sin conexiones TCP y verificando después:

- disponibilidad Modbus TCP;
- presencia física de un socket W5500 `LISTEN` en puerto 502;
- respuesta ICMP/ping tras un fallo de conexión;
- salud de periféricos;
- coherencia entre estado lógico Ethernet/Modbus y estado físico del W5500.

## Resultado físico

El problema quedó reproducido de forma clara.

```text
AGING_S=480
ATTEMPTS=12
PASS_CYCLES=9
CONNECT_FAILURES=3
PROTOCOL_FAILURES=0
FAIL_WITH_HW_LISTEN=3
FAIL_WITHOUT_HW_LISTEN=0
FAIL_WITH_PING_PASS=1
FAIL_WITH_PING_FAIL=2
CONNECT_MIN_MS=0.383
CONNECT_AVG_MS=4.121
CONNECT_MAX_MS=19.362
FINAL_FULL_RUNTIME_READY=YES
FINAL_PERIPHERAL_FAILURE_COUNT=0

A14_3_AGED_REACCEPT=REPRODUCED_IP_PATH_WITH_HW_LISTENER
```

Los tres primeros intentos TCP expiraron alrededor de 2 s cada uno. En los tres casos existía físicamente un socket W5500 en:

```text
SR=LISTEN
PORT=502
SERVER_PORT=502
```

Por tanto, el problema no corresponde a ausencia del listener Modbus TCP.

Secuencia observada:

```text
ATTEMPT_1: TCP timeout + ping FAIL
ATTEMPT_2: TCP timeout + ping FAIL
ATTEMPT_3: TCP timeout + ping PASS
ATTEMPT_4: TCP PASS + FC03 PASS
```

Este patrón coincide con el comportamiento previamente observado durante la calificación de `FULL_RUNTIME_REALISTIC @ 1000 req/s`.

## Estado del runtime durante aging

Durante los 8 minutos:

```text
FULL_RUNTIME_READY=YES
SERVER_READY=YES
CLIENT_CONNECTED=NO
PERIPHERAL_FAILURE_COUNT=0
```

La TFT continuó actualizándose durante toda la ventana de aging y no se registraron fallos de periféricos.

Antes del primer intento posterior al aging:

```text
ETH_READY=YES
ETH_RUNTIME_STATE=5
ETH_LINK_STATUS=1
ETH_LOGICAL_IP=192.168.0.31
W5500_PHYCFGR=0xBF
W5500_SIPR=192.168.0.31
LISTEN_502_COUNT=1
ESTABLISHED_502_COUNT=0
```

## Clasificación

```text
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
A14_3_AGED_REACCEPT=REPRODUCED_IP_PATH_WITH_HW_LISTENER
LISTENER_GAP=NO
MODBUS_PROTOCOL_FAILURE=NO
PERIPHERAL_FAILURE=NO
ROOT_CAUSE_LAYER=ETHERNET_IP_DHCP_OR_L2
LONG_RUN_1000RPS=HOLD
```

El listener Modbus TCP puede permanecer correctamente armado mientras la ruta IP hacia el JWPLC no responde temporalmente.

## Hallazgo de código: temporización DHCP sospechosa

Durante la revisión posterior se identificó en `DhcpAsyncControl.cpp`:

```cpp
elapsed /= 1000;

if (_renewInSec < elapsed * 2)
{
    _renewInSec = 0;
}
else
{
    _renewInSec -= elapsed;
}

if (_rebindInSec < elapsed * 2)
{
    _rebindInSec = 0;
}
else
{
    _rebindInSec -= elapsed;
}
```

La condición de saturación usa `elapsed * 2`, mientras la resta efectiva usa sólo `elapsed`. Este comportamiento es sospechoso y no fue cubierto por el gate Alpha6 de T1/T2, porque aquel gate forzaba directamente los timers a cero para iniciar renew/rebind y validaba la secuencia funcional, no la tasa real de countdown.

No se modifica todavía la implementación: primero se medirá físicamente el countdown usando los hooks de prueba existentes.

## Nota sobre una captura RAW incoherente

En `ATTEMPT_10_BEFORE` apareció una única captura RAW con valores imposibles/incoherentes (`SIPR=248.21.0.3`, `PHYCFGR=0xF7`, `TXFREE=256`, etc.) y, aun así, la conexión TCP/FC03 inmediatamente posterior funcionó.

Esta lectura no se considera evidencia de corrupción real del W5500. El helper RAW temporal adquiría el mutex SPI y deseleccionaba CS, pero no envolvía las lecturas directas `W5100.read*()` con `SPI.beginTransaction(SPI_ETHERNET_SETTINGS)` / `SPI.endTransaction()`. El backend W5100 de bajo nivel espera que la transacción SPI adecuada sea configurada externamente. Los siguientes diagnósticos RAW deberán corregir ese helper.

## Precompilación

`JWPLC_Ethernet/library.properties` no declara `precompiled=full`, por lo que esta librería se compila desde fuente en el package actual. Para instrumentar `JWPLC_Ethernet` no es necesario anular precompilación. La advertencia de precompilados sigue siendo crítica para librerías que sí los usan, como `JW_SD`, `JW_FRAM`, `JWPLC_Display`, etc.

## Siguiente gate

Medir con los hooks de Alpha6 el countdown real de `renewInSec` y `rebindInSec`, sin cambiar comportamiento productivo. El objetivo es responder:

1. cuánto decrecen los timers por segundo real;
2. cuándo entra realmente en T1/T2;
3. si la ventana de pérdida IP coincide temporalmente con renew/rebind;
4. si corregir la temporización DHCP es necesario antes del long run de 1000 req/s.
