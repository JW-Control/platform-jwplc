# Alpha14 — R-FC10 R2 — PASS 2026-10-02

## Resultado

`PASS`

Qualification física de 600 s con:

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- RX BULK/BULK;
- TX QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frameGap 100 us;
- CRC BITWISE.

Resultados:

- FC10_SUCCESS=158782;
- FC03_SUCCESS=158782;
- CYCLES=19847;
- TRANSACTION_MAX_US=6879;
- FC10_FAILED=0;
- FC03_FAILED=0;
- VERIFY_FAILS=0;
- REQUEST_REJECTED=0;
- CRC Master=0;
- CRC Slave=0;
- timeout Master=0;
- exceptions Slave=0;
- cantidades 1/2/4/8/16/32/64/123 cubiertas;
- Master stats exacto;
- Slave cross-count exacto;
- Master/Slave sin reboot;
- source-first: un objeto `JWPLC_ModbusRTU.cpp.o` por build;
- precompiled marker: NO.

FC10 Master queda validado para continuar con el gate multi-FC FAST.