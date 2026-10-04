# Alpha14 — NB3-D: UDP SEND cooperative engine

## Objetivo

Eliminar la espera síncrona de `SEND_OK/TIMEOUT` del backend UDP usado por las rutas cooperativas, manteniendo compatibilidad con la API Arduino legacy.

NB3-C ya validó físicamente las primitives acotadas del W5500 y los cuatro modos raw. NB3-D no modifica frecuencia SPI, tamaño de buffers, estrategia UDP RX ni `parsePacket()`.

## Problema

La ruta actual es:

```text
EthernetUDP::endPacket()
 -> EthernetClass::socketSendUDP()
 -> Sock_SEND
 -> while espera SEND_OK/TIMEOUT
```

En NB2 se midió:

```text
DNS_BEGIN_HOLD_MAX_US=6033
```

La espera DNS posterior ya es cooperativa; el hold restante se atribuyó al SEND UDP síncrono.

## Diseño

### Capa socket

Se añaden:

```cpp
static int socketBeginSendUDP(uint8_t s);
static int socketPollSendUDP(uint8_t s);
```

Contrato:

```text
-1 = error/timeout
 0 = pending
 1 = SEND_OK
```

`socketBeginSendUDP()` limpia flags anteriores, ejecuta `Sock_SEND` mediante `execCmdSnChecked()` y retorna sin esperar confirmación de red.

`socketPollSendUDP()` realiza una sola consulta de flags por llamada.

`socketSendUDP()` se conserva y pasa a ser un wrapper legacy bloqueante sobre begin/poll.

### EthernetUDP

Se añaden:

```cpp
int beginEndPacketAsync();
int pollEndPacketAsync();
bool endPacketAsyncInProgress() const;
void cancelEndPacketAsync();
```

`endPacket()` conserva el contrato Arduino y sigue siendo bloqueante deliberadamente.

### DNS

`DNSClient::beginResolveAsync()` deja de llamar al `endPacket()` legacy. Inicia el SEND UDP asíncrono y retorna pending.

`DNSClient::pollResolveAsync()` completa primero el SEND y sólo después inicia la ventana de espera de respuesta DNS.

## Compatibilidad

- `EthernetUDP::endPacket()`: preservado.
- `DNSClient::getHostByName()`: preservado como wrapper bloqueante.
- APIs cooperativas: aditivas.
- No se cambia Modbus.
- No se cambia `parsePacket()` en NB3-D.
- No se cambia la frecuencia SPI.

## Gates

### NB3-D1

Apply + compile, sin upload:

- valida working tree/hashes;
- aplica motor cooperativo;
- compila raw candidate;
- compila API probe;
- verifica que el source UDP ya no contenga espera directa de `SEND_OK`;
- verifica que DNS async use `beginEndPacketAsync()/pollEndPacketAsync()`.

### NB3-D2

Validación física posterior:

- repetir DNS válido y timeout controlado;
- comparar `DNS_BEGIN_HOLD_MAX_US` contra los 6033 us anteriores;
- verificar `DNS_POLL_HOLD_MAX_US`;
- confirmar cero errores SPI;
- comprobar UDP TX raw para no introducir regresión.

## Criterio

NB3-D sólo podrá cerrarse después de evidencia física. El compile PASS de NB3-D1 no implica todavía cierre.
