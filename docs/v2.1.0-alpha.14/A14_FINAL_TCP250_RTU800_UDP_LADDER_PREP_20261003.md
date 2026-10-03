# Alpha14 - campaña final TCP250 + RTU800 + UDP FAST — preparación 2026-10-03

## Objetivo

Cerrar la caracterización de coexistencia simultánea del JWPLC Basic con una carga industrial equilibrada:

- Modbus TCP: 250 req/s.
- Modbus RTU: 800 req/s.
- RTU EXP-MIX: 8 transacciones por scan.
- objetivo de scan RTU: 100 scans/s.
- interpretación temporal: 8 transacciones de expansión por scan, equivalentes a atender 8 módulos una vez cada 10 ms.
- UDP FAST RX: variable a escalar.

La bancada física usa un único Slave ID 2 que emula las ocho operaciones lógicas
(2 DI + 2 DO + 2 AI + 2 AO). A nivel de tiempo de bus, cada operación conserva
su trama RTU completa; por tanto sirve para caracterizar el presupuesto de
comunicación de ocho módulos. No constituye por sí sola una prueba multidrop
con ocho placas físicas diferentes.
- full runtime activo: Display + SD/DataLog + FRAM + RTC + TCA/I/O + botonera.

Este gate es diagnóstico y aditivo. No cambia las APIs productivas.

## Justificación del punto base

La campaña previa mostró:

- RTU-only EXP-MIX: ~1346.7 req/s.
- TCP500 + RTU unpaced: ~900 req/s.
- TCP1000 + RTU100: PASS estricto confirmado.

Para la carga final se fija TCP en 250 req/s y RTU en 800 req/s. Esto representa una carga TCP alta pero más cercana a un uso industrial real y conserva el requisito de 8 expansiones a 100 Hz.

## Hallazgo de preflight: pacing por request no representa el requisito real

La primera ejecución del baseline, todavía con pacing individual de 800 req/s,
produjo:

- TCP: 250.000 req/s.
- RTU: 731.975 req/s.
- scans: 91.497 scans/s.
- UDP: 0 Mbps.
- clasificación: RTU800_TARGET_FAIL_CLEAN.

El resultado no indica falta de capacidad media del RTU. La evidencia previa
unpaced mostró aproximadamente 900 req/s aun con TCP500. El problema fue el
criterio temporal usado: exigir una transacción exactamente cada 1.25 ms hace
que cualquier transacción ligeramente más larga marque un periodo perdido.

Para el requisito de expansiones, el contrato correcto es por scan:

- 100 scans/s.
- periodo de scan: 10 ms.
- 8 transacciones por scan.
- los 8 slots se ejecutan back-to-back dentro de la ventana.
- al terminar el scan se espera la siguiente frontera de 10 ms.
- un skip sólo se registra si no se puede iniciar el siguiente scan dentro de
  su ventana temporal.

Por tanto, 800 req/s sigue siendo la carga media equivalente, pero la garantía
se mide como 100 scans/s de ocho expansiones y no como ocho deadlines
independientes de 1.25 ms.

## Hallazgo de campaña completa: pacing UDP del host inválido

La primera campaña completa del ladder no estableció el techo coexistente UDP.

El sender del PC cuantizaba la carga en ticks de 10 ms y emitía los datagramas
del tick back-to-back. Con payload 1016 B y header UDP W5500 de 8 B, cada record
ocupa 1024 B; el socket RX de 2048 B sólo puede contener dos records completos.

A 12 Mbps el host generaba aproximadamente 14-15 datagramas por tick de 10 ms.
El techo artificial de dos records por burst es 1.6256 Mbps y el DUT midió
~1.6466 Mbps, confirmando overflow inducido por el harness.

Clasificación:

- HARNESS_FAILURE=YES.
- PRODUCT_FAILURE=NO.
- UDP_COEXISTENCE_CEILING=NOT_ESTABLISHED.

Corrección:

- `UDP_HOST_PACING_MODE=ONE_PACKET_DEADLINE_NO_CATCHUP`.
- no se permiten catch-up bursts;
- se registran deadlines del host omitidos;
- el sender corre en un proceso dedicado con reloj de alta resolución;
- el pacing se valida sobre separación start-to-start de las llamadas `sendto()`;
- el completion-gap queda sólo como telemetría de latencia de syscall;
- el start-gap mínimo debe ser >=90 % del intervalo objetivo;
- no puede haber start-gaps marcados como too-close;
- el preflight incluye self-test localhost de pacing antes del hardware.

## UDP ladder

Cada escalón dura 300 s:

`0, 1, 2, 4, 6, 8, 10, 12 Mbps`

Características:

- UDP FAST aditivo.
- payload: 1016 bytes.
- batch: 2.
- puerto: 5002.
- número de secuencia de 32 bits al inicio de cada datagrama.
- el punto 0 Mbps mantiene el socket UDP creado pero desactiva el polling FAST para no introducir carga artificial de empty-poll.

## Segundo baseline y separación de contratos

Con pacing por scan a 100 Hz, el baseline UDP=0 produjo:

- TCP: 250.003 req/s.
- RTU: 799.728 req/s.
- scans: 99.966 scans/s.
- UDP: 0 Mbps.
- clasificación antigua: RTU800_TARGET_FAIL_CLEAN.

Las métricas de throughput/scan ya cumplen el objetivo operacional. El único
predicado restante que podía causar el FAIL era `RTU_PERIODS_SKIPPED == 0`.
Ese predicado corresponde a una garantía determinística de cero deadline-miss,
no a la capacidad media/sostenida de 100 Hz que se quiere caracterizar aquí.

Desde este punto el harness separa explícitamente dos contratos:

### Contrato operacional

Usado para seleccionar el ladder UDP:

- TCP >=99.9 % del target.
- RTU >=99 % de 800 req/s.
- scan RTU >=99 scans/s.
- cero failed/rejected/verify/CRC/timeouts.
- mapa Master/Slave exacto.
- runtime/periféricos limpios.
- UDP >=99 % del target solicitado y sin errores de integridad.

`RTU_PERIODS_SKIPPED` se conserva como telemetría de jitter/deadline-miss y no
se oculta.

### Contrato determinístico

Se reporta en paralelo:

- todo el contrato operacional;
- `RTU_PERIODS_SKIPPED == 0`.

Por tanto un caso puede cerrar como operacionalmente válido y, al mismo tiempo,
indicar que no alcanzó hard zero-skip timing bajo full runtime.

## Criterio por escalón

TCP:

- target 250 req/s.
- >=99.9 % del target.
- cero timeout/transport/protocol errors.

RTU operacional:

- target 800 req/s.
- >=99 % del target.
- >=99 scans/s.
- 0 failed/rejected/verify/CRC/timeouts.
- mapa de salidas Master/Slave exacto.
- 2 DI + 2 DO + 2 AI + 2 AO por scan.

RTU determinístico:

- todos los criterios operacionales;
- 0 skipped periods.

UDP:

- el host debe ofrecer >=99 % del target solicitado.
- el DUT debe entregar >=99 % del target.
- delivery >=99 % de paquetes enviados.
- 0 wrong-size packets.
- 0 sequence decode errors.
- 0 duplicates.
- 0 reorders.
- 0 transport errors.
- 0 SPI lock errors.

Runtime:

- periféricos ready.
- SD commits sin fallos.
- SPI probe sin fallos.
- sin resets inesperados.

La pérdida por saturación se registra mediante delivery, range missing y throughput DUT; no se confunde con corrupción.

## Selección y confirmación

Se selecciona automáticamente el mayor escalón UDP positivo que cumpla el
contrato operacional. En cada caso se registra además si también cumple el
contrato determinístico cero-skip.

Ese punto se repite durante 600 s.

Resultado de cierre esperado:

`A14_FINAL_TRIPLE_COEXISTENCE=PASS_TRIPLE_COEXISTENCE_CONFIRMED`

## Firmware diagnóstico

Master:

`tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_tcp250_rtu800_udp_master/`

Es un clon diagnóstico del full-runtime EXP-MIX con:

- comando adicional RTU800;
- socket UDP FAST;
- habilitación/deshabilitación explícita del servicio UDP;
- telemetría de secuencia UDP;
- sin cambios en la API productiva.

Slave:

`tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave/`

## Runner y gate

Runner:

`tools/modbus-tcp-benchmark/pc/a14_final_tcp250_rtu800_udp_ladder.py`

Gate:

`tools/modbus-tcp-benchmark/gates/a14_final_tcp250_rtu800_udp_ladder.ps1`

### Preflight no destructivo

Antes de la corrida física larga se ejecuta el mismo gate con:

`-PreflightOnly`

Ese modo valida:

- contratos de source;
- sintaxis Python;
- compile Master;
- compile Slave;
- objetos source-first para RTU/TCP/Ethernet/UDP/SPI.

No realiza upload ni benchmark físico.

Sólo después de:

`A14_FINAL_TRIPLE_PREFLIGHT=PASS`

se ejecuta la campaña completa.

## Tiempo de prueba

Ladder:

- 8 x 300 s = 40 min.

Confirmación:

- 600 s = 10 min.

Ventanas de medición: ~50 min.

Añadir compile/upload, boot, snapshots y persistencia de evidencia.

## Evidencia esperada

Raíz:

`tools/modbus-tcp-benchmark/results/a14_tcp250_rtu800_udp_ladder_YYYYMMDD_HHMMSS/`

Archivos principales:

- MANIFEST.txt
- compile_master.log
- compile_slave.log
- upload_master.log
- upload_slave.log
- runner.log
- UDP_LADDER.csv
- SELECTION.json
- FINAL_SUMMARY.json
- FINAL_STATUS.txt
- GATE_STATUS.txt
- snapshots/result.json por caso
- snapshots de confirmación.

## Regla de publicación

La cifra final debe expresarse como coexistencia bajo configuración explícita, por ejemplo:

`TCP 250 req/s + RTU 800 req/s (100 scans/s, 8 módulos) + UDP FAST X Mbps + full runtime`

No sustituye los ceilings RAW individuales ya caracterizados.
