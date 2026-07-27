# Ethernet_Continuous_Stress_TFT

Prueba continua y diagnóstico por capas del puerto Ethernet del **JWPLC Basic**. Esta revisión está orientada a validar durante periodos prolongados:

- bus SPI y presencia del W5500;
- enlace físico RJ45/PHY;
- DHCP e IP local;
- resolución DNS;
- conexión TCP;
- recepción HTTP;
- código y contenido recibido;
- estabilidad frente a calentamiento, movimiento del conector y falsos contactos.

## Mejoras de la revisión v3

### 1. Menos reescrituras en la TFT

La interfaz mantiene una caché por fila con:

- etiqueta;
- valor mostrado;
- color.

Antes de limpiar y volver a escribir una fila, compara el contenido nuevo con el último contenido dibujado. Si texto y color no cambiaron, no toca esa zona de la TFT.

Con esto:

- el título y el pie solo se reconstruyen al cambiar de página;
- hardware, link e IP no se reescriben en cada refresco si permanecen iguales;
- en la actualización de un segundo normalmente solo cambia la fila de uptime;
- durante cada transacción se actualizan únicamente las filas cuyos contadores o tiempos cambiaron.

La optimización reduce tráfico innecesario sobre el bus SPI compartido, pero no elimina las actualizaciones necesarias para ver en tiempo real pruebas, errores y tiempos.

### 2. Arranque cualificado

La versión anterior convertía cualquier estado de enlace distinto de `LinkON` en `LINK OFF`. Durante los primeros instantes la librería puede devolver `Unknown`, que no equivale a una caída física.

La revisión v3 separa:

```text
LINK ?    = todavía no existe una lectura válida
LINK OFF  = el PHY respondió y confirmó ausencia de enlace
LINK ON   = enlace físico confirmado
```

Al arrancar, no ejecuta la primera prueba ni genera una alarma inmediatamente. Espera a observar simultáneamente:

```text
W5500 detectado
+ LINK conocido y ON
+ runtime Ethernet listo
+ IP válida
```

La condición debe permanecer estable durante:

```text
750 ms
```

El tiempo máximo de espera inicial es:

```text
20 s
```

Durante ese periodo la TFT puede mostrar:

```text
ARRANQUE: ESPERANDO W5500
ARRANQUE: LEYENDO LINK
ARRANQUE: ESPERANDO LINK ON
ARRANQUE: ESPERANDO RUNTIME
ARRANQUE: ESPERANDO DHCP/IP
ARRANQUE: ESTABILIZANDO
```

Si pasan 20 segundos sin completar la cualificación, comienza el diagnóstico normal y reporta la causa real encontrada. Una vez cualificado el arranque, las caídas posteriores sí se registran inmediatamente.

### 3. Prueba más exigente

Los intervalos disponibles son:

```text
CONT, 50 ms, 100 ms, 250 ms, 500 ms,
1000 ms, 2000 ms, 5000 ms y 10000 ms
```

El intervalo inicial es:

```text
500 ms
```

Ahora el intervalo se mide **de inicio a inicio**. Por ejemplo, si se seleccionan `500 ms` y la transacción tarda `330 ms`, la siguiente prueba comienza aproximadamente 170 ms después. En la versión anterior el intervalo empezaba a contarse al terminar, por lo que la frecuencia real era menor.

Si la transacción tarda más que el intervalo elegido, la siguiente inicia apenas termina la anterior. El modo `CONT` no añade ninguna pausa entre transacciones.

Controles:

| Botón | Acción |
|---|---|
| `LEFT` / `RIGHT` | Cambia de página. |
| `OK` | Pausa o reanuda. |
| `UP` | Reduce la exigencia aumentando el intervalo. |
| `DOWN` | Aumenta la exigencia reduciendo el intervalo. |
| `ESC` | Reconoce la alarma cuando la falla actual desapareció. |

### 4. DNS periódico e IP cacheada

Resolver DNS en cada transacción puede convertir al servidor DNS en el cuello de botella y no exige de forma directa el camino TCP/HTTP.

La revisión v3:

1. realiza una resolución DNS real;
2. conserva el IP obtenido;
3. usa ese IP para las siguientes conexiones TCP/HTTP;
4. vuelve a consultar DNS cada 20 pruebas.

La TFT y el monitor Serial indican:

```text
DNS LIVE
```

cuando se hizo una consulta real, o:

```text
DNS CACHE
```

cuando se utilizó el último IP válido.

Así se mantiene la supervisión periódica de DNS, pero la mayor parte de la carga se concentra en:

- apertura de sockets TCP;
- transmisión HTTP;
- recepción de datos;
- lectura/escritura del W5500 por SPI.

### 5. Normalización de códigos DNS

La implementación de `DNSClient` usada por la librería Ethernet puede transportar internamente algunos errores negativos mediante un retorno de 16 bits. Por ello, un timeout `-1` puede observarse externamente como `65535`, `-2` como `65534`, y así sucesivamente.

El sketch normaliza los valores `65535..65530` nuevamente a `-1..-6` antes de clasificarlos. Con ello:

```text
65535 -> -1 -> DNS TIMEOUT
65534 -> -2 -> DNS SERVIDOR
65533..65530 -> DNS RESPUESTA
```

La captura obtenida con la revisión anterior, que mostraba `DNS OTRO raw=65535`, corresponde realmente a un `DNS TIMEOUT`. En la v3 corregida quedará identificado con su categoría apropiada.

Cuando una consulta `DNS LIVE` falla y ya existe una IP cacheada, el error se conserva y se ejecuta la prueba TCP auxiliar. Las siguientes pruebas vuelven a usar la IP cacheada hasta el próximo refresco DNS periódico, evitando que una caída temporal del servidor DNS detenga el esfuerzo continuo de TCP/HTTP.

## Frecuencia de sondeo físico

Aunque no haya una transacción HTTP en ese instante, el sketch consulta cada:

```text
50 ms
```

- presencia del W5500;
- estado de link;
- dirección IP.

Esto permite detectar eventos breves como:

```text
W5500 -> no detectado -> W5500
LINK ON -> LINK OFF -> LINK ON
```

El sondeo no se ejecuta mientras una transacción mantiene ocupado el mutex SPI, para no interferir con ella.

## Diagnóstico por capas

```text
W5500 / SPI
    ↓
LINK físico
    ↓
DHCP / IP
    ↓
DNS LIVE o CACHE
    ↓
TCP al IP resuelto
    ↓
HTTP
    ↓
Código y contenido
```

Errores diferenciados:

| Error TFT | Etapa |
|---|---|
| `SPI LOCK` | No se adquirió el bus SPI compartido. |
| `W5500 NO DETECTADO` | El controlador dejó de responder o no fue detectado. |
| `LINK OFF` | El PHY confirmó ausencia de enlace. |
| `ETH NO LISTO` | Runtime o lectura de link todavía no válida. |
| `DHCP/IP` | Dirección IP inválida o DHCP fallido. |
| `DNS TIMEOUT` | No llegó respuesta DNS dentro del tiempo. |
| `DNS SERVIDOR` | Servidor DNS inválido. |
| `DNS RESPUESTA` | Respuesta DNS truncada, inválida, rechazada o sin registros A. |
| `DNS OTRO` | Fallo de socket/envío u otro retorno DNS. |
| `TCP CONNECT` | No se abrió TCP al IP resuelto. |
| `SIN RESPUESTA` | TCP abrió, pero no llegaron bytes HTTP. |
| `TIMEOUT RX` | La recepción no terminó dentro del tiempo. |
| `HTTP INVALIDO` | No llegó una línea de estado válida. |
| `CODIGO HTTP` | Código fuera de `200..399`. |
| `CONTENIDO` | No apareció el texto esperado. |

## Registro correcto esperado

Una consulta DNS real:

```text
[ETH-STRESS][OK] #1 DNS LIVE/42ms -> 93.184.216.34 |
TCP 18ms | 1B 138ms | TOTAL 331ms | HTTP 200 | 868 bytes
```

Las pruebas siguientes pueden utilizar la caché:

```text
[ETH-STRESS][OK] #2 DNS CACHE/0ms -> 93.184.216.34 |
TCP 17ms | 1B 135ms | TOTAL 326ms | HTTP 200 | 868 bytes
```

Después de 20 pruebas se fuerza una nueva consulta `DNS LIVE`.

## Referencia LAN opcional

Para validar soldadura y hardware sin depender de Internet, habilita una PC de la misma red:

```cpp
static const bool ENABLE_LOCAL_REFERENCE = true;
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

Y ejecuta desde esa carpeta:

```bash
python -m http.server 8080 --bind 0.0.0.0
```

Para usar `CONT`, `50 ms`, `100 ms` o `250 ms` durante pruebas largas, se recomienda configurar el **destino principal** también hacia ese servidor local, en lugar de cargar continuamente un sitio público.

Ejemplo:

```cpp
static const char STRESS_HOST[] = "192.168.0.4";
static const char STRESS_PATH[] = "/";
static const char EXPECTED_TOKEN[] = "JWPLC_STRESS_OK";
static const uint16_t STRESS_PORT = 8080;
```

## Interpretación para soldadura

Resultados favorables:

- W5500 siempre detectado;
- `LINK ON` continuo;
- IP estable;
- referencia LAN correcta;
- ausencia de `SPI LOCK`;
- TCP/HTTP correctos durante frío, calentamiento y movimiento controlado.

Resultados que aumentan la sospecha física:

- `W5500 NO DETECTADO`;
- `SPI LOCK` repetitivo;
- `LINK OFF` al mover o calentar el conector;
- referencia LAN que falla al mismo tiempo que Internet;
- errores repetitivos de RX o datos corruptos contra un servidor local estable.

Una falla aislada `DNS TIMEOUT` con:

```text
W5500 detectado
LINK ON
IP válida
TCP CACHE OK
```

apunta al servidor DNS o a la red externa, no demuestra una soldadura defectuosa.

## Páginas TFT

### 1. ETH STRESS TEST

Muestra:

- modo RUN, PAUSA o PROBANDO;
- intervalo actual;
- W5500, link e IP;
- pruebas, éxitos, errores y racha;
- último resultado;
- origen probable;
- tiempos DNS/TCP/primer byte/total;
- uptime y alarma.

### 2. DIAGNOSTICO CAPAS

Muestra:

- DNS LIVE/CACHE;
- código DNS bruto normalizado;
- servidor DNS e IP resuelta;
- TCP;
- HTTP;
- tiempos;
- prueba contra IP cacheada;
- referencia LAN.

### 3. CONTADORES DE FALLA

Muestra contadores separados por capa, tráfico recibido, eventos físicos, latencias y racha máxima.

### 4. ULTIMO ERROR

Conserva tipo, fecha RTC, origen probable, detalle y estado latcheado de la alarma.

## Validación recomendada

- [ ] Compilar con la placa `JWPLC Basic`.
- [ ] Confirmar que durante el arranque aparece primero `LINK ?`, espera o estabilización, sin alarma falsa.
- [ ] Confirmar que después de `LINK ON` e IP válida comienza la prueba.
- [ ] Verificar que una desconexión posterior sí genera `LINK OFF`.
- [ ] Confirmar que las filas TFT estáticas no parpadean ni se reescriben visualmente.
- [ ] Ejecutar 30 minutos a `500 ms`.
- [ ] Ejecutar una prueba local a `250 ms`, `100 ms` y `CONT`.
- [ ] Repetir en frío y con calentamiento moderado controlado.
- [ ] Mover suavemente cable y conector durante la prueba local.
- [ ] Confirmar alternancia `DNS LIVE` / `DNS CACHE`.
- [ ] Provocar o esperar una falla DNS y confirmar que `65535` se presenta como `DNS TIMEOUT raw=-1`.
- [ ] Confirmar que el último error permanece visible después de recuperarse.

## Consideraciones

El W5500 comparte el bus SPI con TFT, FRAM y microSD. La caché de filas reduce escrituras innecesarias de la pantalla, pero DNS/TCP/HTTP conservan el mutex SPI mientras acceden al W5500. Una transacción lenta puede retrasar temporalmente el refresco visual; esto es deliberado para evitar colisiones sobre el bus compartido.
