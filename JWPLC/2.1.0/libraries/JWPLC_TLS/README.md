# JWPLC_TLS

HTTPS (TLS) del **JWPLC Basic** por cable o WiFi con el mismo código, sobre `JWPLC_Red`. Versión **0.1.0**
(línea `v2.1.0-alpha.19/JWPLC-Sim`).

```cpp
#include <JWPLC_TLS.h>

JWPLC_TLSClient cliente;

void loop()
{
    if (JWPLC_Red.isReady() && cliente.connect("ota.jw-control.com", 443))
    {
        cliente.print("GET /health HTTP/1.1\r\nHost: ota.jw-control.com\r\nConnection: close\r\n\r\n");
        // leer con available() / read() y terminar con stop()
    }
}
```

- `JWPLC_TLSClient` es un `Client` de Arduino: sirve para HTTPS, MQTT con TLS y JW Cloud.
- Sale por la red activa de `JWPLC_Red` (`useEthernet()`, `useWiFi()` o `useAuto()`).
- **Verifica el certificado** con las raíces de Let's Encrypt incluidas (ISRG X1, X2, YE, YR: las de JW
  Cloud y de la mayoría de sitios). Si el nombre no coincide o la CA no es de confianza, no conecta.
- `connect()` hace el saludo TLS (1-2 s en la placa; máximo `setHandshakeTimeout()`, 10 s por defecto).
  En programas con control en `loop()`, conectar desde una tarea aparte (así lo hará `JWPLC_Cloud`).
- RAM: ~40 KB durante la conexión, liberada en `stop()`.

| Llamada | Para qué |
|---|---|
| `connect(nombre, puerto)` | conectar y cifrar; 1 si quedó lista |
| `connect(ip, puerto)` | solo con `setInsecure()` (sin nombre no se verifica el certificado) |
| `print`, `write`, `available`, `read`, `peek`, `stop`, `connected` | como cualquier `Client` |
| `setCACert(pem)` / `useDefaultCA()` | otra CA (texto PEM que debe seguir existiendo) o volver a las de Let's Encrypt |
| `setInsecure()` | no verificar el certificado: **solo pruebas** |
| `setHandshakeTimeout(ms)` | tiempo máximo del saludo TLS |
| `lastError()` / `errorString()` | por qué falló: `SIN CONEXION`, `SALUDO TLS FALLIDO`, `CERTIFICADO NO VALIDO`, `SIN MEMORIA`, `CA NO VALIDA`, `CONEXION CERRADA` |
| `network()` | `RED_ETHERNET` o `RED_WIFI` |

**JWPLC Sim:** el mismo sketch funciona por cable. El cifrado lo hace la PC (servicio TLS del puente de red
del Sim, con las CA del sistema y `NODE_EXTRA_CA_CERTS`); `setCACert()` no se aplica en el Sim.

Ejemplo: `01.TLS_HTTPS_Cable`. Se distribuye en código fuente (sin `.a`). En la placa usa el mbedTLS del
ESP-IDF.
