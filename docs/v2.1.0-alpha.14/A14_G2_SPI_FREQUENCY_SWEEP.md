# Alpha14 — G2: sweep de frecuencia SPI Ethernet

## Objetivo

Evaluar el efecto de aumentar únicamente la frecuencia SPI efectiva del backend Ethernet/W5500, manteniendo congelados el firmware raw, el runner raw y el algoritmo de transporte.

Frecuencias inicialmente planificadas: **14, 20, 24, 26 y 30 MHz**. No se incluye 40 MHz en este sweep.

El sweep se cerró experimentalmente en **26 MHz** al confirmarse una meseta de rendimiento entre 20, 24 y 26 MHz. **30 MHz no se ejecuta en G2**: queda registrado como `NOT_TESTED_AFTER_PLATEAU_DECISION`, no como fallo.

## Invariantes

- Firmware raw SHA256: `E77A53C1503401BD51AEFDA969C2D287DE1FB9DD349675EC7B0FD355229BF356`
- Runner raw SHA256: `FD25997CFE777B4FF50BBC10EFE555BD6B7A8A8DCE9D4ADD1E7D2DBA6D52470F`
- `core.a` SHA256: `6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68`
- `libJW_SD.a` SHA256: `E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1`
- TCP RX: 4 chunks por ownership window, máximo 1024 bytes por chunk.
- Presupuesto interno `TCP_SPI_HOLD_MAX_US <= 5000`.
- Cero diagnóstico visual `SPI` permitido.
- UDP RX: `PC_MBPS` representa tráfico ofrecido; la métrica de rendimiento primaria es `DUT_MBPS`.

## Baseline congelado — 14 MHz

Fuente: D40-R2, 12/12 PASS, cero eventos visuales `SPI`.

| Modo | Métrica primaria | Promedio 14 MHz | Hold máximo observado |
|---|---:|---:|---:|
| TCP_RX | PC Mbps | 9.755605 | 4042 us |
| TCP_TX | PC Mbps | 3.706707 | 2703 us |
| UDP_RX | DUT Mbps | 8.132316 | — |
| UDP_TX | PC Mbps | 4.163015 | 213 us |

> En UDP RX el valor de la PC corresponde al flood ofrecido y no se interpreta como throughput del W5500.

## 20 MHz

### G2-A — cambio estático

**PASS**.

- Frecuencia efectiva: 20 MHz.
- Único archivo tracked modificado: `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h`.
- Diff: 1 inserción / 1 eliminación.
- Hashes protegidos intactos.

### G2-B — smoke físico

**PASS** con snapshots exactos del runner congelado.

- Compile: PASS.
- Upload: PASS.
- READY / link / IP: PASS.
- TCP_RX / TCP_TX / UDP_RX / UDP_TX: PASS.
- Errores de transporte: 0.
- Errores de ownership SPI TCP/UDP: 0.
- Eventos visuales `SPI`: 0.

#### Incidente de atribución temporal

Un smoke previo reportó `TCP_SPI_HOLD_MAX_US=13206` en un snapshot serial tomado después de finalizar TCP_RX. El firmware continúa ejecutando `serviceTcp()` y actualizando telemetría incluso en IDLE, por lo que ese valor no podía atribuirse de forma válida a la fase TCP_RX.

Se implementó un bridge que captura el snapshot exacto utilizado internamente por el runner. Dos diagnósticos consecutivos de 12 corridas TCP_RX cada uno dieron:

- 24/24 corridas sin exceder 5000 us.
- peor hold exacto: **3251 us**.
- eventos visuales `SPI`: **0**.

Conclusión: no se estableció un fallo de producto a 20 MHz; el rechazo anterior fue una ambigüedad temporal del harness.

### G2-C — calificación 3 × 10 s por modo

**PASS**.

| Modo | Métrica | Baseline 14 | Mín. 20 | Prom. 20 | Máx. 20 | Ganancia prom. | Hold máx. | Resultado |
|---|---|---:|---:|---:|---:|---:|---:|---|
| TCP_RX | PC Mbps | 9.756 | 12.244 | 12.860 | 13.188 | +31.8 % | 3193 us | PASS |
| TCP_TX | PC Mbps | 3.707 | 4.676 | 4.685 | 4.692 | +26.4 % | 2463 us | PASS |
| UDP_RX | DUT Mbps | 8.132 | 10.508 | 10.692 | 10.792 | +31.5 % | 202 us | PASS |
| UDP_TX | PC Mbps | 4.163 | 5.102 | 5.104 | 5.108 | +22.6 % | 184 us | PASS |

Las 12 corridas funcionales dieron PASS, sin errores de transporte, sin errores de lock SPI y sin eventos visuales `SPI`.

### Decisión 20 MHz

`G2_20MHZ=PASS`

20 MHz queda aceptado dentro del sweep. Es el primer punto donde aparece prácticamente toda la mejora de rendimiento observada frente al baseline de 14 MHz.

## 24 MHz

### G2-A — cambio estático

**PASS**.

- Transición controlada desde 20 MHz.
- Se validó primero el único dirty esperado en `w5100.h` con diff `1/1`.
- Se restauró únicamente la variación experimental al baseline de 14 MHz.
- Se aplicó 24 MHz como única variación.
- Frecuencia efectiva: 24 MHz.
- Diff final: 1 inserción / 1 eliminación.
- Hashes protegidos intactos.

### G2-B — smoke físico exacto

**PASS** en dos corridas consecutivas.

| Modo | Smoke 1 | Smoke 2 | Hold máx. peor observado |
|---|---:|---:|---:|
| TCP_RX | 13.186824 Mbps | 13.269746 Mbps | 3125 us |
| TCP_TX | 4.661697 Mbps | 4.600297 Mbps | 2337 us |
| UDP_RX DUT | 10.252936 Mbps | 10.328552 Mbps | 537 us |
| UDP_TX | 5.032605 Mbps | 5.033194 Mbps | 157 us |

En ambas corridas:

- compile/upload: PASS;
- READY / link / IP: PASS;
- errores de transporte: 0;
- errores de ownership SPI TCP/UDP: 0;
- errores UDP begin/write/end: 0;
- integridad de secuencia UDP_TX: PASS;
- eventos visuales `SPI`: 0.

### G2-C — calificación 3 × 10 s por modo

**PASS**.

| Modo | Métrica | Baseline 14 | Mín. 24 | Prom. 24 | Máx. 24 | Ganancia prom. | Hold máx. | Resultado |
|---|---|---:|---:|---:|---:|---:|---:|---|
| TCP_RX | PC Mbps | 9.756 | 13.049 | 13.119 | 13.192 | +34.5 % | 3146 us | PASS |
| TCP_TX | PC Mbps | 3.707 | 4.621 | 4.638 | 4.651 | +25.1 % | 2477 us | PASS |
| UDP_RX | DUT Mbps | 8.132 | 10.357 | 10.495 | 10.613 | +29.1 % | 204 us | PASS |
| UDP_TX | PC Mbps | 4.163 | 5.035 | 5.036 | 5.037 | +21.0 % | 221 us | PASS |

### Decisión 24 MHz

`G2_24MHZ=PASS`

24 MHz queda aceptado funcionalmente. Mejora TCP_RX sólo ~2 % respecto a 20 MHz, mientras TCP_TX, UDP_RX y UDP_TX quedan ligeramente por debajo de 20 MHz.

## 26 MHz

### G2-A — cambio estático

**PASS**.

- Transición controlada desde 24 MHz.
- Único dirty esperado: `w5100.h`.
- Diff de origen y destino: `1/1`.
- Restauración intermedia al baseline de 14 MHz: PASS.
- Frecuencia efectiva final: 26 MHz.
- Hashes protegidos intactos.

### G2-B — smoke físico exacto

**PASS**.

| Modo | Smoke 26 MHz | Hold máx. |
|---|---:|---:|
| TCP_RX | 13.144489 Mbps | 2964 us |
| TCP_TX | 4.656146 Mbps | 2394 us |
| UDP_RX DUT | 10.338475 Mbps | 197 us |
| UDP_TX | 5.034235 Mbps | 192 us |

- compile/upload: PASS;
- errores de transporte: 0;
- errores de ownership SPI TCP/UDP: 0;
- errores UDP begin/write/end: 0;
- integridad UDP_TX: PASS;
- eventos visuales `SPI`: 0.

### G2-C — calificación 3 × 10 s por modo

**PASS**.

| Modo | Métrica | Baseline 14 | Mín. 26 | Prom. 26 | Máx. 26 | Ganancia prom. | Hold máx. | Resultado |
|---|---|---:|---:|---:|---:|---:|---:|---|
| TCP_RX | PC Mbps | 9.756 | 13.065 | 13.099 | 13.135 | +34.3 % | 3252 us | PASS |
| TCP_TX | PC Mbps | 3.707 | 4.569 | 4.604 | 4.625 | +24.2 % | 2496 us | PASS |
| UDP_RX | DUT Mbps | 8.132 | 10.255 | 10.424 | 10.693 | +28.2 % | 223 us | PASS |
| UDP_TX | PC Mbps | 4.163 | 5.034 | 5.035 | 5.036 | +20.9 % | 182 us | PASS |

Las 12 corridas funcionales dieron PASS, sin errores de transporte, sin errores de lock SPI y sin eventos visuales `SPI`.

### Decisión 26 MHz

`G2_26MHZ=PASS`

26 MHz confirma la meseta observada desde 20 MHz. Frente a 24 MHz, TCP_RX cambia aproximadamente `-0.15 %`, TCP_TX `-0.73 %`, UDP_RX `-0.68 %` y UDP_TX `-0.02 %`. Estas diferencias se consideran ruido/meseta y no una mejora por frecuencia.

## Comparación consolidada G2

| Modo | 14 MHz | 20 MHz | 24 MHz | 26 MHz | Mejor promedio observado |
|---|---:|---:|---:|---:|---:|
| TCP_RX | 9.756 | 12.860 | **13.119** | 13.099 | 24 MHz |
| TCP_TX | 3.707 | **4.685** | 4.638 | 4.604 | 20 MHz |
| UDP_RX DUT | 8.132 | **10.692** | 10.495 | 10.424 | 20 MHz |
| UDP_TX | 4.163 | **5.104** | 5.036 | 5.035 | 20 MHz |

Lectura:

- `14 → 20 MHz` produce el salto grande: aproximadamente +22.6 % a +31.8 % según el modo.
- `20 → 24 MHz` sólo mejora TCP_RX ~2 %, mientras los otros tres modos retroceden ~1–2 %.
- `24 → 26 MHz` no produce mejora material en ningún modo.
- El cuello dominante deja de parecer la frecuencia SPI pura con el burst actual de 4 chunks.

## Cierre G2

`G2_FREQUENCY_SWEEP=CLOSED_PLATEAU_AT_26MHZ`

`G2_30MHZ=NOT_TESTED_AFTER_PLATEAU_DECISION`

La decisión de no ejecutar 30 MHz es deliberada: después de tres puntos consecutivos de 20, 24 y 26 MHz sin tendencia ascendente significativa, se prioriza estudiar el tamaño del burst TCP_RX antes de seguir aumentando frecuencia.

Esto **no fija todavía la frecuencia final del producto**. G3 debe comprobar si el tamaño del burst interactúa con la frecuencia y cambia el punto óptimo.

## Estado del sweep

| Frecuencia | Estático | Smoke físico | Calificación | Estado |
|---:|---|---|---|---|
| 14 MHz | baseline | PASS | D40-R2 congelado | BASELINE |
| 20 MHz | PASS | PASS | PASS | PASS |
| 24 MHz | PASS | PASS ×2 | PASS | PASS |
| 26 MHz | PASS | PASS | PASS | PASS |
| 30 MHz | — | — | — | NOT TESTED — PLATEAU |

## G3 propuesto — sweep de chunks TCP RX

G3 cambia deliberadamente el algoritmo, por lo que queda separado de G2.

### Frecuencias iniciales

Probar **14, 20 y 26 MHz**:

- 14 MHz: control bajo / baseline de frecuencia;
- 20 MHz: knee observado en G2;
- 26 MHz: zona de meseta alta.

La separación permite detectar si 8 chunks cambia la interacción entre frecuencia y throughput.

### Configuración inicial

- baseline G2: `TCP_RX_MAX_CHUNKS_PER_LOCK = 4`;
- candidato G3: `TCP_RX_MAX_CHUNKS_PER_LOCK = 8`;
- máximo por chunk: 1024 bytes, sin cambios;
- resto del algoritmo sin cambios.

### Criterio de hold para G3

- `TCP_SPI_HOLD_MAX_US <= 5000`: **PASS_PREFERRED**;
- `5000 < TCP_SPI_HOLD_MAX_US <= 10000`: **REVIEW**;
- `TCP_SPI_HOLD_MAX_US > 10000`: **FAIL**;
- cualquier diagnóstico visual `SPI`: **FAIL**;
- cualquier error de ownership/transporte o pérdida de integridad: **FAIL**.

El límite de 10 ms es un techo experimental, no una relajación automática del criterio de convivencia SPI.

### Posible paso intermedio

Si 8 chunks produce una mejora material de throughput pero cae repetidamente en `REVIEW`, evaluar 6 chunks como compromiso entre rendimiento y latencia de bus.

## Pendientes

1. Iniciar G3 con cambio aislado `4 → 8 chunks`.
2. Ejecutar primero una validación estática del cambio de chunks sin modificar simultáneamente frecuencia.
3. Probar 8 chunks en 14, 20 y 26 MHz con snapshots exactos y criterios G3.
4. Comparar contra los resultados G2 de 4 chunks.
5. Si corresponde, probar 6 chunks como compromiso.
6. No iniciar optimización TX hasta cerrar G3.
7. Mantener pendiente histórico `D24_OTHER_RAW_CALL_COUNT=1_PENDING` hasta cerrar completamente la auditoría G1A.
