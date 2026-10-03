# Alpha14 - fallos de tooling y prevención — 2026-10-03

Este documento registra los incidentes observados durante la preparación de la
campaña final TCP250 + RTU800 + UDP FAST y complementa
`JWPLC_ASSISTANT_FAILURES_AND_PREVENTION`.

## F077 — Arduino autoprototype dependency not preflighted

### Síntoma

El sketch diagnóstico falló al compilar por prototipos automáticos generados
antes de que existieran tipos/constantes usados en las firmas:

- `SD_RECORD_BYTES`.
- `RtuMixSlot`.

### Clasificación

- HARNESS_FAILURE=YES
- PRODUCT_FAILURE=NO
- HARDWARE_FAILURE=NO

### Prevención

1. Todo sketch diagnóstico nuevo debe compilarse con Arduino CLI antes de
   cualquier upload/prueba física.
2. Evitar tipos personalizados en firmas sensibles al autoprototipado Arduino o
   declarar prototipos explícitos en una posición segura.
3. El gate debe mostrar automáticamente el tail del compile log.
4. La compilación debe formar parte del preflight no destructivo.

## F078 — requisito industrial traducido a contrato temporal incorrecto

### Síntoma

`8 módulos @100 Hz` se tradujo inicialmente a 800 deadlines independientes,
uno cada 1.25 ms.

Eso produjo aproximadamente:

- TCP=250 req/s.
- RTU=731.975 req/s.
- scans=91.497/s.

aunque campañas previas habían demostrado ~900 RTU req/s incluso con TCP500
cuando el RTU operaba unpaced.

### Causa

Se confundió:

- carga media: 800 transacciones/s;
- requisito de scan: 8 transacciones dentro de una ventana de 10 ms;
- hard real-time: cero deadline miss individual.

### Prevención

1. Definir primero el requisito físico.
2. Traducirlo explícitamente a scheduler/periodo.
3. Separar throughput, frecuencia de scan y determinismo.
4. No introducir un criterio hard-real-time si el producto no lo exige.

## F079 — FAIL sin exponer el predicado decisivo

### Síntoma

El runner imprimió:

- TCP.
- RTU.
- scans.
- clasificación.

pero no imprimió `RTU_PERIODS_SKIPPED`, aun cuando `skipped == 0` era parte
del PASS.

### Prevención

Todo `CASE_END` debe imprimir las métricas y booleanos que participan
directamente en la clasificación:

- target percentages;
- skips;
- runtime clean;
- TCP pass;
- RTU operational pass;
- RTU deterministic pass;
- UDP pass;
- operational pass;
- deterministic pass.

## F080 — criterio no cumplido escalado como runner failure

### Síntoma

Un resultado válido de benchmark terminó como:

`TRIPLE_RUNNER_FAILED`

porque el runner lanzó excepción al no cumplir el baseline.

### Causa

Se mezcló:

- ejecución inválida/harness roto;
- producto con error;
- resultado válido que no cumple un contrato.

### Prevención

Los resultados esperables deben cerrar con estados estructurados, no con
excepciones:

- `PASS_TRIPLE_COEXISTENCE_CONFIRMED`.
- `CHARACTERIZED_BASELINE_NOT_OPERATIONAL`.
- `CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL`.
- `REVIEW_CONFIRMATION_FAILED`.

Una excepción se reserva para problemas de tooling, entorno, perfil o producto
que impiden interpretar la medición.

## Separación final de contratos RTU

### Operacional

Usado para la selección UDP:

- RTU >=99 % de 800 req/s.
- scan >=99 Hz.
- cero failed/rejected/verify/CRC/timeouts.
- runtime limpio.
- `RTU_PERIODS_SKIPPED` se registra como jitter.

### Determinístico

Se reporta en paralelo:

- todo lo operacional;
- `RTU_PERIODS_SKIPPED == 0`.

Esto evita afirmar hard real-time cuando sólo se ha demostrado rendimiento
industrial sostenido, sin esconder los deadline misses.

## Nueva regla de preflight

Antes de otra corrida larga:

`PATCH -> RE-READ -> SYNTAX -> COMPILE -> SOURCE-FIRST -> PREFLIGHT PASS -> PHYSICAL RUN`

El gate final dispone de `-PreflightOnly` para ejecutar todas las validaciones
no destructivas sin upload ni benchmark físico.


## F081 — UDP host pacing quantized in 10 ms bursts overflowed the 2 KB RX ring

### Síntoma

La primera campaña completa TCP250 + RTU800 + UDP FAST produjo:

- UDP target 1 Mbps -> DUT ~0.864 Mbps.
- UDP target 2 Mbps -> DUT ~1.036 Mbps.
- UDP target 4 Mbps -> DUT ~1.038 Mbps.
- UDP target 12 Mbps -> DUT ~1.647 Mbps.

Al mismo tiempo:

- TCP permaneció en ~250 req/s.
- RTU permaneció operacional hasta 10 Mbps ofrecidos.
- UDP wrong-size/decode/duplicate/reorder = 0.
- UDP transport errors = 0.
- SPI lock errors = 0.
- runtime/periféricos = clean.

### Causa confirmada

El sender del PC usaba un tick de 10 ms y acumulaba el número de datagramas
correspondiente a ese tick, emitiéndolos back-to-back.

Con:

- payload UDP = 1016 bytes;
- header W5500 UDP = 8 bytes;
- record RX = 1024 bytes;
- RX socket W5500 = 2048 bytes;

sólo caben dos records completos simultáneamente en el RX ring.

A 12 Mbps:

- ~1476 datagramas/s;
- ~14.76 datagramas por tick de 10 ms;
- el sender generaba ráfagas de ~14-15;
- el W5500 sólo podía almacenar 2 antes del drenaje.

El techo artificial impuesto por ese patrón es:

`2 * 1016 * 8 / 0.010 = 1.6256 Mbps`

La medición real fue ~1.6466 Mbps, confirmando la firma del overflow por burst.
Por tanto, esa campaña no mide el techo coexistente UDP del producto.

### Clasificación

- HARNESS_FAILURE=YES
- PRODUCT_FAILURE=NO
- HARDWARE_FAILURE=NO
- UDP_COEXISTENCE_CEILING=NOT_ESTABLISHED

### Corrección

El sender se cambia a:

`UDP_HOST_PACING_MODE=ONE_PACKET_DEADLINE_NO_CATCHUP`

Reglas:

1. un datagrama por deadline;
2. no emitir catch-up bursts después de jitter del host;
3. si un deadline está demasiado atrasado, omitir ese slot del host;
4. registrar host pacing skips;
5. registrar máximo de paquetes emitidos en cualquier ventana de 1 ms;
6. rechazar como source válido un host que genere >2 paquetes/1 ms;
7. exigir >=99 % del target ofrecido en la campaña física.

### Prevención automatizada

El preflight ejecuta un self-test localhost para 1/2/4/6/8/10/12 Mbps y exige:

- pacing mode correcto;
- >=98.5 % del target en el self-test corto;
- máximo 2 paquetes/1 ms;
- cero send errors.

Sólo después se permite upload y medición física.
