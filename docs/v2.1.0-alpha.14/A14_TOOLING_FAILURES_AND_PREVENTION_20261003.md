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


## F082 — Python thread/time.sleep pacing insufficient for sub-2ms UDP slots

### Síntoma

El preflight de pacing corrigió el burst de 10 ms, pero el sender todavía
ejecutado como thread Python con `time.sleep()` no sostuvo los targets altos:

- 4 Mbps: ~98.41 %.
- 6 Mbps: ~70.60 %.
- 8 Mbps: ~87.83 %.
- 10 Mbps: ~70.69 %.
- 12 Mbps: ~67.68 %.

También reaparecieron agrupaciones temporales en los targets altos.

### Causa

Con payload 1016 B, los intervalos objetivo son aproximadamente:

- 4 Mbps: 2.032 ms.
- 6 Mbps: 1.355 ms.
- 8 Mbps: 1.016 ms.
- 10 Mbps: 0.813 ms.
- 12 Mbps: 0.677 ms.

Un thread Python que comparte proceso/GIL y depende de `time.sleep()` no ofrece
precisión suficiente y estable en esas ventanas bajo Windows.

### Clasificación

- HARNESS_FAILURE=YES.
- PRODUCT_FAILURE=NO.
- HARDWARE_FAILURE=NO.
- PHYSICAL_RUN_STARTED=NO.

### Corrección

1. Mover el sender UDP a un proceso dedicado mediante `multiprocessing spawn`.
2. Usar `perf_counter_ns()`.
3. Usar espera híbrida sleep + spin.
4. Para ventanas <=~2.5 ms, priorizar spin de alta resolución.
5. Programar el siguiente slot desde el inicio real del envío.
6. Si el envío invade el siguiente slot, descartar ese slot; nunca catch-up.
7. Validar spacing relativo al intervalo objetivo:
   - `MIN_GAP >= 90 %` del intervalo objetivo.
   - `TOO_CLOSE_PACKETS == 0`.
8. Mantener target ofrecido >=98.5 % en self-test y >=99 % en campaña física.

### Regla preventiva

No usar un thread Python con sleep periódico como generador de tráfico de
validación para intervalos sub-2 ms sin un self-test previo del host real.

El preflight debe demostrar pacing suficiente antes del upload.

### Estado

- F082=CONFIRMED_AND_CORRECTED_IN_HARNESS.
- PRODUCT_SOURCE_MUTATION=NO.
- NEXT_STEP=HOST_PACING_PREFLIGHT_ONLY.


## F083 — pacing guard medía retorno de sendto en vez de inicio de envío

### Síntoma

El sender dedicado sostuvo prácticamente el rate completo:

- 4 Mbps: 99.915 %.
- 6 Mbps: 99.881 %.
- 8 Mbps: 99.941 %.
- 10 Mbps: 99.929 %.
- 12 Mbps: 99.885 %.

Sin embargo el preflight falló por mínimos de separación calculados entre los
timestamps tomados después de que `sendto()` retornaba.

### Causa

El scheduler ya mantenía el siguiente deadline desde `send_begin_ns`, es decir,
desde el inicio real de cada llamada de envío. Pero el guard calculaba spacing
entre `send_ns`, tomado después del retorno de `sendto()`.

Como la duración de `sendto()` varía, dos retornos consecutivos pueden quedar
más cerca aunque los inicios de ambas llamadas hayan conservado correctamente
el intervalo objetivo.

Por tanto:

- `send-start gap` representa el pacing generado por el harness;
- `send-completion gap` mezcla pacing + duración variable de la syscall.

### Clasificación

- HARNESS_FAILURE=YES.
- PRODUCT_FAILURE=NO.
- HARDWARE_FAILURE=NO.
- PHYSICAL_RUN_STARTED=NO.

### Corrección

1. medir y validar `send_begin_ns[n] - send_begin_ns[n-1]`;
2. conservar completion-gap como telemetría diagnóstica;
3. registrar `send_call_max_us`;
4. source PASS usa:
   - offered rate;
   - mínimo start-gap relativo al target;
   - cero start-gap too-close;
   - cero send errors;
5. completion-gap nunca decide por sí solo el PASS del source.

### Prevención

Para generadores de tráfico, validar el instante que representa realmente la
política de pacing. No usar el retorno de una syscall como proxy del instante de
emisión cuando la latencia de la syscall forma parte de la medición.

### Estado

- F083=CONFIRMED_AND_CORRECTED_IN_HARNESS.
- PRODUCT_SOURCE_MUTATION=NO.
- NEXT_STEP=HOST_PACING_PREFLIGHT_ONLY.


## Recurrencia F080 — confirmation miss volvió a etiquetarse como runner failure

La campaña válida seleccionó UDP 2 Mbps tras PASS operacional de 300 s, pero la
confirmación de 600 s cerró con:

- TCP: 250.001 req/s.
- RTU: 788.239 req/s.
- scans: 98.530/s.
- UDP DUT: 1.996 Mbps.
- UDP delivery: 99.988 %.
- runtime clean: YES.
- operational pass: NO por RTU <99 %.

Ese resultado es una caracterización válida, no un fallo de ejecución. Sin
embargo el runner devolvió exit code 2 para `REVIEW_CONFIRMATION_FAILED` y el
gate volvió a reportar `TRIPLE_RUNNER_FAILED`.

### Corrección F080 reforzada

1. Todo outcome interpretable de criterio devuelve exit code 0.
2. `TRIPLE_RUNNER_FAILED` queda reservado para fallo real del runner/tooling.
3. Estados estructurados nuevos:
   - `CHARACTERIZED_CONFIRMATION_NOT_OPERATIONAL`.
   - `CHARACTERIZED_CONFIRMATION_ONLY_NOT_OPERATIONAL`.
   - `PASS_CONFIRMATION_ONLY`.
4. El gate reconoce explícitamente esos estados y los persiste en
   `GATE_STATUS.txt`.

## F084 — confirmación fallida obligaba a repetir todo el ladder

### Síntoma

Después de invertir 8 x 300 s en el ladder, el candidato 2 Mbps falló sólo en
su confirmación de 600 s. El harness no ofrecía una ruta para confirmar el
siguiente punto ya caracterizado (1 Mbps) sin repetir toda la campaña.

### Causa

El runner tenía un único flujo:

`ladder completo -> seleccionar máximo -> una confirmación -> terminar`.

No existía modo reanudable de confirmación.

### Corrección

Se añade:

- runner: `--confirm-only-mbps X`;
- gate: `-ConfirmOnlyMbps X`.

Ese modo mantiene exactamente:

- TCP250;
- RTU800 scan-paced;
- full runtime;
- UDP FAST;
- host pacing validado;
- duración de confirmación >=600 s;

pero omite el ladder ya cerrado.

### Prevención F084

Después de un ladder costoso, los pasos de confirmación deben ser reanudables.
Un candidato que falla confirmación no debe forzar repetir evidencia válida.

### Estado

- F080_RECURRENCE=CORRECTED.
- F084=CONFIRMED_AND_CORRECTED.
- PRODUCT_SOURCE_MUTATION=NO.
- NEXT_GATE=CONFIRM_ONLY_UDP1_MBPS_600S.
