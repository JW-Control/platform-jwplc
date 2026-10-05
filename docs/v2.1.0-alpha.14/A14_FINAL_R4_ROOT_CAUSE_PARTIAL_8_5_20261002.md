# Alpha14 — R4 causa raíz de timeout RTU FAST — 2026-10-02

## Evidencia

En R4 full-runtime con TCP OFF:

- Master started/completed: 380496;
- Master success: 380495;
- Master failed/timeouts: 1;
- fallo: AO / FC10;
- Slave requests OK: 380495;
- Slave discarded tails: 2;
- Slave discarded bytes: 13;
- tail lengths: 8 y 5 bytes;
- CRC Master/Slave: 0/0;
- exceptions: 0;
- RTU_RX_BYTES Slave: 3709836.

El workload hizo 47562 scans. Cada scan transmite 78 bytes de requests RTU,
por lo que 47562 x 78 = 3709836 bytes. Coincide exactamente con
`RTU_RX_BYTES`: todos los bytes físicos llegaron al Slave.

FC10 con 2 registros mide 13 bytes. El descarte 8+5 = 13 demuestra que la
request se fragmentó por la ruta FIFO/BULK y el parser STRUCTURAL descartó
ambos fragmentos antes de recomponerla.

El primer tail llegó a una edad máxima de 5863 us. El hold histórico del
parser FAST era 1750 us.

## Causa raíz

`JWPLC_MODBUS_FAST_PARTIAL_HOLD_US=1750` es demasiado corto para full-runtime
con Slave FIFO8. Una request local estructural parcial puede permanecer
fragmentada más de 1.75 ms aunque todos sus bytes hayan llegado al UART.

Subir el timeout del Master no corrige la causa: una request ya descartada por
el Slave nunca tendrá respuesta.

## Corrección candidata

Ampliar únicamente el hold de prefijos locales incompletos del parser
STRUCTURAL a 15000 us. Los frames completos continúan despachándose de forma
inmediata; la espera adicional sólo afecta prefijos incompletos.

El modo STRUCTURAL sigue siendo opt-in y no cambia la semántica default del
package.