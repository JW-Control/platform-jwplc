# A14.2 — Decisión: TX TCP cooperativo

Fecha: `2026-09-11`

## Hallazgo

El gate físico de `beginConnectAsync()/pollConnectAsync()` quedó en PASS, pero la ruta legado `EthernetClient::write()` no es plenamente cooperativa: `socketSend()` espera espacio TX y luego espera `SEND_OK` antes de retornar.

Por lo tanto, no se construirá el Client/Master Modbus TCP sobre `write()` declarando que es no bloqueante.

## Decisión

Se añade un helper interno de JWPLC Ethernet:

```text
jwplc_ethernet_async_tx.h
jwplc_ethernet_async_tx.cpp
```

Contrato:

```text
begin(client, data, length)
  -1 = error
   0 = pending / SEND iniciado
   1 = length=0, terminado

poll(client)
  -1 = error / TIMEOUT / socket cerrado
   0 = pending
   1 = SEND_OK

inProgress()
reset()
```

El helper:

- no cambia `EthernetClient::write()`;
- no cambia `socket.cpp`;
- usa el socket ya asignado a `EthernetClient`;
- verifica `ESTABLISHED/CLOSE_WAIT`;
- espera espacio TX antes de cargar el frame;
- copia al buffer TX W5x00;
- limpia flags previos `SEND_OK/TIMEOUT`;
- dispara `Sock_SEND` y retorna;
- deja la finalización a `poll()`;
- detecta `SEND_OK`, `TIMEOUT` o cierre de socket.

Cada llamada será protegida externamente por el mutex SPI compartido JWPLC cuando la use `JWPLC_ModbusTCP`.

## Gate siguiente

```text
A14_2_ASYNC_TX_SERVER_REGRESSION_COMPILE=NOT_EXECUTED
A14_2_ASYNC_TX_API_COMPILE=NOT_EXECUTED
A14_2_ASYNC_TX_RUNTIME=NOT_EXECUTED
```

No se implementará todavía la state machine Modbus TCP Client hasta que este helper compile y pase un gate físico de TX.
