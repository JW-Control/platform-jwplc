# Alpha14 — G3B-E1 INT RSR-drain — PREP 2026-10-02

## Motivo

La ruta D3 se cierra después de C3: estabilizar estados mediante thresholds no produjo una mejora global frente a C0.

La documentación W5500 diferencia dos criterios de recepción TCP: evento `Sn_IR(RECV)` y `Sn_RX_RSR`. Advierte que RECV puede solaparse cuando el host no procesa perfectamente cada DATA packet antes del siguiente evento, y en ese caso no recomienda depender de cada RECV individual.

Referencia:
https://docs.wiznet.io/Product/Chip/Ethernet/W5500/Application/tcp

## Hipótesis E1

Usar INTn sólo como wake-up. Una vez despierto, mantener el servicio en drain hasta observar RX realmente vacío. No reconocer `Sn_IR(RECV)` al inicio de cada pasada.

Al observar RX vacío:

1. reconocer RECV;
2. volver a consultar `Sn_RX_RSR` inmediatamente;
3. si llegaron bytes durante la carrera de ACK, continuar drenando;
4. si permanece vacío, volver a esperar INT/fallback.

## Qué no cambia

- W5500 26 MHz;
- RX budget 64;
- FIFO/DLEN/COPY_OUT actuales;
- commit RX inmediato;
- fallback INT 10 ms;
- TX Modbus TCP legacy bloqueante;
- RTU 50 Hz;
- periféricos full-runtime;
- defaults productivos.

## Build E1

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=1
JWPLC_MODBUS_TCP_INT_RSR_DRAIN=1
JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
PROFILE_HOOKS=0
```

## Criterio de cierre

60 s a FC03/125 1000 req/s + full-runtime. Si E1 no queda aproximadamente a par de C0 (throughput completo y tails cercanos), INT se cerrará para JWPLC Basic v2. Sólo si queda realmente cerca se gastará una segunda corrida matched C0 antes de la decisión final.
