# Alpha14.3 — D0m Full Runtime 30 min — REVIEW por postproceso

Fecha: 2026-09-13

## Clasificación

`REVIEW`

La ejecución física de 30 minutos terminó y el benchmark Modbus TCP reportó `STABLE_PASS`, pero el runner falló durante el postproceso al intentar leer una clave inexistente (`row['ok']`). Por ello no se considera todavía cerrado el gate completo hasta recuperar y validar el snapshot final de periféricos.

## Perfil ejecutado

- FC03 / 125 registros
- 1000 req/s solicitados
- 1800 s
- HMI declarativa `USER_REFRESH_ON_DEMAND`
- barra dinámica cada 100 ms
- microSD workload completo:
  - append cada 1000 ms
  - flush cada 5 registros
  - verify cada 5000 ms
- FRAM, RTC, botonera, TCA/I/O y probe SPI activos
- sin recompilación ni upload respecto de D0l

## Resultado Modbus TCP preservado

- `ACHIEVED_REQ_S=999.34`
- `ACHIEVED_PCT=99.934`
- `OK=1798804/1800000`
- `P95_US=1323.7`
- `P99_US=2406.6`
- `MAX_US=27319.1`
- `LOOP_AVG_US=496`
- `LOOP_MAX_US=27253`
- `CONNECT_ATTEMPTS=1`
- `TIMEOUTS=0`
- `TRANSPORT_ERRORS=0`
- `PROTOCOL_ERRORS=0`
- `BUS_LOCK_TIMEOUTS=0`
- `CROSS_COUNT_PASS=YES`
- resultado formal: `STABLE_PASS`

## Fallo del runner

Después de terminar el soak y de obtener el snapshot final mediante `q.wait_server_ready(...)`, el script falló al imprimir:

```python
row['ok']
```

El diccionario devuelto por `frontier.run_case()` no expone esa clave con ese nombre. El fallo ocurrió en el postproceso y no durante la ventana de 30 minutos.

Salida:

```text
D0M_FATAL=KeyError: 'ok'
A14_3_FULL_RUNTIME_30MIN=REVIEW
RUNNER_EXIT=1
```

## Interpretación

- No hay evidencia de fallo físico del JWPLC Basic.
- La ventana completa de 1800 s terminó.
- El plano Modbus TCP fue estable y prácticamente alcanzó el objetivo exacto de 1000 req/s.
- Falta únicamente recuperar/mostrar los contadores finales de HMI, SD y periféricos para completar la clasificación integral.
- No se repetirá el soak de 30 minutos salvo que el snapshot recuperado indique una anomalía material.

## Siguiente gate

`D0m-R — snapshot recovery`, PC-only, sin compile ni upload.

Objetivo:

1. consultar el snapshot acumulado del firmware D0l todavía cargado;
2. validar HMI, SD, FRAM, RTC, I/O, botones y SPI;
3. combinar ese snapshot con el resultado Modbus ya preservado;
4. clasificar definitivamente D0m;
5. si queda limpio, cerrar A14.3 y pasar a A14.4 RTU + TCP simultáneo.
