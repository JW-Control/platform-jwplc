# Alpha14 - Cierre final RAW TCP/UDP ceiling — 2026-10-03

## Resultado

`A14_FINAL_RAW_UDP_CEILING=PASS_CHARACTERIZED`

Se ejecutaron cinco ventanas de 300 s sobre el package actual y W5500 a 26 MHz.

## Resultados actuales

| Caso | DUT throughput | PC offered/throughput | Errores | Reset |
| --- | ---: | ---: | ---: | --- |
| TCP RX | 14.108621 Mbps | 14.108621 Mbps | 0 | NO |
| TCP TX | 4.646077 Mbps | 4.646049 Mbps | 0 | NO |
| UDP RX legacy | 11.601009 Mbps | 1007.346828 Mbps offered | 0 | NO |
| UDP TX | 5.175095 Mbps | 5.175055 Mbps | 0 | NO |
| UDP RX FAST | 13.179105 Mbps | 956.998807 Mbps offered | 0 | NO |

La tasa del host en UDP RX es offered rate; bajo flood la pérdida no representa corrupción del DUT. La cifra de capacidad relevante es el throughput consumido por el DUT.

## Telemetría

### TCP RX

- SPI occupancy: 96.784 %
- SPI hold avg: 4138 us
- SPI hold max: 5251 us
- loop gap avg: 4285 us
- loop gap max: 58224 us

### TCP TX

- SPI occupancy: 93.754 %
- SPI hold avg: 1185 us
- SPI hold max: 2587 us
- loop gap avg: 1266 us
- loop gap max: 57933 us

### UDP RX legacy

- DUT: 11.601009 Mbps
- loss under offered flood: 98.848360 %
- SPI occupancy reported by benchmark: 6.125 %
- SPI hold avg: 26 us
- SPI hold max: 619 us
- loop gap avg: 441 us

### UDP TX

- DUT: 5.175095 Mbps
- secuencias: 131839
- duplicates: 0
- reorders: 0
- range missing: 0
- wrong size: 0
- SPI occupancy: 1.605 %
- SPI hold max: 572 us

### UDP RX FAST

- DUT: 13.179105 Mbps
- loss under offered flood: 98.622871 %
- transport errors: 0
- SPI lock errors: 0
- SPI occupancy: 92.470 %
- SPI hold avg: 353 us
- SPI hold max: 1966 us
- loop gap avg: 382 us
- loop gap max: 2900 us

## Comparación histórica válida

### TCP RX

Baseline histórico RAW a 26 MHz: ~13.798 Mbps.

Actual: 14.108621 Mbps.

Variación aproximada: +2.25 %.

Existe además evidencia histórica puntual cercana a 14.49 Mbps; se conserva como mejor observación histórica, pero no sustituye el valor current-package de esta campaña.

### UDP RX legacy

Baseline histórico: 11.410917 Mbps.

Actual: 11.601009 Mbps.

Variación: +1.67 %.

### UDP TX

Baseline histórico: 5.178235 Mbps.

Actual: 5.175095 Mbps.

Variación: -0.06 %.

Conclusión: throughput esencialmente idéntico al baseline.

### UDP RX FAST

Mejor candidato experimental P3: ~13.866 Mbps.

FAST productivo actual: 13.179105 Mbps.

Variación frente al candidato experimental: aproximadamente -4.96 %.

Sin embargo, frente al legacy current-package:

- legacy: 11.601009 Mbps
- FAST: 13.179105 Mbps
- mejora FAST: aproximadamente +13.60 %

La comparación correcta es mantener separado el candidato experimental P3 del camino FAST actualmente productizado.

## Source/product verification

Los compile logs de F1A y F1B muestran compilación desde source para:

- JWPLC_Ethernet/JWPLC_W5x00_Ethernet.cpp
- JWPLC_Ethernet/EthernetUdp.cpp
- JWPLC_Ethernet/utility/w5100.cpp
- SPI/SPI.cpp

No aparece marcador de librería precompilada para JWPLC_Ethernet ni SPI.

El core y otras librerías cualificadas pueden usar sus mecanismos precompilados normales; eso no oculta los cambios del transport Ethernet/SPI medidos aquí.

## Cifras RAW current-package aptas para documentación

- TCP RX RAW: **14.11 Mbps**
- TCP TX RAW: **4.65 Mbps**
- UDP RX legacy RAW: **11.60 Mbps**
- UDP TX RAW: **5.18 Mbps**
- UDP RX FAST RAW: **13.18 Mbps**

Estas cifras caracterizan caminos RAW y no deben confundirse con req/s Modbus.

## Evidencia local

`tools/modbus-tcp-benchmark/results/a14_final_raw_udp_ceiling_20261003_112011/`

Resultado final:

`A14_FINAL_RAW_UDP_CEILING=PASS_CHARACTERIZED`

## Siguiente frente

Con TCP/RTU coexistence y ceilings RAW individuales cerrados, queda validar una combinación simultánea TCP + UDP + RTU bajo full runtime antes de publicar una cifra conjunta de coexistencia.
