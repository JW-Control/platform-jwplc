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

**FAIL**.

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

### Interpretación

TCP_RX mejora sólo alrededor de 4–5 % frente al promedio G2 de 26 MHz / 4 chunks, pero supera el objetivo preferido de 5 ms y el sistema pierde convivencia visual SPI.

El hold de ~1.0 s ocurrió durante TCP_TX y fue capturado dentro del snapshot exacto de esa fase, por lo que no es contaminación temporal del harness. Sin embargo, el camino TCP_TX no cambió entre 4 y 8 chunks; con una sola observación no se atribuye causalidad directa al cambio de chunks. Si un hold similar reaparece con otro candidato, debe abrirse un diagnóstico específico de TCP_TX.

### Decisión

`G3_26MHZ_8CHUNKS=FAIL`

Razones suficientes para rechazo:

1. evento visual `SPI`;
2. hold TCP_TX > 10 ms;
3. TCP_RX ya cae en `REVIEW` con 5213 us;
4. la mejora de throughput TCP_RX es pequeña respecto al costo de convivencia.

No se continúa con 8 chunks en 20 ni 14 MHz. Al ser frecuencias SPI inferiores, no existe una razón experimental fuerte para esperar menores tiempos de ownership con el mismo burst; se prioriza un candidato intermedio.

## Siguiente candidato

Probar **26 MHz / 6 chunks** como compromiso:

- 1024 bytes máximos por chunk;
- máximo teórico por ventana: 6144 bytes;
- misma frecuencia de 26 MHz para aislar únicamente el cambio `8 -> 6`;
- mismos criterios de hold y diagnóstico visual.

Si 6 chunks también produce evento visual `SPI`, hold >10 ms o reaparece el hold extremo TCP_TX, detener el sweep de chunks y diagnosticar antes de seguir.

## Estado

| Frecuencia | Chunks | Estático | Smoke | Estado |
|---:|---:|---|---|---|
| 26 MHz | 4 | baseline G2 | PASS | BASELINE |
| 26 MHz | 8 | PASS | FAIL | REJECTED |
| 26 MHz | 6 | pendiente | pendiente | NEXT |
