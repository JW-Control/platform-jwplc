# Alpha14 - R4-R6 matriz final TCP x RTU EXP-MIX - PREP 2026-10-02

## Objetivo

Ejecutar la matriz final de coexistencia TCP x RTU usando la configuración
ya validada en R4-R5, sin cambios funcionales adicionales.

## Configuración congelada

- C0 POLLING;
- INT/D2/D3/E1 OFF;
- W5500 SPI 26 MHz;
- RTU 500000 baud;
- Master FIFO 9 / Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP / Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE;
- timeout benchmark 25 ms;
- partial hold STRUCTURAL 15000 us;
- workload 2DI+2DO+2AI+2AO unpaced;
- full-runtime normal.

## Matriz

Cada caso dura 300 s:

- TCP OFF;
- TCP 100 req/s;
- TCP 250 req/s;
- TCP 500 req/s;
- TCP 750 req/s;
- TCP 1000 req/s.

Se exige cero failures/timeouts/rejected/verify/CRC, cero discarded tails/bytes,
cross-count exacto, mapas finales exactos y runtime/periféricos sin fallos.

Un target TCP no alcanzado con runtime limpio se clasifica
`SATURATION_FAIL_CLEAN` y no aborta la matriz.

R4-R6 será la fuente primaria para la curva final TCP vs capacidad de
expansiones y para definir valores comerciales con margen.