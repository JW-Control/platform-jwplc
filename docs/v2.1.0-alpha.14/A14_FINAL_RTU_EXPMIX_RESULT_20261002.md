# Alpha14 — RTU final R3 EXP-MIX — PASS 2026-10-02

## Resultado

`EXP_MIX=QUALIFIED`

Dos patrones de ocho expansiones lógicas fueron ejecutados durante 300 s
cada uno sobre RTU FAST 500 kbaud, FIFO 9/8, BULK, QUEUED, Master GAP y
Slave STRUCTURAL.

| Patrón | Scans | Scan Hz | Periodo scan | RTU req/s | Tx max | Estado |
|---|---:|---:|---:|---:|---:|---|
| 3DI+3DO+1AI+1AO | 59901 | 199.669 | 5.008 ms | 1597.356 | 2308 us | PASS |
| 2DI+2DO+2AI+2AO | 58140 | 193.800 | 5.160 ms | 1550.396 | 2450 us | PASS |

Ambos casos cumplieron:

- ocho transacciones exactas por scan;
- FC02/FC04 payload verificado;
- FC0F/FC10 sin fallos;
- mapa final de outputs exacto;
- rejected=0;
- CRC Master/Slave=0;
- timeout=0;
- exception=0;
- Master stats exacto;
- Slave cross-count exacto;
- reboot Master/Slave=0;
- source-first PASS.

## Decisión

Para la matriz TCP x RTU se selecciona `2DI+2DO+2AI+2AO` como patrón
conservador porque fue ligeramente más lento (~2.9 %) que `3-3-1-1`.

R3 usa un Slave físico con ocho slots lógicos. Caracteriza el mix/tamaño de
ADU y presupuesto de bus; no se presenta como validación multidrop física de
ocho dispositivos independientes.