# Alpha14 — R4-R3 matriz completa TCP x RTU EXP-MIX — PREP 2026-10-02

## Objetivo

Ejecutar la matriz final bajo el mismo firmware y configuración que pasó
R4-R2, sin cambios funcionales adicionales.

### Configuración

- full-runtime real;
- C0 POLLING;
- INT/D2/D3/E1 OFF;
- W5500 SPI 26 MHz;
- RTU FAST 500 kbaud;
- Master FIFO 9 / Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP / Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE;
- timeout benchmark RTU 50 ms;
- workload `2DI + 2DO + 2AI + 2AO` unpaced.

### Casos

Cada caso dura 300 s:

- TCP OFF;
- TCP 100 req/s;
- TCP 250 req/s;
- TCP 500 req/s;
- TCP 750 req/s;
- TCP 1000 req/s.

Una incapacidad de alcanzar el target TCP con runtime limpio se clasifica
`SATURATION_FAIL_CLEAN` y no detiene la matriz. Un fallo real de RTU, TCP,
periféricos, SPI, mapas, cross-count o reboot sí la detiene.

R4-R3 será la fuente principal para la curva y tabla comercial redondeada.