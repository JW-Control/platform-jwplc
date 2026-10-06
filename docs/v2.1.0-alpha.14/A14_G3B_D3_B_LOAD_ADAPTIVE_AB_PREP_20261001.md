# Alpha14 — G3B-D3-B load-adaptive A/B — PREP 2026-10-01

## Objetivo

Comparar el candidato D3 contra POLLING sin cambiar defaults productivos.

Matriz:

```text
IDLE
100 req/s
500 req/s
750 req/s
1000 req/s
```

Cada build usa source de `JWPLC_ModbusTCP` y habilita hooks de perfil sólo
para el gate.

## Variantes

```text
POLLING:
  INT_GUIDED_RX=0
  INT_HOT_POLL_US=0
  INT_LOAD_ADAPTIVE=0

D3:
  INT_GUIDED_RX=1
  INT_HOT_POLL_US=0
  INT_LOAD_ADAPTIVE=1
```

Los dos builds conservan:

```text
FIFO_REUSE=ON
DLEN_REUSE=ON
COPY_OUT_64=ON
DIRECT_RX=OFF
TCP_RX_BATCH=8
W5500_SPI_HZ=26 MHz
```

## Evidencia

Además de req/s, P95/P99, loop y calls de status/available, D3-B registra:

- estado final D3;
- número de ADUs completas observadas;
- transiciones de estado;
- pasadas ACTIVE_POLL;
- último gap de trama.

D3-B es caracterización. No promueve automáticamente el candidato ni abre
H3E-R D3-C sin revisar primero la curva medida.

## Comando

```powershell
& "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe" -B `
  .\tools\modbus-tcp-benchmark\gates\a14_g3b_d3b_load_adaptive_ab.py `
  --serial COM14 `
  --arduino-cli "$env:LOCALAPPDATA\Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"
```
