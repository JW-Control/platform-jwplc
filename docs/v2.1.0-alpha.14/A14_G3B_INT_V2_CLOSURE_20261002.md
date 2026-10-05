# Alpha14 — cierre de evaluación INT para JWPLC Basic v2 — 2026-10-02

## Decisión final

La evaluación de recepción TCP guiada por INT se cierra para JWPLC Basic v2.

```text
SELECTED_POLICY=POLLING_C0
INT_V2=NOT_PROMOTED
INT_DEFAULT=OFF
D2_DEFAULT=OFF
D3_DEFAULT=OFF
E1_RSR_DRAIN_DEFAULT=OFF
```

No se observó una variante INT que supere globalmente a C0 en full-runtime.
INT sí demostró reducción importante de polling vacío y actividad SPI en
idle/carga baja, pero esa ventaja no se convirtió en una mejora de throughput
o latencia bajo 1000 req/s con runtime completo.

## Evidencia principal

| Variante | req/s | AVG us | P95 us | P99 us | MAX us | Loop avg us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| C0 POLLING | 1000.00 | 884.1 | 1262.6 | 2413.6 | 8972.1 | 591 |
| INT puro R1 | 997.86 | 977.5 | 1373.6 | 2659.8 | 8155.3 | 266 |
| D2 hot 1500 | 1000.00 | 937.8 | 1360.8 | 2953.7 | 17182.9 | 879 |
| D3-C1 diagnóstico | 999.965 | 935.1 | 1374.1 | 2622.3 | 8065.8 | 905 |
| D3-C2 diagnóstico | 999.949 | 944.0 | 1415.6 | 2727.5 | 8073.6 | 1096 |
| D3-C3 | 853.791 | 1166.2 | 1898.3 | 4131.1 | 15540.9 | 712 |
| E1 RSR-drain | 999.999 | 945.8 | 1494.5 | 3558.8 | 19311.6 | 398 |

## Qué sí demostró INT

La línea INT no se considera inútil. Los gates RAW/controlados demostraron:

```text
INT_BUS_EFFICIENCY=YES
INT_SPEED_GAIN=NO_PROVEN
```

En baja carga/idle se redujeron fuertemente las consultas SPI vacías. Esa
propiedad puede volver a ser valiosa en una arquitectura con bus Ethernet
dedicado.

## Motivo de cierre en v2

El JWPLC Basic v2 comparte recursos SPI con otros periféricos del runtime.
Después de INT puro, hot-poll D2, scheduler adaptativo D3 y RSR-drain E1, no
apareció una política INT que conserve la ventaja de baja carga y además
iguale/supere el comportamiento de C0 bajo full-runtime.

Continuar ajustando thresholds o agregando estados ya no tiene una relación
beneficio/tiempo favorable.

## Reapertura futura

Reevaluar INT en JWPLC Basic v3 con ESP32-S3 y W5500 en bus SPI dedicado.

El nuevo estudio debe partir nuevamente de dos candidatos simples:

1. POLLING optimizado.
2. INT wake-up + RSR-drain.

No heredar automáticamente thresholds D2/D3 de v2.

## Pendientes posteriores a este cierre

- Mantener defaults INT/D2/D3/E1 en OFF.
- No promover código experimental INT como política productiva de v2.
- Conservar los gates/documentación como evidencia y referencia para v3.
- Continuar Alpha14 desde la política C0 POLLING validada.
