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

## Estado del sweep

| Frecuencia | Estático | Smoke físico | Calificación | Estado |
|---:|---|---|---|---|
| 14 MHz | baseline | PASS | D40-R2 congelado | BASELINE |
| 20 MHz | PASS | PASS | PASS | PASS |
| 24 MHz | pendiente | pendiente | pendiente | PENDING |
| 26 MHz | pendiente | pendiente | pendiente | PENDING |
| 30 MHz | pendiente | pendiente | pendiente | PENDING |

## Pendientes

1. Ejecutar 24 MHz manteniendo las mismas invariantes.
2. Si 24 MHz pasa smoke, ejecutar 3 × 10 s por modo.
3. Repetir en 26 MHz y 30 MHz.
4. No iniciar optimización TX hasta terminar todo el sweep.
5. Mantener pendiente histórico `D24_OTHER_RAW_CALL_COUNT=1_PENDING` hasta cerrar completamente la auditoría G1A.
