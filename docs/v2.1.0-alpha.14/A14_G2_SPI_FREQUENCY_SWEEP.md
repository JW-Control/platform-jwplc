# Alpha14 — G2: sweep de frecuencia SPI Ethernet

## Objetivo

Evaluar el efecto de aumentar únicamente la frecuencia SPI efectiva del backend Ethernet/W5500, manteniendo congelados el firmware raw, el runner raw y el algoritmo de transporte.

Frecuencias planificadas: **14, 20, 24, 26 y 30 MHz**. No se incluye 40 MHz en este sweep.

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

20 MHz queda aceptado dentro del sweep y habilita avanzar a 24 MHz. No se interpreta aún como frecuencia final del producto; la decisión final se toma después de completar 24, 26 y 30 MHz.

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

Las 12 corridas funcionales dieron PASS, sin errores de transporte, sin errores de lock SPI y sin eventos visuales `SPI`.

### Comparación 20 vs 24 MHz

24 MHz mejora ligeramente TCP_RX respecto a 20 MHz, pero los otros tres modos quedan ligeramente por debajo de 20 MHz. La diferencia es pequeña y apunta por ahora a una meseta de rendimiento, no a una regresión funcional.

| Modo | Prom. 20 MHz | Prom. 24 MHz | Observación |
|---|---:|---:|---|
| TCP_RX | 12.860 | 13.119 | 24 MHz ligeramente mejor |
| TCP_TX | 4.685 | 4.638 | 20 MHz ligeramente mejor |
| UDP_RX DUT | 10.692 | 10.495 | 20 MHz ligeramente mejor |
| UDP_TX | 5.104 | 5.036 | 20 MHz ligeramente mejor |

### Decisión 24 MHz

`G2_24MHZ=PASS`

24 MHz queda aceptado dentro del sweep y habilita avanzar a 26 MHz. No se interpreta todavía como frecuencia final.

## Estado del sweep

| Frecuencia | Estático | Smoke físico | Calificación | Estado |
|---:|---|---|---|---|
| 14 MHz | baseline | PASS | D40-R2 congelado | BASELINE |
| 20 MHz | PASS | PASS | PASS | PASS |
| 24 MHz | PASS | PASS ×2 | PASS | PASS |
| 26 MHz | pendiente | pendiente | pendiente | PENDING |
| 30 MHz | pendiente | pendiente | pendiente | PENDING |

## Barrido posterior propuesto — chunks TCP RX

Este experimento queda deliberadamente fuera de G2 para no mezclar frecuencia SPI y algoritmo.

Una vez terminadas 26 y 30 MHz:

1. ordenar las frecuencias por rendimiento global y estabilidad;
2. escoger las **tres mejores frecuencias** del sweep G2;
3. comparar al menos **4 chunks vs 8 chunks** por ownership window, manteniendo 1024 bytes máximos por chunk;
4. mantener `5000 us` como objetivo preferido de hold;
5. clasificar `5000 < HOLD_MAX_US <= 10000` como **REVIEW**, no PASS automático;
6. considerar `HOLD_MAX_US > 10000`, error de ownership/transporte o cualquier evento visual `SPI` como **FAIL**;
7. registrar throughput, hold promedio/máximo, loop gap y comportamiento visual de TFT.

La intención es medir si el mayor burst amortiza overhead y mejora TCP_RX sin degradar de forma inaceptable la convivencia con TFT, SD y FRAM.

## Pendientes

1. Ejecutar 26 MHz manteniendo las mismas invariantes de G2.
2. Si 26 MHz pasa smoke, ejecutar 3 × 10 s por modo.
3. Repetir en 30 MHz.
4. Al cerrar G2, seleccionar las tres mejores frecuencias para el barrido de chunks.
5. No iniciar optimización TX hasta terminar todo el sweep de frecuencia y el análisis acordado de chunks.
6. Mantener pendiente histórico `D24_OTHER_RAW_CALL_COUNT=1_PENDING` hasta cerrar completamente la auditoría G1A.
