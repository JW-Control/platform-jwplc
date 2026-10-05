# Alpha14 — R4-R1 TCP x RTU EXP-MIX — diagnóstico 2026-10-02

## Corrida 1

TCP OFF, 300 s, full-runtime, RTU EXP-MIX 2-2-2-2 unpaced:

- RTU started/completed: 380256;
- RTU success: 380254;
- RTU failed: 2;
- ambos fallos: DO / FC0F;
- RTU master timeouts: 2;
- CRC: 0;
- verify: 0;
- rejected: 0;
- last failure result: 5 = JWPLC_MODBUS_TIMEOUT;
- last failure duration: 24677 us;
- max failure/transaction: 24987 us;
- transactions >20 ms: 2;
- peripheral failures: 0;
- SPI probe failures: 0;
- Ethernet/server: sanos;
- reboots: 0.

Conclusión: fallo real bajo el timeout agresivo de benchmark de 25 ms.
El package público no usa 25 ms como default de API; este valor pertenecía
al firmware de benchmark.

## Corrida 2 accidental

TCP OFF pasó limpio:

- RTU 1270.635 req/s;
- 158.829 scans/s;
- 6.296 ms/scan;
- 381216/381216 success;
- transaction max 10863 us;
- clasificación PASS.

Al pasar a TCP 100 req/s apareció de nuevo PRODUCT_FAILURE. El snapshot
posterior volvió a mostrar un extremo de 24786 us y 3 transacciones >20 ms,
firma consistente con el umbral de 25 ms. No se usa esta segunda corrida
para afirmar número/tipo exacto de fallos porque esos campos no se imprimieron
en el diagnóstico capturado.

El cambio accidental posterior de firmware del Slave no invalida la evidencia
guardada de las ventanas ya cerradas ni los contadores conservados por el Master.

## Problemas de harness detectados

1. El snapshot completo previo al inicio de ventana se imprimía después del
   reset y contaminaba LOOP_GAP_MAX / RTU_SERVICE_GAP_MAX.
2. `master_final.log` quedaba vacío porque el helper parseado no conservaba
   `_RAW`.

Ambos se corrigen para R4-R2.