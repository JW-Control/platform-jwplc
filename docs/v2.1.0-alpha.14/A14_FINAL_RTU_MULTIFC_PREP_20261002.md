# Alpha14 — RTU final R2 MULTI-FC FAST — PREP 2026-10-02

## Objetivo

Validar durante 600 s el perfil RTU FAST con todos los function codes que
cubren las operaciones principales de módulos de expansión y las APIs
fundamentales del Master:

- FC01 Read Coils: 8 bits;
- FC02 Read Discrete Inputs: 8 bits;
- FC03 Read Holding Registers: 2 registros;
- FC04 Read Input Registers: 4 registros;
- FC05 Write Single Coil: 1 bit;
- FC06 Write Single Register: 1 registro;
- FC0F Write Multiple Coils: 8 bits;
- FC10 Write Multiple Registers: 2 registros.

## Verificación de escrituras

El ciclo no considera suficiente una respuesta válida. Después de cada
escritura se realiza readback:

- FC05 -> FC01;
- FC06 -> FC03;
- FC0F -> FC01;
- FC10 -> FC03.

Los valores deben coincidir exactamente con el estado esperado.

## Perfil

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frame gap 100 us;
- CRC default BITWISE;
- TCP OFF.

## Criterios

- cada operación cubierta;
- started == success por operación;
- failed=0;
- verify failures=0;
- rejected=0;
- CRC=0;
- timeouts=0;
- exceptions=0;
- Master stats exacto;
- Slave RX/TX/OK igual al total del Master;
- sin reboot;
- source-first;
- Serial mudo durante la ventana;
- stop quiescente sólo en frontera de ciclo multi-FC completo.

Si R2 pasa, el siguiente gate será R3 EXP-MIX con los patrones de ocho módulos
`3-3-1-1` y `2-2-2-2`.