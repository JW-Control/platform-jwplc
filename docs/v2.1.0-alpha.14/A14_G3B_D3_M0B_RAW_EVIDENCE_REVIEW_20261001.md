# Alpha14 — G3B-D3-M0B RAW TCP evidence review — 2026-10-01

## Objetivo

Decidir si hace falta repetir un gate físico RAW TCP antes de implementar D3-A.

## Evidencia disponible

G2-R1 ya ejecutó RAW TCP matched con tres repeticiones por variante en
CONTROLLED y SATURATED.

Resultado SATURATED:

```text
POLLING_DUT_MEDIAN=11.711949 Mbps
INT_DUT_MEDIAN=11.886347 Mbps
INT_VS_POLLING=+1.489 %
THROUGHPUT_GAIN=NOT_CONFIRMED

POLLING_SPI_OCCUPANCY=92.960 %
INT_SPI_OCCUPANCY=82.615 %
DELTA=-10.345 pp

HOLD/STATUS_REDUCTION≈80 %
FUNCTIONAL=PASS
RECONNECT=PASS
```

Resultado CONTROLLED:

```text
throughput≈sin cambio
SPI_OCCUPANCY=65.886 -> 16.555 %
HOLD/STATUS_REDUCTION≈90.2 %
FUNCTIONAL=PASS
```

## Verificación de vigencia

Se comparó el commit del cierre G2-R1:

```text
d0fb8927dc44db997ccbd503444e86b0ecc697ba
```

contra el HEAD previo a M0A/migración.

Desde G2-R1 no cambiaron:

```text
JWPLC_Ethernet/*
SPI transport source
raw TCP profiler transport path
```

Los cambios funcionales posteriores relevantes quedaron en:

```text
JWPLC_ModbusTCP.cpp/.h
jwplc_hardware_config.h
```

Por tanto el camino RAW Ethernet/SPI que G2-R1 midió no ha sido modificado por
D1/D2, que pertenecen al scheduler de JWPLC_ModbusTCP.

## Interpretación

RAW saturation no aporta evidencia de que INT deba mantenerse activo para ganar
throughput:

```text
RAW_INT_THROUGHPUT_GAIN=NOT_CONFIRMED
RAW_INT_BUS_EFFICIENCY=CONFIRMED
```

Para D3 esto es suficiente:

- en idle/baja carga, INT tiene valor claro por eficiencia;
- en alta carga, pasar a polling no sacrifica una ganancia RAW de throughput
  demostrada;
- el objetivo prioritario en alta carga puede ser conservar latencia/headroom
  y fairness del runtime.

## Decisión

```text
M0B_PHYSICAL_RERUN_REQUIRED=NO
M0B_REASON=RAW_TRANSPORT_SOURCE_UNCHANGED_AND_G2R1_ALREADY_MATCHED
M0B_STATUS=PASS_EVIDENCE_REUSE
NEXT=G3B_D3_A_LOAD_ADAPTIVE_IMPLEMENTATION
```

No repetir hardware sólo para reproducir una evidencia ya válida.

El RAW TCP ~14.49 Mbps de P4.2 LR600 sigue siendo un pendiente distinto:
reconciliar el ceiling current-package después de cerrar/promover D3 y antes de
reabrir DIRECT_RX.
