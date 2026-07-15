# Ethernet_Continuous_Stress_TFT

Prueba continua del puerto Ethernet del **JWPLC Basic** orientada a detectar fallas intermitentes que aparecen después de varios minutos u horas, especialmente durante la revisión de soldadura del W5500, RJ45, magnetics y señales SPI.

## Qué hace

El ejemplo comienza automáticamente después de una espera de 8 segundos para permitir el arranque de Ethernet y DHCP. Luego ejecuta de forma repetitiva:

1. lectura del identificador de hardware Ethernet;
2. lectura del estado físico del enlace;
3. comprobación de la IP obtenida;
4. apertura de una conexión TCP;
5. solicitud HTTP `GET`;
6. recepción completa de la respuesta;
7. validación del código HTTP;
8. búsqueda de un texto conocido dentro del contenido.

De forma predeterminada consulta:

```text
http://example.com/
```

Y espera encontrar:

```text
Example Domain
```

El W5500 también se sondea cada `250 ms`, incluso entre solicitudes HTTP, para detectar desapariciones breves del hardware o caídas del enlace.

## Comportamiento ante una falla

- El resultado actual cambia a rojo.
- El LED `ERR` queda encendido y latcheado.
- Se conserva el tipo, detalle, fecha RTC y uptime del último error.
- Aunque la comunicación se recupere, el último error no se borra.
- `ESC` reconoce la alarma únicamente cuando la prueba actual vuelve a estar correcta.
- Los contadores no se borran al reconocer la alarma.
- El ejemplo no reinicia automáticamente el W5500, porque un reinicio podría ocultar una falla física intermitente.

Los contadores se mantienen en RAM y se reinician al apagar o resetear el ESP32.

## Controles

| Botón | Acción |
|---|---|
| `LEFT` / `RIGHT` | Cambia entre las tres páginas. |
| `OK` | Pausa o reanuda la prueba. |
| `UP` | Aumenta el intervalo entre solicitudes. |
| `DOWN` | Reduce el intervalo entre solicitudes. |
| `ESC` | Reconoce el LED `ERR` si la falla ya desapareció. |

Intervalos disponibles:

```text
250 ms, 500 ms, 1000 ms, 2000 ms, 5000 ms y 10000 ms
```

El intervalo inicial es `1000 ms`.

## Páginas TFT

### 1. ETH STRESS TEST

Muestra:

- modo `RUN`, `PAUSA` o `PROBANDO`;
- intervalo actual;
- hardware detectado;
- estado `LINK ON/OFF`;
- dirección IP;
- total de pruebas, éxitos y errores;
- último código HTTP, latencia y bytes recibidos;
- racha actual y máxima de fallas;
- tiempo total de ejecución;
- antigüedad del último resultado correcto;
- estado latcheado del LED `ERR`.

### 2. CONTADORES DE FALLA

Separa los errores en:

- timeout del mutex SPI;
- W5500 no detectado;
- link físico caído;
- DHCP o IP inválida;
- runtime Ethernet no listo;
- fallo `connect()` o DNS;
- conexión sin bytes recibidos;
- timeout durante la recepción;
- línea HTTP inválida;
- código HTTP de error;
- contenido inesperado.

También muestra eventos de caída y recuperación de hardware/link, tráfico recibido y latencias.

### 3. ULTIMO ERROR

Conserva:

- tipo de error;
- fecha y hora del RTC;
- uptime en el que ocurrió;
- detalle técnico;
- última línea de estado HTTP recibida;
- estado de la alarma latcheada.

## Interpretación orientativa

| Error TFT | Posibles causas |
|---|---|
| `SPI LOCK` | Contención anormal del bus, problema de SPI, CS, alimentación o soldadura del W5500. |
| `W5500 NO DETECTADO` | Alimentación del chip, señales SPI, CS, soldadura o daño del W5500. |
| `LINK OFF` | Cable, conector RJ45, magnetics, pares Ethernet, PHY o soldadura asociada. |
| `DHCP/IP` | DHCP no disponible, pérdida de red, recepción/transmisión inestable o configuración incorrecta. |
| `CONNECT/DNS` | DNS, gateway, acceso a Internet, servidor remoto o transmisión TCP intermitente. |
| `SIN RESPUESTA` | El TCP abrió, pero no llegaron datos; revisar recepción, red o servidor. |
| `TIMEOUT RX` | Respuesta incompleta, enlace intermitente, pérdida de datos o servidor lento. |
| `HTTP INVALIDO` | Datos truncados o corruptos, respuesta no HTTP o interrupción durante la recepción. |
| `CODIGO HTTP` | El servidor respondió, pero con un código fuera de `200..399`. |
| `CONTENIDO` | La respuesta llegó, pero no contiene el texto esperado; puede ser truncamiento, portal cautivo o cambio del servidor. |

La clasificación ayuda a ubicar la etapa probable, pero no reemplaza la inspección física con lupa, multímetro u osciloscopio.

## Prueba recomendada para revisar soldadura

Usar `example.com` prueba además DNS, gateway e Internet. Para aislar el hardware del JWPLC y evitar que una falla externa parezca una falla de soldadura, conviene usar una PC dentro de la misma red.

En la PC crea un archivo `index.html` con:

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

Luego modifica al inicio del sketch:

```cpp
static const char STRESS_HOST[] = "192.168.1.100"; // IP de la PC
static const char STRESS_PATH[] = "/";
static const char EXPECTED_TOKEN[] = "JWPLC_STRESS_OK";
static const uint16_t STRESS_PORT = 8080;
```

Para intervalos de `250 ms` o `500 ms`, se recomienda usar este servidor local y no un sitio público.

## Registro por USB

El monitor Serial trabaja a `115200` baudios y emite:

```text
[ETH-STRESS][OK] #125 HTTP 200 | 142 ms | 1256 bytes
[ETH-STRESS][ERROR] #126 LINK OFF | PHY/RJ45 sin enlace...
[ETH-STRESS][EVENTO] W5500 NO DETECTADO | W5500 desaparecio...
```

Cada 5 segundos imprime además un resumen general.

## Validación sugerida

- [ ] Compilar con la placa `JWPLC Basic`.
- [ ] Confirmar IP DHCP válida.
- [ ] Dejar la prueba activa al menos 30 minutos.
- [ ] Mover suavemente el cable y el conector para buscar falsos contactos.
- [ ] Aplicar calentamiento moderado y controlado sobre la zona sospechosa.
- [ ] Confirmar que una desconexión del RJ45 genera `LINK OFF`.
- [ ] Confirmar que la reconexión incrementa el contador `Link ON`.
- [ ] Confirmar que el último error permanece visible después de recuperarse.
- [ ] Confirmar que `ESC` apaga `ERR` únicamente después de recuperarse.
- [ ] Repetir con un servidor HTTP local para descartar Internet o DNS.

## Consideraciones SPI y TFT

El W5500 comparte el bus SPI con TFT, FRAM y microSD. La transacción HTTP toma el mutex SPI global durante la conexión y recepción. Mientras una solicitud está activa, la TFT puede pausar brevemente; al finalizar se actualizan únicamente sus filas de contenido y no se limpia la pantalla completa en cada callback.
