# JW_DWIN

Librería para pantallas **DWIN DGUS** con objetos de teclas, valores y textos, independiente del medio físico.

Conserva la API de la clase `JW_DWIN` de `JW_DWIN_RS485` 1.x: los mismos structs `DWIN_Key`, `DWIN_Value` y `DWIN_String`, y los mismos métodos (`setPage`, `enableTouch`, `sendValue`, `readValue`, `sendText`, `checkKey`, `keyEncoder`, `withDebug`). Un sketch hecho con esa versión solo cambia la declaración del objeto `dwin`.

## Transportes

| Transporte | Medio | Estado |
|---|---|---|
| `JW_DWIN_ModbusRTU` | RS-485, firmware DGUS Modbus RTU | v0.1 |
| `JW_DWIN_ModbusUSB` | USB de la tarjeta, Modbus RTU en sobres (modo Simular de JW DWIN) | v0.2 |
| UART con protocolo DGUS nativo (`5A A5`) | UART TTL / RS-232 | Previsto |

## Compatibilidad

- ESP32 con Arduino core 3.x.

## Dependencias

- `JW_ModbusRTU` y `JW_RS485` (solo si se usa `JW_DWIN_ModbusRTU`). `JW_DWIN_ModbusUSB` no tiene dependencias.

## Conexión básica (RS-485)

Pantalla DWIN con proyecto DGUS compilado para **Modbus RTU** (esclavo), conectada al bus RS-485 del ESP32. Configurar el ID y la velocidad iguales en la pantalla y en `JW_DWIN_ModbusRTU`.

## Ejemplo mínimo

```cpp
#include <JW_DWIN.h>
#include <JW_DWIN_ModbusRTU.h>

JW_RS485 rs485(Serial2, 16, 17);
JW_ModbusRTU modbus(rs485);
JW_DWIN_ModbusRTU transport(modbus, 1, 115200, SERIAL_8N1);  // esclavo 1
JW_DWIN dwin(transport);

DWIN_Value prog = {0x10, 0x00, 0, 0, 0};  // VP 0x1000

void setup()
{
    rs485.setAutoDirection();
    dwin.begin();
    dwin.setPage(0x10);
}

void loop()
{
    dwin.readValue(prog);
    dwin.sendValue(prog, prog.val);
}
```

## API principal

| Método | Descripción |
|---|---|
| `begin()` | Inicia el transporte. |
| `setPage(page)` | Cambia de página (`0x5A01` en `0x0084`). |
| `enableTouch(obj, state, force)` | Habilita o bloquea un control táctil (`0x5AA5` en `0x00B0`). |
| `sendValue(obj, value, force, debug)` | Escribe un VP numérico si cambió o no estaba confirmado. |
| `readValue(obj, debug)` | Lee un VP; devuelve `true` si cambió. La lectura que restablece el enlace nunca cuenta como cambio. |
| `readValues(objs, count, changed)` | `readValue()` de un arreglo de punteros a valores, con una lectura 03 por tramo de VPs cercanos; `changed[i]` (opcional) indica cuáles cambiaron. `false` si alguna lectura falló. |
| `isConfirmed(obj)` | `true` si la pantalla confirmó el valor o texto del objeto desde la última reconexión. |
| `sendText(obj, text, force)` | Escribe un texto de largo fijo (relleno con espacios). |
| `checkKey(key)` | Si la tecla fue pulsada, `key.Enable = 1` y limpia el VP. |
| `checkKeys(keys, count)` | Lo mismo para un arreglo de punteros a teclas, con una lectura 03 por tramo de VPs cercanos (en vez de una por tecla); solo escribe 0 en las pulsadas. `false` si alguna lectura falló. |
| `keyEncoder(minus, plus, count, min, max)` | Dos teclas como encoder, con vuelta circular. |
| `readRegister(vp, value)`, `readRegisters(vp, count, values)`, `writeRegister(vp, value)` | Acceso directo a VPs, sin cache. |
| `isOnline()` | `true` mientras no se alcancen los fallos seguidos de la política sin enlace. |
| `reconnected()` | `true` una sola vez tras volver a comunicar. |
| `failureCount()` | Fallos acumulados desde el arranque. |
| `lastSuccessMs()` | `millis()` de la última operación confirmada. |
| `currentPage()` | Última página pedida con `setPage()`. |
| `setOfflinePolicy(fallos, ms)` | Fallos seguidos para pasar a sin enlace e intervalo de reintento. |
| `setTouchDelay(ms)` | Espera tras cambiar un estado táctil (25 ms por defecto). |
| `invalidateCache()` | Fuerza el reenvío de todo. |
| `lastErrorString()` | Causa del último error del transporte. |
| `setDebug(Stream&, mask)`, `setDebug(bool)` | Debug opcional, apagado por defecto. `setDebug(true)` usa `Serial` con `DWIN_DBG_ALL`. |
| `setDebugMask(mask)`, `debugMask()` | Cambia o consulta la máscara de debug. |
| `withDebug(out, mask, fn)` | Ejecuta `fn` con una salida y máscara de debug temporales. |

### `JW_DWIN_ModbusRTU`

| Método | Descripción |
|---|---|
| `JW_DWIN_ModbusRTU(modbus, slaveId, baud, config)` | Esclavo 1, 115200 y `SERIAL_8N1` por defecto. |
| `setTimeout(ms)`, `timeout()` | Timeout por operación (150 ms por defecto). |
| `slaveId()` | ID Modbus de la pantalla. |

### `JW_DWIN_ModbusUSB`

Ver [Simular por USB](#simular-por-usb).

| Método | Descripción |
|---|---|
| `JW_DWIN_ModbusUSB(port, slaveId)` | Puerto ya iniciado (normalmente `Serial`); esclavo 1 por defecto. |
| `setTimeout(ms)`, `timeout()` | Timeout por operación (150 ms por defecto). |
| `slaveId()` | ID Modbus de la pantalla. |
| `text()` | `Stream` con el texto recibido fuera de los sobres. Lo que se escribe en él sale tal cual por el puerto. |
| `poll()` | Procesa lo recibido sin esperar. `text().available()` ya lo llama. |
| `lastError()`, `lastException()` | Último error (`JW_DWIN_USBError`) y código de la última excepción Modbus. |
| `badEnvelopes()` | Sobres descartados por largo o CRC inválido. |
| `strayFrames()` | Tramas válidas que no respondían a un pedido pendiente (tardías o de otro id). |
| `textOverflows()` | Bytes de texto perdidos porque el buffer estaba lleno. |

### Opciones de compilación

Se usan dentro de `JW_DWIN.cpp`, así que deben llegar como flags de compilación (`build_opt.h` en el core ESP32, `build_flags` en PlatformIO). Un `#define` en el sketch no alcanza a la librería.

| Macro | Defecto | Uso |
|---|---|---|
| `JW_DWIN_OFFLINE_FAILURES` | 3 | Fallos seguidos para pasar a sin enlace. En ejecución: `setOfflinePolicy()`. |
| `JW_DWIN_OFFLINE_RETRY_MS` | 500 | Intervalo de reintento sin enlace (ms). En ejecución: `setOfflinePolicy()`. |
| `JW_DWIN_MAX_TEXT` | 128 | Mayor texto admitido por `sendText()`. |
| `JW_DWIN_MAX_READ` | 125 | Registros por lectura en `checkKeys()` y `readValues()` (límite de Modbus: 125). |
| `JW_DWIN_READ_MAX_GAP` | 16 | Mayor hueco entre dos VPs que `checkKeys()` y `readValues()` cubren con la misma lectura. |
| `JW_DWIN_MODBUS_DEFAULT_TIMEOUT_MS` | 150 | Timeout inicial de `JW_DWIN_ModbusRTU`. En ejecución: `setTimeout()`. |
| `JW_DWIN_USB_DEFAULT_TIMEOUT_MS` | 150 | Timeout inicial de `JW_DWIN_ModbusUSB`. En ejecución: `setTimeout()`. |
| `JW_DWIN_USB_TEXT_BUFFER` | 512 | Bytes de texto recibido que se guardan hasta que el firmware los lee. |
| `JW_DWIN_USB_STALE_MS` | 100 | Un sobre a medias sin bytes nuevos en este tiempo se abandona. |

### Transporte propio

Para otro medio físico, heredar de `JW_DWIN_Transport` e implementar `begin()`, `readRegisters()`, `writeRegister()`, `writeRegisters()` y `lastErrorString()`. Cada operación debe ser bloqueante con timeout y devolver `true` solo si la pantalla confirmó.

## Simular por USB

`JW_DWIN_ModbusUSB` lleva el Modbus de la pantalla por el USB de la tarjeta, junto con el texto de depuración, para usar la pantalla emulada de **JW DWIN → Simular → USB** sin adaptador RS-485 ni pantalla real.

Cada trama Modbus RTU completa (con su CRC) va dentro de un sobre, igual en los dos sentidos:

```
1B 4A 57 LEN <trama RTU>      ESC 'J' 'W', LEN = largo de la trama (4..255)
```

Todo byte fuera de un sobre es texto: lo que imprime el firmware aparece en la consola del simulador, y lo que se escribe en esa consola llega al firmware por `text()`.

```cpp
#include <JW_DWIN.h>
#include <JW_DWIN_ModbusUSB.h>

JW_DWIN_ModbusUSB transport(Serial, 1);  // esclavo 1
JW_DWIN dwin(transport);

void setup()
{
    Serial.begin(115200);  // los mismos baudios que en el simulador
    dwin.begin();
}

void loop()
{
    while (transport.text().available())
    {
        char c = transport.text().read();  // comandos que llegan por USB
    }
    // ... dwin.sendValue(), dwin.readValue(), Serial.println() ...
}
```

- **Leer comandos de `text()`, no de `Serial`:** el transporte consume los bytes del puerto mientras espera cada respuesta y guarda el texto en un buffer (`JW_DWIN_USB_TEXT_BUFFER`). Leer `Serial` directo puede tomar bytes de un sobre.
- **Texto de depuración:** se sigue imprimiendo con `Serial` como siempre. Cada sobre sale en una sola llamada a `write()`, que en el core ESP32 toma el mutex del UART, así que el texto de otras tareas no se mete dentro de un sobre. Lo que escribe la ROM o `ets_printf` sin pasar por `Serial` no respeta ese mutex.
- **Una sola tarea:** usar `text()` y `dwin` desde la misma tarea (normalmente `loop`); el transporte no tiene bloqueo propio.
- **Sobres inválidos:** uno con largo menor a 4 o CRC malo se descarta (`badEnvelopes()`); si llega mientras se espera una respuesta, la operación falla en el acto con `CRC error`.
- **Respuesta tardía:** una respuesta que llega después del timeout se descarta en la siguiente operación (`strayFrames()`).
- Ver el ejemplo `Simular_USB`.

## Errores posibles

Las operaciones devuelven `false` si la pantalla no confirmó. `lastErrorString()` informa la causa del transporte (por ejemplo, timeout o CRC).

## Notas de uso

- **Cache confirmada:** un valor, texto o estado táctil solo se da por enviado cuando la pantalla responde. Si falla, la siguiente llamada lo reintenta.
- **Sin enlace:** tras `JW_DWIN_OFFLINE_FAILURES` fallos seguidos (3), solo se intenta una operación cada `JW_DWIN_OFFLINE_RETRY_MS` (500 ms). El loop de la aplicación sigue corriendo sin esperar timeouts repetidos.
- **Reconexión:** al volver a comunicar se invalida la cache y `reconnected()` devuelve `true` una vez. Usarlo para volver a la página actual.
- **Valores editables por el operador:** leerlos como edición solo si `isConfirmed(obj)`; si no, reenviar primero el valor vigente. Así un envío fallido o el valor inicial de una pantalla reiniciada nunca se toman como una edición.
- **Estados enviados una sola vez** (íconos, habilitación táctil): tras una reconexión no se reenvían solos. Conviene enviarlos en cada ciclo del `loop` según el estado de la aplicación; la cache evita el tráfico repetido.
- **Teclas:** con varias teclas por página, sondearlas juntas con `checkKeys()` cada 50–100 ms en lugar de llamar `checkKey()` por tecla en cada ciclo del `loop`; los valores que el operador edita, igual con `readValues()`. Cada pedido espera a la pantalla, así que leer tecla por tecla satura el bus y retrasa las escrituras.
- Las operaciones son bloqueantes con timeout (`JW_DWIN_ModbusRTU::setTimeout`, 150 ms por defecto).

## Limitaciones

- Solo Modbus RTU (por RS-485 o en sobres por USB); falta el protocolo DGUS nativo.
- Una operación por vez (sin cola asíncrona).

## Pruebas

```bash
bash extras/test/run_tests.sh
```
