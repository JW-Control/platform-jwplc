# Alpha14 - R4-R4 partial hold 15 ms - PREP 2026-10-02

Objetivo: comprobar que ampliar el hold de parciales STRUCTURAL de 1750 a
15000 us evita el descarte 8+5 observado en FC10 bajo full-runtime.

Se mantiene sin cambios el resto del perfil: 500 kbaud, FIFO 9/8, BULK,
QUEUED, Master GAP, Slave STRUCTURAL, frame gap 100 us, CRC BITWISE, timeout
de benchmark 50 ms y workload 2DI+2DO+2AI+2AO.

Primer gate: TCP OFF durante 600 s.

Criterios: cero failures/timeouts/rejected/verify/CRC, cero discarded tails y
discarded bytes en Slave, cross-count exacto, mapas finales exactos y runtime
completo sin fallos ni reboot.

Si pasa, se reanuda la matriz TCP.