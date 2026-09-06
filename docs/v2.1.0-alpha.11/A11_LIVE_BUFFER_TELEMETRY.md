# Alpha11 — LIVE buffer parcial y telemetría

Fecha: 2026-09-05

## Contexto

El primer bridge de Live Preview reservaba un framebuffer RGB565 completo de 320 × 170 píxeles:

```text
320 × 170 × 2 = 108800 bytes
```

Ese enfoque produjo overflow de DRAM en JWPLC Basic v2. La corrección posterior cambió a streaming de una sola fila de 320 píxeles (640 B), eliminó el overflow y, junto con 500000 baud + ACK por frame, dio estabilidad visual física.

## Decisión A11-LIVE-BUFFER

Para evaluar mejor fluidez sin regresar al costo de RAM del framebuffer completo, el bridge usa un framebuffer **parcial de 16 filas**:

```text
FRAME_BUFFER_ROWS=16
FRAME_BUFFER_PIXELS=320*16
FRAME_BUFFER_BYTES=10240
SERIAL_BAUD=500000
SERIAL_RX_BUFFER=8192
HOST_TX_CHUNK=1024
FLOW_CONTROL=FRAME_ACK
```

El último bloque del ST7789 contiene 10 filas porque 170 no es múltiplo de 16.

### Objetivo

Reducir el overhead de dibujo del bridge:

```text
BUFFER_1_ROW   -> hasta 170 operaciones de dibujo por frame
BUFFER_16_ROWS -> 11 operaciones de dibujo por frame
```

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
buffer_rows=16
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

## Gate A/B pendiente

Comparar físicamente el bridge estable previo de una fila contra el nuevo buffer de 16 filas.

Prueba recomendada:

```text
DURACION=2-3_min
ACCION=drag_continuo + cambios_XY + colores + valores
VISUAL_CORRUPTION=0
LIVE_ERRORS=0
```

Registrar al menos dos ventanas `JWHMI_LIVE_STATS` y observar:

```text
frame_us_avg
frame_us_max
draw_us_avg
draw_us_max
free
min
largest
errors
```

## Criterio de decisión

Mantener 16 filas si:

1. no reaparece corrupción visual;
2. `errors=0` durante el stress;
3. heap mínimo mantiene margen holgado;
4. el tiempo de dibujo/total mejora de forma útil o la sensación de arrastre es más fluida.

Si la mejora es marginal, volver al buffer de una fila y priorizar actualización por regiones modificadas como siguiente optimización de LIVE.

## Estado

```text
A11_LIVE_500K_STABILITY=PASS_PHYSICAL
A11_LIVE_FRAME_ACK=PASS_PHYSICAL
A11_LIVE_BUFFER_16_ROWS=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_TELEMETRY=IMPLEMENTED_PENDING_PHYSICAL_GATE
SECOND_HMI_RUNTIME=NO
PRODUCTION_CODEGEN_UNCHANGED=YES
```
