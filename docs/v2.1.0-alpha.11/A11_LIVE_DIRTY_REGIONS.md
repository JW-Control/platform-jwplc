# Alpha11 — LIVE por eventos y Dirty Regions

Fecha: 2026-09-05

## Contexto

Las pruebas físicas anteriores cerraron dos problemas distintos del Live Preview:

- el framebuffer completo de 320×170 en DRAM no es viable para JWPLC Basic v2;
- el envío sin backpressure podía producir corrupción visual.

La configuración `500000 baud + ACK + buffer parcial de 16 filas` fue físicamente estable, pero la fluidez se percibió regular. Posteriormente se probó `921600 baud + buffer parcial de 32 filas + ACK`; la estabilidad se mantuvo, pero con cinco objetos no se observó una mejora perceptible suficiente.

Esto indica que aumentar baudrate o `g_frame` ya no ataca el cuello principal: el host seguía esperando un polling de 120 ms y retransmitiendo el framebuffer completo ante cambios pequeños.

## Decisión A11-LIVE-DIRTY

Se introduce un transporte incremental de desarrollo:

```text
EVENT_DRIVEN=YES
POLL_120MS_PRIMARY_SCHEDULER=REMOVED
FALLBACK_SYNC_MS=1000
SERIAL_BAUD=921600
SERIAL_RX_BUFFER=8192
HOST_TX_CHUNK=1024
FRAME_BUFFER_ROWS=32
FLOW_CONTROL=FRAME_ACK
LATEST_STATE_COALESCING=YES
```

El evento principal es:

```text
jwplc:editor-refresh
```

El Designer solicita sincronización inmediatamente después de refrescar su framebuffer. Si ya existe una imagen esperando ACK, no encola estados intermedios: conserva únicamente que existe un estado más reciente pendiente y lo evalúa después del ACK.

## Baseline confirmado

El host conserva una copia lógica RGB565 de la última imagen confirmada por el JWPLC:

```text
LAST_ACK_FRAME=BASELINE
```

Una nueva imagen se compara contra ese baseline. Sólo después de recibir:

```text
JWHMI_LIVE_FRAME <seq>
```

el host avanza el baseline al estado enviado.

Esta regla permite que una región incluya correctamente tanto los píxeles que deben borrarse de la posición anterior como los píxeles nuevos.

## Protocolos

### FULL — `JWH1`

Se conserva el protocolo de frame completo para conexión inicial, resync y cambios grandes:

```text
magic      4 B  JWH1
sequence   u32
runCount   u32
width      u16
height     u16
runs       N × {count:u16, color:u16}
```

Para Alpha11:

```text
width=320
height=170
```

### REGION — `JWH2`

Se agrega una imagen rectangular parcial:

```text
magic      4 B  JWH2
sequence   u32
runCount   u32
x          u16
y          u16
width      u16
height     u16
runs       N × {count:u16, color:u16}
```

Los runs representan la región en orden row-major.

El bridge valida que:

```text
width>0
height>0
x+width<=320
y+height<=170
runCount>0
runCount<=width*height
DECODED_PIXELS=width*height
```

## Selección FULL / REGION

El Designer calcula el bounding rectangle de todos los píxeles diferentes respecto al último frame confirmado.

```text
FIRST_FRAME        -> FULL
RESYNC_AFTER_ERROR -> FULL
DIRTY_AREA>=65%    -> FULL
DIRTY_AREA<65%     -> REGION
NO_PIXEL_CHANGE    -> NO_TX
```

El umbral de 65 % queda como valor inicial de Alpha11 y puede ajustarse con telemetría física.

## Buffer parcial del bridge

No se incrementa `g_frame` por encima de 32 filas:

```text
MAX_BUFFER=320*32*2=20480_bytes
```

Para REGION el mismo buffer se reutiliza con el ancho real de la región. El bridge dibuja bloques de hasta 32 filas mediante:

```text
jwplcSPI_acquire()
jwplcSPI_prepareForTFT()
drawRGBBitmap(x,y,...)
jwplcSPI_release()
```

El acceso directo a TFT pertenece únicamente al bridge de desarrollo. El codegen de producción continúa usando la API pública JWPLC_UI.

## Diagnóstico integrado

Durante LIVE, Web Serial posee el puerto COM. Arduino Serial Monitor no puede abrir el mismo puerto simultáneamente.

Por ello se habilita la pestaña inferior:

```text
Diagnóstico
```

como Monitor LIVE integrado.

Presenta:

```text
baud
FPS efectivo
latencia del último ACK
bytes del último TX
modo FULL / REGION
rectángulo enviado
conteo FULL
conteo REGION
heap libre
heap mínimo
mayor bloque contiguo
errores
log TX/RX
```

Incluye acciones:

```text
Pausar log
Limpiar
Copiar
```

Las líneas `JWHMI_LIVE_STATS` emitidas por el bridge alimentan memoria y errores dentro de esta misma vista.

## Telemetría del bridge

Cada 30 imágenes correctas:

```text
JWHMI_LIVE_STATS
frames=<n>
frame_us_avg=<us>
frame_us_max=<us>
rx_us_avg=<us>
draw_us_avg=<us>
draw_us_max=<us>
full=<n>
region=<n>
region_px_avg=<pixels>
free=<bytes>
min=<bytes>
largest=<bytes>
errors=<n>
buffer_rows=32
```

## Gate físico

Prueba recomendada:

```text
DURACION=2-3_min
ESCENA=5_o_mas_objetos
ACCION=drag_continuo + cambios_XY + VALUE + colores
```

Validar:

```text
VISUAL_CORRUPTION=0
LIVE_ERRORS=0
ACK_TIMEOUTS=0
REGION_FRAMES>FULL_FRAMES_durante_drag
REGION_TX_BYTES<<FULL_TX_BYTES
RESPONSIVENESS=MEJORA_PERCEPTIBLE
```

Tomar captura de la pestaña Diagnóstico después del stress para registrar bytes, ACK, FPS y memoria.

## Estado

```text
A11_LIVE_EVENT_DRIVEN=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_DIRTY_REGION_JWH2=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_LATEST_STATE_COALESCING=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_DIAGNOSTIC_PANEL=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_LIVE_FULL_JWH1_COMPATIBILITY=PRESERVED
A11_LIVE_BAUD=921600
A11_LIVE_BUFFER_ROWS=32
SECOND_HMI_RUNTIME=NO
PRODUCTION_CODEGEN_UNCHANGED=YES
```
