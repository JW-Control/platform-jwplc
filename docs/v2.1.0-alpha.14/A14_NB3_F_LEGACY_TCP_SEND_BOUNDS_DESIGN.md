# Alpha14 — NB3-F: bounds del TCP send legacy

## Objetivo

Eliminar las dos esperas potencialmente indefinidas restantes dentro de `EthernetClass::socketSend()` sin cambiar la API pública Arduino.

## Diseño

- `socketSend()` sigue siendo bloqueante por compatibilidad.
- `EthernetClient::write()` pasa su `_timeout` existente.
- `EthernetServer::write()` conserva su llamada actual mediante un default interno de 1000 ms.
- El mismo presupuesto total limita espera de TX libre y espera de `SEND_OK`.
- `Sock_SEND` usa `execCmdSnChecked(..., 1000 us)`.
- `SnIR::TIMEOUT` se observa explícitamente.

## Compatibilidad

- No cambia la firma pública de `EthernetClient::write()`.
- No cambia la firma pública de `EthernetServer::write()`.
- No cambia UDP.
- No cambia la frecuencia SPI.
- No se hace tuning de throughput.

NB3-F1 será apply + compile sin upload. NB3-F2 hará validación física normal y de backpressure.