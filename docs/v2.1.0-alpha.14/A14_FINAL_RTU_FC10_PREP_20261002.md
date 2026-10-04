# Alpha14 — RTU final R-FC10 — PREP 2026-10-02

## Hipótesis

El Slave ya soportaba FC10 (`0x10 Write Multiple Registers`), pero el Master
cooperativo no exponía esa operación. Se añade como API aditiva sin modificar
defaults ni funciones existentes.

APIs nuevas:

- `requestWriteMultipleRegisters(...)`
- `writeMultipleRegistersSync(...)`
- `writeMultipleRegisters(...)`

El límite implementado es el estándar RTU de 1..123 registros por request.

## Gate físico

Duración mínima: 600 s.

Perfil qualification:

- 500000 baud
- Master FIFO=9
- Slave FIFO=8
- RX BULK/BULK
- TX QUEUED/QUEUED
- Master framing GAP
- Slave framing STRUCTURAL
- frameGap=100 us
- CRC BITWISE, sin cambiar el default

El Master alterna FC10 y FC03 de readback para cantidades:

`1, 2, 4, 8, 16, 32, 64, 123` registros.

Debe cumplirse: FC10/FC03 sin fallos, verify=0, rejected=0, CRC=0, timeout=0,
exceptions=0, cross-count Master/Slave exacto, todas las cantidades cubiertas,
sin reboot y build source-first de `JWPLC_ModbusRTU`.

Este gate prueba sólo FC10. No modifica TCP ni promueve FAST como default
universal. Si pasa, el siguiente gate será el workload mixto de expansiones
`FC02 + FC04 + FC0F + FC10` para patrones `3-3-1-1` y `2-2-2-2`.