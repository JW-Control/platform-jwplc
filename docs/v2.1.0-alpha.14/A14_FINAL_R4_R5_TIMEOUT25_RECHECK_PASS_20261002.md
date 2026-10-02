# Alpha14 - R4-R5 timeout 25 ms tras fix STRUCTURAL - PASS 2026-10-02

## Resultado

`PASS_CHARACTERIZED`

Ventana: 600 s, TCP OFF, full-runtime, RTU EXP-MIX
`2DI + 2DO + 2AI + 2AO` unpaced.

Configuración:

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE;
- timeout benchmark Master: 25 ms;
- `JWPLC_MODBUS_FAST_PARTIAL_HOLD_US=15000`.

## RTU

- scans completos: 96175;
- requests started/completed/success: 769400/769400/769400;
- failed: 0;
- rejected: 0;
- verify failures: 0;
- master timeouts: 0;
- CRC Master/Slave: 0/0;
- exceptions: 0;
- RTU rate: 1282.252 req/s;
- scan rate: 160.282 scans/s;
- scan period: 6.239 ms;
- transaction max: 15312 us;
- transacciones >20 ms: 0;
- DI/DO/AI/AO failures: 0/0/0/0;
- cross-count exacto.

## Parser / transporte

- Slave discarded tails: 0;
- Slave discarded bytes: 0;
- Slave RX bytes: 7501650;
- Master TX bytes: 7501650;
- Slave TX bytes: 6732250;
- Master RX bytes: 6732250.

Los bytes TX/RX cruzados coinciden exactamente en ambos extremos.

## Full-runtime

- FULL_RUNTIME_READY=YES;
- SERVER_READY=YES;
- ETH_READY=YES / ETH_LINK=UP;
- Display/FRAM/SD/RTC/I-O/botonera ready;
- peripheral failures=0;
- SPI probe failures=0;
- loop gap avg/max: 147 / 12577 us;
- SPI probe max wait: 463 us;
- no reboot;
- tracked dirty/staged final: 0/0.

## Decisión

Se confirma que el timeout de 25 ms no era la causa raíz. Con el parser
STRUCTURAL corregido a 15 ms, el mismo timeout agresivo vuelve a pasar
full-runtime durante 600 s sin pérdidas, tails ni timeouts.

El fix `partial hold 15 ms` queda como candidato a promoción para Alpha14.
El siguiente paso es ejecutar la matriz TCP completa sin cambios funcionales.