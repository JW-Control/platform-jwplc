# Alpha11 — LIVE buffer parcial y telemetría

Fecha: 2026-09-05

## Contexto

El primer bridge de Live Preview reservaba un framebuffer RGB565 completo de 320 × 170 píxeles:

```text
320 × 170 × 2 = 108800 bytes
```

Ese enfoque produjo overflow de DRAM en JWPLC Basic v2. La corrección posterior cambió a streaming de una sola fila de 320 píxeles (640 B), eliminó el overflow y, junto con 500000 baud + ACK por frame, dio estabilidad visual física.

## Evolución del buffer parcial

### Etapa 1 — 16 filas @ 500000 baud

Se validó un framebuffer parcial de 16 filas:

```text
FRAME_BUFFER_ROWS=16
FRAME_BUFFER_PIXELS=320*16
FRAME_BUFFER_BYTES=10240
SERIAL_BAUD=500000
SERIAL_RX_BUFFER=8192
HOST_TX_CHUNK=1024
FLOW_CONTROL=FRAME_ACK
```

Esto reduce el máximo aproximado desde 170 operaciones de dibujo por frame a 11 operaciones.

La prueba física mantuvo:

```text
VISUAL_CORRUPTION=0
FRAME_ACK=PASS
```

pero la sensación de fluidez durante drag continuo fue considerada regular. La compilación observada con 16 filas fue:

```text
PROGRAM_BYTES=416397
GLOBAL_BYTES=42884
GLOBAL_PERCENT=13
LOCAL_HEAP_HEADROOM_REPORTED=284796
```

Por lo tanto existe margen de RAM para evaluar un bloque mayor sin volver al framebuffer completo.

### Etapa 2 — candidato 32 filas @ 921600 baud

Se define como siguiente candidato físico:

```text
FRAME_BUFFER_ROWS=32
FRAME_BUFFER_PIXELS=320*32
FRAME_BUFFER_BYTES=20480
SERIAL_BAUD=921600
SERIAL_RX_BUFFER=8192
HOST_TX_CHUNK=1024
HOST_POLL_MS=120
FLOW_CONTROL=FRAME_ACK
```

El último bloque del ST7789 contiene 10 filas porque 170 no es múltiplo de 32.

Objetivo:

```text
BUFFER_1_ROW   -> hasta 170 operaciones de dibujo por frame
BUFFER_16_ROWS -> 11 operaciones de dibujo por frame
BUFFER_32_ROWS -> 6 operaciones de dibujo por frame
```

El regreso a 921600 baud se realiza únicamente después de tener ACK por frame estable. El host no puede enviar el siguiente framebuffer hasta que el bridge confirme que el frame previo terminó de recibirse y dibujarse.

No cambia el contrato de producción ni aumenta la capacidad de campos/objetos del Designer. Es exclusivamente una optimización del transporte de desarrollo LIVE.

## Telemetría física

Cada 30 frames procesados correctamente el bridge emite:

```text
JWHMI_LIVE_STATS
frames=<n>
frame_us_avg=<us>
frame_us_max=<us>
rx_us_avg=<us>
draw_us_avg=<us>
draw_us_max=<us>
free=<bytes>
min=<bytes>
largest=<bytes>
errors=<n>
buffer_rows=<n>
```

Definiciones:

- `frame_us_*`: tiempo total desde recepción del header hasta frame completamente dibujado.
- `draw_us_*`: tiempo acumulado dentro de las operaciones SPI/TFT del frame.
- `rx_us_avg`: aproximación `frame_us - draw_us`; incluye recepción Serial, decodificación RLE y overhead no-TFT.
- `free`: heap libre al emitir la ventana de estadísticas.
- `min`: mínimo heap libre observado por el core desde arranque.
- `largest`: mayor bloque contiguo asignable.
- `errors`: contador acumulado de errores del bridge.

El ACK `JWHMI_LIVE_FRAME <seq>` se emite antes de la telemetría para no retrasar el backpressure del host.

## Gate físico del candidato 32/921600

Prueba recomendada:

```text
DURACION=2-3_min
ACCION=drag_continuo + cambios_XY + colores + valores
VISUAL_CORRUPTION=0
LIVE_ERRORS=0
```

Registrar:

```text
PROGRAM_BYTES
GLOBAL_BYTES
frame_us_avg
frame_us_max
draw_us_avg
draw_us_max
free
min
largest
errors
buffer_rows
```

## Criterio de decisión

Mantener 32 filas + 921600 baud si:

1. no reaparece corrupción visual;
2. `errors=0` durante el stress;
3. heap mínimo mantiene margen holgado;
4. la sensación de arrastre mejora claramente frente a 16 filas @ 500000;
5. no aparecen timeouts de ACK.

Si el resultado es estable pero la mejora sigue siendo limitada, no se seguirá aumentando `g_frame` de forma indefinida. La siguiente optimización prioritaria será transmitir/dibujar sólo regiones modificadas.

## Estado

```text
A11_LIVE_500K_STABILITY=PASS_PHYSICAL
A11_LIVE_FRAME_ACK=PASS_PHYSICAL
A11_LIVE_BUFFER_16_ROWS=PASS_STABILITY_FLUIDITY_REGULAR
A11_LIVE_TELEMETRY=IMPLEMENTED
A11_LIVE_921600_ACK_CANDIDATE=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_BUFFER_32_ROWS=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_DIRTY_REGIONS=DEFERRED_UNTIL_BUFFER32_GATE
SECOND_HMI_RUNTIME=NO
PRODUCTION_CODEGEN_UNCHANGED=YES
```
