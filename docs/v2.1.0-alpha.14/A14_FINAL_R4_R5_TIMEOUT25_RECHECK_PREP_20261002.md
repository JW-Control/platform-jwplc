# Alpha14 - R4-R5 recheck timeout 25 ms - PREP 2026-10-02

## Hipótesis

Con el partial hold STRUCTURAL de 15 ms ya corregido, el timeout agresivo de
benchmark de 25 ms debe volver a pasar full-runtime sin pérdidas ni tails.

## Único cambio

- timeout RTU del firmware de benchmark: 50 -> 25 ms.

Se mantiene el fix de parser:

- JWPLC_MODBUS_FAST_PARTIAL_HOLD_US=15000 us.

El resto del perfil no cambia.

## Gate

TCP OFF durante 600 s.

Criterios:

- RTU failed/timeouts/rejected/verify/CRC=0;
- Slave discarded tails/bytes=0;
- cross-count y mapas exactos;
- periféricos/SPI/Ethernet/reboots limpios;
- source-first RTU/TCP probado.

Si pasa, la matriz completa TCP se ejecuta usando 25 ms.