# Alpha14 — G3B-D2-C0 current-head POLLING — PREP 2026-10-01

## Objetivo

Separar definitivamente dos hipótesis observadas después del H3E-R adaptativo:

1. la cola P99/MAX mayor pertenece al scheduler D2;
2. la diferencia fue variabilidad de la corrida full-runtime.

C0 no cambia producto ni harness. Reutiliza el H3E-R existente en el HEAD
actual y compila el Master sin overrides INT/HOT.

## Configuración

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
TCP=FC03/125 @1000 req/s
WINDOW=120 s
RTU=50 Hz
FULL_RUNTIME=ON
```

## Referencias

POLLING G3B-D0:

```text
REQ_S=1000.00
AVG=884.8 us
P95=1269.2 us
P99=2272.5 us
MAX=7942.6 us
LOOP_AVG=633 us
LOOP_MAX=12101 us
```

ADAPTIVE G3B-D2:

```text
REQ_S=1000.00
AVG=937.8 us
P95=1360.8 us
P99=2953.7 us
MAX=17182.9 us
LOOP_AVG=879 us
LOOP_MAX=12415 us
```

## Clasificación automática

El wrapper `a14_g3b_d2_c0_current_head_polling.ps1` exige primero PASS formal
H3E-R, 120000/120000 y defaults INT/HOT apagados.

Luego marca `C0_POLLING_REPRODUCES_LOW_TAIL=YES` cuando:

- rate >= 999.95 req/s;
- P95 <= D0 +5 %;
- P99 <= D0 +10 %.

Estos márgenes no redefinen los guards de producto; sirven únicamente para
clasificar si la corrida current-head volvió al régimen de cola baja ya
observado dos veces con POLLING.

Si reproduce:

```text
C0_INTERPRETATION=D2_TAIL_COST_CONFIRMED
C0_NEXT=G3B_D3_LOAD_ADAPTIVE_DESIGN
```

Si no reproduce:

```text
C0_INTERPRETATION=FULL_RUNTIME_VARIABILITY_REVIEW
C0_NEXT=REVIEW_C0_BEFORE_D3
```

## Ejecución

```powershell
& .\tools\modbus-tcp-benchmark\gates\a14_g3b_d2_c0_current_head_polling.ps1 `
  -MasterPort COM14 `
  -SlavePort COM4 `
  -PythonExe "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe"
```

No abrir monitor serie ni ejecutar tráfico adicional durante la ventana formal.
