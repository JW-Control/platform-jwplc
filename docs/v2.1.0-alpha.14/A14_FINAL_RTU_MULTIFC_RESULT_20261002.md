# Alpha14 — RTU final R2 MULTI-FC FAST — PASS 2026-10-02

## Resultado

`MULTI_FC_FAST=QUALIFIED`

Ventana física: 600.000 s.

Perfil:

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- RX BULK/BULK;
- TX QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE;
- TCP OFF.

## Funciones

| FC | Operación | Success | Failed |
|---:|---|---:|---:|
| 01 | Read Coils | 249312 | 0 |
| 02 | Read Discrete Inputs | 83104 | 0 |
| 03 | Read Holding Registers | 249312 | 0 |
| 04 | Read Input Registers | 83104 | 0 |
| 05 | Write Single Coil | 83104 | 0 |
| 06 | Write Single Register | 83104 | 0 |
| 0F | Write Multiple Coils | 83104 | 0 |
| 10 | Write Multiple Registers | 83104 | 0 |

## Totales

- ciclos completos: 83104;
- transacciones exitosas: 997248;
- rate mixto: 1662.080 req/s;
- ciclos multi-FC: 138.507 ciclos/s;
- periodo equivalente por ciclo: ~7.220 ms;
- transaction max: 2385 us;
- verify failures: 0;
- request rejected: 0;
- CRC Master/Slave: 0/0;
- timeouts: 0;
- exceptions: 0;
- cross-count Master/Slave exacto;
- reboot Master/Slave: 0;
- source-first PASS;
- tracked dirty final: 0;
- staged final: 0.

El rate multi-FC es superior al ceiling FC03-only porque el ciclo contiene
varias ADU cortas. No debe compararse como si ambos workloads tuvieran la
misma longitud de trama.

## Decisión

El perfil FAST queda cualificado para FC01/02/03/04/05/06/0F/10 con las
cantidades representativas verificadas por este gate.

El siguiente gate R3 mide dos workloads de ocho expansiones, sin readbacks
adicionales dentro de la ventana, para obtener capacidad de scan representativa:

- 3DI + 3DO + 1AI + 1AO;
- 2DI + 2DO + 2AI + 2AO.

R3 usa ocho slots lógicos sobre un Slave físico. Su objetivo es caracterizar
el mix/longitud de ADU y el presupuesto de bus; no sustituye la evidencia
histórica de multidrop físico.