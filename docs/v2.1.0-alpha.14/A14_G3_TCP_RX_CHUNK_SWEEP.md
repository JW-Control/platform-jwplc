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
- cualquier diagnóstico visual `SPI` confirmado por observación física: `FAIL`;
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

**FAIL observado, pendiente de separar teardown TCP de trabajo normal.**

| Modo | Throughput | Hold máx. | Clase |
|---|---:|---:|---|
| TCP_RX | 13.698309 Mbps | 5213 us | REVIEW |
| TCP_TX | 4.624685 Mbps | 1000754 us | FAIL |
| UDP_RX DUT | 10.312363 Mbps | 168 us | PASS_PREFERRED |
| UDP_TX | 5.032712 Mbps | 175 us | PASS_PREFERRED |

Observación física:

- en este smoke sí se observó parpadeo rojo del diagnóstico `SPI` en la TFT;
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
- se volvió a reportar observación visual `SPI` durante el bloque;
- `G3_STOP_TIMEOUT_SIGNATURE=REPRODUCED`.

La firma de ~1 s no es una coincidencia aislada: fue reproducida por segunda vez y nuevamente quedó asociada a TCP_TX.

### Causa del hold extremo — confirmada por A/B

La librería `JWPLC_Ethernet` inicializa `_timeout = 1000` ms en `EthernetClient`. Su método `stop()` inicia un cierre graceful y puede esperar hasta `_timeout`, con `delay(1)`, antes de forzar `socketClose()`.

El benchmark llama `tcpClient.stop()` desde `acceptTcpClient()`, y esa ruta se ejecuta dentro de `serviceTcp()` después de adquirir el mutex SPI y antes de medir/liberar el hold. Por ello el tiempo de espera completo de `stop()` queda cargado a `TCP_SPI_HOLD_MAX_US` y mantiene bloqueado el SPI compartido.

Se ejecutó un A/B diagnóstico con una copia temporal del sketch, sin modificar la fuente tracked:

- control: timeout por defecto **1000 ms**;
- variante diagnóstica: `tcpClient.setConnectionTimeout(200)`;
- misma frecuencia: 26 MHz;
- mismos 8 chunks;
- mismos 12 pares TCP_RX -> TCP_TX;
- mismo runner/snapshot exacto.

Resultado del A/B:

- firma ~1 s TCP_TX: **0/12** con timeout de 200 ms;
- firma desplazada 150–300 ms TCP_TX: **1/12**;
- hold TCP_TX máximo: **199547 us**;
- loop gap asociado: **201061 us**;
- hold TCP_RX máximo: **5424 us**;
- firma desplazada en TCP_RX: **0/12**;
- `G3_STOP_TIMEOUT_AB=SHIFTED_TO_DIAG_TIMEOUT`.

La relación observada es directa:

| Timeout configurado | Hold extremo observado |
|---:|---:|
| 1000 ms | 1000460–1000754 us |
| 200 ms | 199547 us |

Por tanto se considera establecida experimentalmente la causalidad:

- `TCP_EXTREME_HOLD_CAUSE=ETHERNETCLIENT_STOP_TIMEOUT`;
- `STOP_TIMEOUT_CAUSALITY=CONFIRMED_BY_AB`;
- `8_CHUNKS_CAUSED_1S_HOLD=NO`;
- `TCP_RX_8_CHUNKS_NORMAL_HOLD≈5.1–5.4ms`.

La observación física del A/B de 200 ms **no se usa como evidencia**, porque el operador indicó que no pudo estar atento a la TFT durante esa corrida. El dato válido del gate es el desplazamiento temporal medido en snapshot exacto.

### Alcance de la causa

El `stop()` bloqueante pertenece a la librería, pero el hold SPI de ~1 s observado aquí surge de la combinación de dos hechos:

1. `EthernetClient::stop()` puede esperar hasta `_timeout`;
2. el raw benchmark mantiene el mutex SPI global durante toda la llamada a `stop()`.

Esto confirma un defecto de la ruta de benchmark/ownership actual. **Todavía no demuestra por sí solo que todo uso normal de `EthernetClient::stop()` en producto mantenga el SPI bloqueado durante 1 s.** Antes de cambiar la API o el comportamiento legacy se debe auditar el uso productivo y definir una estrategia cooperativa compatible.

No se adopta `setConnectionTimeout(200)` como corrección de producto: sólo fue una sonda causal.

### Estado de decisión del candidato 8 chunks

El rechazo definitivo de 8 chunks queda suspendido hasta repetir el smoke con una ruta de teardown que no contamine el hold normal con la espera bloqueante de `stop()`.

Lo que sí está demostrado para el burst de 8 chunks:

- TCP_RX típico: ~13.5–13.8 Mbps;
- hold TCP_RX típico máximo por corrida: ~5.0–5.4 ms;
- TCP_RX >10 ms: 0/12 en el diagnóstico repetido;
- el hold extremo de ~1 s no pertenece al burst RX.

Por tanto:

`G3_26MHZ_8CHUNKS=REVIEW_PENDING_TEARDOWN_FIX`

## Siguiente gate

Antes de pasar a 6 chunks:

1. corregir únicamente la contaminación del teardown TCP en el benchmark/ownership de prueba;
2. no reducir el timeout productivo para ocultar el problema;
3. repetir smoke exacto 26 MHz / 8 chunks;
4. observar físicamente la TFT;
5. decidir 8 vs 6 chunks con telemetría de trabajo normal separada del cierre TCP.

## Estado

| Frecuencia | Chunks | Estático | Smoke | Estado |
|---:|---:|---|---|---|
| 26 MHz | 4 | baseline G2 | PASS | BASELINE |
| 26 MHz | 8 | PASS | contaminado por teardown | REVIEW / FIX TEARDOWN |
| 26 MHz | 6 | pendiente | pendiente | DEFERRED |
