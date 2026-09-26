# Alpha14 — Cierre P3 UDP RX

Fecha: 2026-09-24  
Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Objetivo

Determinar si la ruta UDP RX del W5500 podía alcanzar el rendimiento de TCP RX
manteniendo:

- W5500 a 26 MHz.
- 8 sockets simétricos de 2 KB.
- shared SPI con los periféricos JWPLC.
- compatibilidad Arduino.
- sin mutar código productivo hasta cerrar el mecanismo.

## Baseline histórico

A 26 MHz:

- TCP RX RAW: ~13.798 Mbps.
- UDP RX RAW: ~11.411 Mbps.

La brecha inicial era de aproximadamente 17 %.

## Hallazgos principales

### Batch2

Procesar hasta dos datagramas dentro del mismo ownership SPI mostró que la
amortización del lock compartido es relevante.

### Boundary 1016 B

Con 2 KB por socket:

- pseudo-header W5500 UDP: 8 B.
- payload: 1016 B.
- registro completo: 1024 B.
- dos registros: 2048 B.

Este tamaño permite un batch2 especialmente limpio con el buffer RX actual.

### INT-ETH GPIO15

El pin físico INTn del W5500 fue validado en GPIO15.

La guía por interrupción redujo los service holds vacíos a prácticamente cero
sin pérdida material de throughput.

### Fused receive

Fusionar lectura de pseudo-header + payload redujo trabajo administrativo y
aportó una mejora relativa repetible de aproximadamente 3–4 % frente al camino
legacy dentro de pruebas pareadas.

### Single-CS burst

Reducir toggles CS/headers SPI disminuyó las lecturas SPI por paquete en ~17 %,
pero no produjo ganancia de throughput repetible.

Decisión:

```txt
P3I_SINGLE_CS_PRODUCTIZE=NO
```

### Commit coalescido

El mecanismo final procesa hasta dos registros UDP y difiere:

- `Sn_RX_RD`
- `Sock_RECV`

hasta el final del hold.

El primer intento perdió liveness de INT al limpiar `Sn_IR(RECV)` antes de
liberar el buffer.

La secuencia corregida quedó:

```txt
drain batch2
-> commit RX_RD + Sock_RECV
-> clear Sn_IR(RECV)
-> one-shot Sn_RX_RSR
-> rearm pending si RSR>0 o INT sigue LOW
```

También se añadió una barrera de lifecycle del benchmark mediante comando serial
IDLE y verificación explícita de quiescencia entre corridas.

## P3J-R2 — repetibilidad contra FUSED

```txt
BLOCK1:
COMMIT2_R1=13.872864 Mbps
FUSED=12.786238 Mbps
GAIN=+8.50 %

BLOCK2:
COMMIT2_R1=13.863074 Mbps
FUSED=12.829208 Mbps
GAIN=+8.06 %

GAIN_SPREAD=0.44 pp
COMMIT2_R1_MEDIAN=13.867969 Mbps
FUSED_MEDIAN=12.794340 Mbps
AGGREGATE_GAIN=+8.39 %

COMMIT2_R1_DRIFT=-0.07 %
FUSED_DRIFT=+0.34 %
```

## P3K — TCP RX vs UDP RX same-session

Mismo firmware, un solo upload, 26 MHz, 8x2KB y orden balanceado.

### Bloque 1

```txt
UDP median = 13.860041 Mbps
TCP median = 13.413063 Mbps
UDP vs TCP = +3.33 %
```

### Bloque 2

```txt
UDP median = 13.872847 Mbps
TCP median = 13.344293 Mbps
UDP vs TCP = +3.96 %
```

### Agregado

```txt
UDP median = 13.866349 Mbps
TCP median = 13.412507 Mbps
UDP vs TCP = +3.38 %
GAIN_SPREAD = 0.63 pp

UDP absolute drift = +0.09 %
TCP absolute drift = -0.51 %
```

Resultado:

```txt
P3K_INTERPRETATION=UDP_REPEATABLE_ABOVE_TCP
A14_P3K_SAME_SESSION_TCP_UDP_PARITY=PASS
```

## Conclusión técnica P3

En el benchmark RAW same-session, la ruta UDP RX experimental ya no es el cuello
frente a TCP RX.

```txt
P3_UDP_RX_TARGET_TCP_PARITY=PASS
P3_UDP_RX_SAME_SESSION_RESULT=UDP_ABOVE_TCP_BY_3.38_PERCENT
P3_UDP_RX_FINAL_DIAGNOSTIC_CANDIDATE=BATCH2_INT_FUSED_COMMIT2_R1
P3_UDP_RX_SPI_HZ=26000000
P3_UDP_RX_SOCKET_TOPOLOGY=8x2KB
```

## Decisión de productización

El candidato de alto rendimiento NO se copiará silenciosamente sobre la API
Arduino legacy.

La API actual separa:

```txt
parsePacket()
read(buffer)
```

mientras el candidato de P3 requiere:

- lectura completa de pseudo-header + payload;
- procesamiento de hasta dos datagramas por ownership SPI;
- commit diferido;
- coordinación explícita del lifecycle INT.

Una integración transparente podría cambiar semántica o requerir buffering
adicional.

Decisión:

```txt
LEGACY_UDP_API_BREAK=NO
TRANSPARENT_REPLACEMENT=NO
FUTURE_HIGH_PERFORMANCE_PATH=ADDITIVE_OR_INTERNAL_API
P3_PRODUCT_SOURCE_MUTATION=NO
```

Para Alpha14/Modbus TCP este trabajo se conserva como evidencia de capacidad y
como base para una futura API cooperativa UDP, sin introducir riesgo innecesario
en la API Arduino ya validada.

## Siguiente fase

Cerrar la investigación RAW de rendimiento y volver a qualification de runtime
real:

- Modbus TCP FC03/125.
- carga objetivo 1000 req/s.
- RTU ~50 Hz simultáneo.
- Display y periféricos normales activos.
- latencia avg/P95/P99/max.
- loop max.
- errores de transporte/SPI.
- recuperación y estabilidad.

