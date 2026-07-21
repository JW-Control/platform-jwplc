# Ethernet_Continuous_Stress_TFT

Prueba continua y diagnóstico por capas del puerto Ethernet del **JWPLC Basic**. Está orientada a detectar fallas intermitentes que aparecen después de varios minutos u horas y a separar con mayor precisión si el problema está en:

- bus SPI o presencia del W5500;
- enlace físico RJ45/PHY;
- DHCP o dirección IP;
- resolución DNS;
- apertura de la conexión TCP;
- recepción HTTP;
- código o contenido recibido;
- red externa frente a una referencia local dentro de la LAN.

## Cambio principal de esta revisión

La versión inicial agrupaba en `CONNECT/DNS` cualquier fallo producido por:

```cpp
EthernetClient::connect(host, port)
```

Esa llamada resuelve el nombre y abre TCP internamente, por lo que no permitía identificar cuál de las dos etapas había fallado.

La revisión actual ejecuta las capas por separado:

```text
W5500/SPI
  -> LINK
  -> DHCP/IP
  -> DNS
  -> TCP al IP resuelto
  -> HTTP
  -> código
  -> contenido
```

## Destino predeterminado

La prueba principal consulta:

```text
http://example.com/
```

Y espera encontrar:

```text
Example Domain
```

El intervalo inicial es `1000 ms`. Los intervalos disponibles son:

```text
250 ms, 500 ms, 1000 ms, 2000 ms, 5000 ms y 10000 ms
```

Para un sitio público conviene mantener `1000 ms` o más. Los intervalos de `250 ms` y `500 ms` están pensados principalmente para un servidor local controlado.

## Diagnóstico DNS separado

El sketch usa `DNSClient` directamente y registra:

- servidor DNS entregado por la red;
- código bruto devuelto por la librería;
- descripción del resultado;
- tiempo empleado;
- IP resuelta.

Resultados DNS visibles:

| Resultado | Significado |
|---|---|
| `DNS TIMEOUT` | No llegó respuesta dentro del tiempo configurado. |
| `DNS SERVIDOR` | El servidor DNS configurado es inválido. |
| `DNS RESPUESTA` | Respuesta truncada, inválida, rechazada o con error. |
| `DNS OTRO` | No se pudo abrir/enviar la consulta o apareció otro retorno. |

El log incluye el valor bruto, por ejemplo:

```text
DNS raw=-1 TIMEOUT 4512ms server=192.168.0.1
```

## Diagnóstico TCP separado

Después de resolver DNS, la conexión se realiza con el IP explícito:

```cpp
stressClient.connect(remoteIp, STRESS_PORT)
```

Así un fallo aparece como:

```text
TCP CONNECT
```

Y queda registrado:

- IP destino;
- puerto;
- tiempo de conexión;
- estado físico posterior del W5500, link e IP local.

## Prueba con IP cacheada

Cuando DNS falla y anteriormente existió una resolución correcta, el sketch intenta abrir TCP contra el último IP resuelto.

Ejemplo:

```text
DNS TIMEOUT
TCP CACHE OK 24ms
```

Esta combinación aporta una evidencia fuerte de que:

- la resolución DNS falló;
- el W5500 seguía operativo;
- el enlace y la ruta TCP al destino cacheado continuaban funcionando.

No convierte la prueba principal en exitosa: el fallo DNS sigue contabilizado y el LED `ERR` permanece latcheado.

## Referencia LAN opcional

Para separar Internet/DNS de una posible falla del JWPLC se puede habilitar una PC dentro de la misma red como referencia TCP local.

Al inicio del sketch:

```cpp
static const bool ENABLE_LOCAL_REFERENCE = false;
static IPAddress LOCAL_REFERENCE_IP(192, 168, 0, 4);
static const uint16_t LOCAL_REFERENCE_PORT = 8080;
```

En la PC crea `index.html`:

```html
<!doctype html>
<html>
  <body>JWPLC_STRESS_OK</body>
</html>
```

Desde esa carpeta ejecuta:

```bash
python -m http.server 8080 --bind 0.0.0.0
```

Después:

1. confirma la IP actual de la PC;
2. colócala en `LOCAL_REFERENCE_IP`;
3. cambia `ENABLE_LOCAL_REFERENCE` a `true`;
4. recompila y sube el sketch.

La referencia LAN es una conexión TCP auxiliar. Debe habilitarse solamente cuando el servidor local esté activo; de lo contrario produciría un `LAN FAIL` esperado y perdería valor diagnóstico.

## Interpretación automática

Después de cada error, el sketch ejecuta las pruebas auxiliares disponibles, vuelve a leer W5500/link/IP y genera un origen probable.

| Origen mostrado | Interpretación |
|---|---|
| `POSIBLE SPI/W5500/SOLDADURA` | Falló el mutex SPI o desapareció el W5500. |
| `POSIBLE RJ45/CABLE/MAGNETICOS` | El enlace físico cayó. |
| `DNS; TCP/W5500 SIGUIO OPERATIVO` | DNS falló, pero TCP al IP cacheado funcionó. |
| `DNS/INTERNET; LAN LOCAL OK` | La referencia local funcionó; el problema está fuera de la LAN local. |
| `INTERNET/SERVIDOR; LAN LOCAL OK` | TCP/HTTP externo falló, pero la referencia local funcionó. |
| `LAN/W5500/CABLE/SWITCH: REVISAR` | También falló la referencia LAN con HW/link/IP todavía visibles. |
| `NO CONCLUYENTE; FISICO SIGUE ON` | La falla ocurrió más arriba, sin caída observable de hardware o link. |

La clasificación es una ayuda de diagnóstico, no una prueba eléctrica definitiva.

## Errores separados

| Error TFT | Etapa |
|---|---|
| `SPI LOCK` | Acceso al bus SPI compartido. |
| `W5500 NO DETECTADO` | Presencia del controlador Ethernet. |
| `LINK OFF` | Capa física RJ45/PHY. |
| `DHCP/IP` | Configuración de red local. |
| `ETH NO LISTO` | Estado del runtime Ethernet. |
| `DNS TIMEOUT` | Resolución DNS sin respuesta. |
| `DNS SERVIDOR` | Servidor DNS inválido. |
| `DNS RESPUESTA` | Respuesta DNS corrupta, truncada o rechazada. |
| `DNS OTRO` | Fallo interno/envío/socket DNS. |
| `TCP CONNECT` | No se abrió TCP al IP resuelto. |
| `SIN RESPUESTA` | TCP abrió, pero no llegaron bytes HTTP. |
| `TIMEOUT RX` | La recepción no terminó dentro del tiempo. |
| `HTTP INVALIDO` | No llegó una línea HTTP válida. |
| `CODIGO HTTP` | Código fuera de `200..399`. |
| `CONTENIDO` | No apareció el texto esperado. |

## Tiempos registrados

La página de capas y el monitor Serial muestran:

- tiempo DNS;
- tiempo de apertura TCP;
- tiempo hasta el primer byte;
- tiempo total HTTP;
- bytes recibidos;
- IP resuelta;
- resultado de la prueba cacheada;
- resultado de la referencia LAN.

Ejemplo correcto:

```text
[ETH-STRESS][OK] #125 DNS 38ms -> 93.184.216.34 | TCP 17ms | 1B 142ms | TOTAL 335ms | HTTP 200 | 868 bytes
```

Ejemplo de falla DNS aislada:

```text
[ETH-STRESS][ERROR] #126 DNS TIMEOUT | DNS fallo raw=-1 TIMEOUT; server=192.168.0.1; 4512ms | POST HW=W5500 LINK=ON IP=192.168.0.31 | DIAG: DNS; TCP/W5500 SIGUIO OPERATIVO
[ETH-STRESS][CAPAS] DNS raw=-1 TIMEOUT 4512ms server=192.168.0.1 resolved=0.0.0.0 | TCP=FAIL/NO 0ms | CACHE=TCP CACHE OK 24ms | LOCAL=DESHABILITADA
```

## Páginas TFT

### 1. ETH STRESS TEST

Muestra:

- RUN, PAUSA o PROBANDO;
- intervalo;
- W5500;
- link;
- IP local;
- pruebas correctas y fallidas;
- último resultado;
- origen probable;
- tiempos DNS/TCP/primer byte/total;
- uptime y estado latcheado del LED `ERR`.

### 2. DIAGNOSTICO CAPAS

Muestra:

- resultado DNS y código bruto;
- servidor DNS e IP resuelta;
- resultado y tiempo TCP;
- resultado HTTP, código y bytes;
- primer byte y tiempo total;
- prueba TCP contra IP cacheada;
- referencia LAN opcional.

### 3. CONTADORES DE FALLA

Separa los contadores de:

- SPI, W5500, link, DHCP/IP y runtime;
- DNS timeout, servidor, respuesta y otros;
- TCP connect;
- recepción y timeout;
- HTTP inválido, código y contenido;
- pruebas cacheadas;
- caídas y recuperaciones de hardware/link;
- latencia mínima, promedio y máxima.

### 4. ULTIMO ERROR

Conserva:

- tipo;
- fecha RTC;
- origen probable;
- detalle técnico;
- snapshot posterior de HW/link/IP;
- estado de la alarma latcheada.

## Alarma

- El LED `ERR` queda latcheado después de cualquier falla.
- El último error permanece visible aunque la siguiente prueba sea correcta.
- `ESC` reconoce la alarma solamente cuando la falla actual ya desapareció.
- Los contadores se mantienen hasta reiniciar el ESP32.
- El sketch no reinicia automáticamente el W5500 para evitar ocultar una falla física intermitente.

## Controles

| Botón | Acción |
|---|---|
| `LEFT` / `RIGHT` | Cambia entre las cuatro páginas. |
| `OK` | Pausa o reanuda la prueba. |
| `UP` | Aumenta el intervalo. |
| `DOWN` | Reduce el intervalo. |
| `ESC` | Reconoce el LED `ERR` después de recuperarse. |

## Prueba recomendada de soldadura

Para evaluar específicamente el hardware:

1. habilita la referencia LAN con un servidor HTTP estable en la PC;
2. usa un cable Ethernet y switch conocidos como buenos;
3. deja la prueba activa al menos 30 minutos;
4. mueve suavemente el cable y el conector;
5. aplica calentamiento moderado y controlado sobre la zona sospechosa;
6. observa si aparecen `W5500 NO DETECTADO`, `LINK OFF`, `SPI LOCK` o `LAN FAIL`;
7. repite en frío y después de varios minutos de calentamiento.

Una falla única de DNS con W5500, link e IP estables no demuestra una soldadura defectuosa. Una desaparición del W5500, una caída de link o una falla repetitiva contra la referencia LAN sí aumenta mucho la sospecha sobre hardware, cableado o soldadura.

## Consideraciones SPI y TFT

El W5500 comparte SPI con TFT, FRAM y microSD. DNS, TCP y HTTP toman el mutex SPI global mientras utilizan el W5500. La TFT puede pausar durante una transacción lenta, pero la pantalla completa solamente se reconstruye al cambiar de página; durante la operación normal se limpian y actualizan filas de contenido.