# Alpha14 — G3B-D3-M0A load characterization — PREP 2026-10-01

Objetivo: medir IDLE, 100, 500, 750 y 1000 req/s comparando POLLING contra ADAPTIVE_D2 antes de fijar thresholds D3.

Builds:
- POLLING: INT=0, HOT=0
- ADAPTIVE_D2: INT=1, HOT=1500 us

Cada punto activo usa FC03/125 durante 15 s. IDLE usa una conexión aceptada y 10 s sin payload.

Métricas: req/s, AVG/P95/P99/MAX, loop avg/max, status calls, available calls, available-zero y ratios ADAPTIVE/POLLING.

M0A sólo construye la curva de carga. No modifica el producto ni decide thresholds.

Siguiente: revisar curva -> M0B RAW TCP -> D3-A implementación.

Ejecución:

```powershell
& "C:\\Users\\jeykc\\AppData\\Local\\Programs\\Python\\Python311\\python.exe" -B `
  .\\tools\\modbus-tcp-benchmark\\gates\\a14_g3b_d3_m0a_load_characterization.py `
  --serial COM14 `
  --arduino-cli "$env:LOCALAPPDATA\\Programs\\arduino-ide\\resources\\app\\lib\\backend\\resources\\arduino-cli.exe"
```
