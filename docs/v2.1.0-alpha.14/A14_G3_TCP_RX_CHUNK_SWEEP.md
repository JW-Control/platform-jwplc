# Alpha14 — G3: sweep de chunks TCP RX

## Objetivo

Evaluar si aumentar el número máximo de chunks TCP RX procesados por ventana de ownership SPI mejora el throughput una vez que G2 confirmó una meseta de frecuencia entre 20, 24 y 26 MHz.

G3 cambia deliberadamente el algoritmo respecto a G2. Por ello sus candidatos se clasifican por separado y no invalidan los resultados G2.

## Baseline de referencia

Configuración G2 en 26 MHz:

- `TCP_RX_MAX_CHUNKS_PER_LOCK = 4`;
- máximo 1024 bytes por chunk;
- máximo teórico por ventana: 4096 bytes;
- TCP_RX promedio 3 × 10 s: **13.099 Mbps**;
- TCP_RX hold máximo observado: **3252 us**;
- cero eventos visuales `SPI`;
- cero errores de ownership/transporte.

## Criterios G3

- `TCP_SPI_HOLD_MAX_US <= 5000`: `PASS_PREFERRED`;
- `5000 < TCP_SPI_HOLD_MAX_US <= 10000`: `REVIEW`;
- `TCP_SPI_HOLD_MAX_US > 10000`: `FAIL`;
- cualquier diagnóstico visual `SPI`: `FAIL`;
- cualquier error de ownership/transporte o pérdida de integridad: `FAIL`.

El límite de 10 ms es un techo experimental, no una relajación automática del criterio de convivencia SPI.

## Candidato 26 MHz / 8 chunks

### G3-A — cambio estático

**PASS**.

- frecuencia efectiva: 26 MHz;
- chunks: `4 -> 8`;
- máximo teórico por ventana: 8192 bytes;
- `w5100.h`: diff `1/1` por frecuencia;
- firmware raw: diff `1/1` por chunks;
- firmware candidato SHA256: `08E16E60F008371416859B63F93D7C8B7D2FB88325FB7281AC6C5C72983FEA8F`;
- runner raw intacto;
- `core.a` y `libJW_SD.a` intactos;
- staged: 0.

### G3-B — smoke físico exacto

**FAIL observado**.

| Modo | Throughput | Hold máx. | Clase |
|---|---:|---:|---|
| TCP_RX | 13.698309 Mbps | 5213 us | REVIEW |
| TCP_TX | 4.624685 Mbps | 1000754 us | FAIL |
| UDP_RX DUT | 10.312363 Mbps | 168 us | PASS_PREFERRED |
| UDP_TX | 5.032712 Mbps | 175 us | PASS_PREFERRED |

Observación física:

- se observó parpadeo rojo del diagnóstico `SPI` en la TFT;
- `VISUAL_SPI_EVENTS=1`;
- `G3_PHYSICAL_SMOKE=FAIL`.

Los snapshots exactos no reportaron errores de transporte, locks SPI ni errores UDP begin/write/end. La integridad UDP_TX fue correcta.

### Diagnóstico de transición TCP_RX -> TCP_TX

Se repitieron **12 pares** TCP_RX -> TCP_TX sin recompilar ni subir firmware, manteniendo exactamente 26 MHz / 8 chunks y snapshots exactos del runner.

Resultado:

- TCP_RX hold máximo global: **5396 us**;
- TCP_RX holds >10 ms: **0/12**;
- firma ~1 s en TCP_RX: **0/12**;
- TCP_TX hold máximo global: **1000460 us**;
- firma ~1 s en TCP_TX: **1/12**;
- `LOOP_GAP_MAX_US` del evento: **1002922 us**;
- se volvió a observar diagnóstico visual rojo `SPI`;
- `G3_STOP_TIMEOUT_SIGNATURE=REPRODUCED`.

La firma de ~1 s no es una coincidencia aislada: fue reproducida por segunda vez y nuevamente quedó asociada a TCP_TX.

### Hipótesis de causa — `EthernetClient::stop()`

La librería `JWPLC_Ethernet` inicializa `_timeout = 1000` ms en `EthernetClient`. Su método `stop()` inicia un cierre graceful y puede esperar hasta `_timeout`, con `delay(1)`, antes de forzar `socketClose()`.

El benchmark llama `tcpClient.stop()` desde `acceptTcpClient()`, y esa ruta se ejecuta dentro de `serviceTcp()` después de adquirir el mutex SPI y antes de medir/liberar el hold. Por ello existe una ruta plausible y concreta para retener ownership SPI alrededor de 1 segundo durante el cierre de un cliente anterior.

Clasificación actual:

- `TCP_TX_1S_HOLD_REPRODUCED=YES`;
- `STOP_TIMEOUT_1000MS_MATCH=YES`;
- `STOP_TIMEOUT_CAUSALITY=HIGH_CONFIDENCE_HYPOTHESIS`;
- `8_CHUNKS_CAUSED_1S_HOLD=NOT_ESTABLISHED`.

### A/B diagnóstico pendiente

Antes de rechazar definitivamente 8 chunks o pasar a 6 chunks, se ejecutará un A/B controlado:

- fuente tracked permanece intacta;
- se crea una copia temporal del sketch;
- sólo en esa copia se aplica `tcpClient.setConnectionTimeout(200)` al cliente aceptado;
- se compila/sube esa variante diagnóstica;
- se repiten 12 pares TCP_RX -> TCP_TX;
- si la firma se desplaza de ~1.0 s a ~0.2 s, se considerará evidencia causal fuerte de `EthernetClient::stop()`.

Este cambio de timeout es exclusivamente diagnóstico y **no constituye todavía una corrección de producto**.

### Estado de decisión

`G3_26MHZ_8CHUNKS=FAIL_OBSERVED_PENDING_CAUSE_SEPARATION`

El smoke observado sigue siendo FAIL porque hubo evento visual `SPI` y un hold >10 ms. Sin embargo, la evidencia actual indica que el hold extremo puede pertenecer al cierre TCP legacy y no al tamaño del burst TCP_RX. Por ello se difiere el rechazo definitivo del candidato hasta completar el A/B de timeout.

## Candidato 26 MHz / 6 chunks

Queda **diferido** hasta cerrar el A/B del timeout. Si se demuestra que el evento de ~1 s pertenece a `stop()`, se reevaluará 8 chunks usando telemetría que separe el cierre TCP de la ventana de trabajo normal antes de decidir si 6 chunks es necesario.

## Estado

| Frecuencia | Chunks | Estático | Smoke | Estado |
|---:|---:|---|---|---|
| 26 MHz | 4 | baseline G2 | PASS | BASELINE |
| 26 MHz | 8 | PASS | FAIL observado | A/B TIMEOUT PENDING |
| 26 MHz | 6 | pendiente | pendiente | DEFERRED |
