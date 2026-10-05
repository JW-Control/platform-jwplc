# Alpha14 — R4-R2 timeout RTU 50 ms — PREP 2026-10-02

## Hipótesis única

El timeout de 25 ms del firmware de benchmark es demasiado agresivo para
full-runtime: los fallos observados terminan a ~24.7-25.0 ms sin CRC, verify,
SPI, periféricos, Ethernet o reboot asociados.

## Cambio experimental

Único cambio de comportamiento:

- RTU timeout benchmark: 25 ms -> 50 ms.

No se modifica la librería `JWPLC_ModbusRTU`, sus defaults públicos ni el
perfil FAST 500k/FIFO9-8/BULK/QUEUED/GAP-STRUCTURAL.

## Higiene de medición

- se resetean nuevamente contadores/mapas después de snapshots pre-window;
- `master_final.log` conserva ahora las claves parseadas aunque `_RAW` no exista;
- el runner imprime diagnóstico RTU detallado antes de abortar;
- el gate admite targets selectivos.

## Primera prueba R4-R2

Ejecutar únicamente TCP=100 durante 300 s con RTU EXP-MIX 2-2-2-2 unpaced.

Criterio: cero RTU failures/timeouts/rejected/verify/CRC, cross-count exacto,
periféricos/SPI/Ethernet/reboots limpios y TCP 100 req/s sostenido.

Si pasa, ejecutar la matriz completa OFF/100/250/500/750/1000 con el mismo
timeout de 50 ms.