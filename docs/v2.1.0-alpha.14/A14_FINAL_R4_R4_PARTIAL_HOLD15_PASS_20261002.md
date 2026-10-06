# Alpha14 - R4-R4 partial hold 15 ms - PASS 2026-10-02

## Resultado

`PASS_CHARACTERIZED`

Ventana full-runtime TCP OFF de 600 s con RTU EXP-MIX 2DI+2DO+2AI+2AO
unpaced, 500 kbaud, FIFO 9/8, BULK, QUEUED, Master GAP y Slave STRUCTURAL.

### RTU

- scans completos: 96198;
- RTU rate: 1282.563 req/s;
- scan rate: 160.320 scans/s;
- periodo de scan: 6.238 ms;
- requests started/completed/success: 769584/769584/769584;
- failed/rejected/verify/timeouts: 0/0/0/0;
- transaction max: 12567 us;
- transacciones >20 ms: 0;
- DI/DO/AI/AO failures: 0/0/0/0;
- CRC Master/Slave: 0/0;
- exceptions: 0;
- cross-count exacto.

### Parser STRUCTURAL

- discarded tails: 0;
- discarded bytes: 0;
- discarded max age: 0.

### Comprobación de bytes

Cada scan transmite 78 bytes de requests. 96198 x 78 = 7503444 bytes.
El Slave reportó RTU_RX_BYTES=7503444 exactamente.

### Full-runtime

- FULL_RUNTIME_READY=YES;
- SERVER_READY=YES;
- ETH_READY=YES / ETH_LINK=UP;
- peripheral failures=0;
- SPI probe failures=0;
- SD datalog failed commits=0;
- FRAM failures=0;
- SD append/verify failures=0;
- I/O stale=0;
- buttons not ready=0;
- RTC stale=0;
- no reboot;
- tracked dirty/staged final: 0/0.

## Decisión

El cambio de partial hold STRUCTURAL 1750 -> 15000 us queda validado para
esta condición full-runtime. La causa raíz del timeout previo fue el descarte
de una FC10 fragmentada 8+5 bytes, no pérdida física de bytes.

Antes de reanudar la matriz TCP se restaura el timeout de benchmark de 25 ms
para demostrar que la corrección no depende del margen temporal de 50 ms.