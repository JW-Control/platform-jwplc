# Alpha14 — Benchmark de rendimiento Modbus TCP

Fecha: `2026-09-11`

## Objetivo

Medir el rendimiento real y sostenible de `JWPLC_ModbusTCP` sobre el W5500 integrado del JWPLC Basic, sin confundir la velocidad física Ethernet con la frecuencia útil de transacciones Modbus.

El resultado debe responder dos preguntas distintas:

```text
MAX_STABLE_REQUEST_RATE = mayor tasa sostenida sin errores ni degradación material
MAX_PEAK_REQUEST_RATE   = máximo throughput observado en saturación
```

No se eliminarán periféricos del autoload ni se modificará la arquitectura normal del JWPLC únicamente para mejorar el benchmark.

## Variables a medir

Para cada punto de prueba se registrará:

```text
requested_req_s
achieved_req_s
requests_total
requests_ok
errors
timeouts
reconnects
protocol_errors
transport_errors
latency_min_us
latency_avg_us
latency_p50_us
latency_p95_us
latency_p99_us
latency_max_us
application_bytes_s
loop_or_service_gap_avg_us
loop_or_service_gap_max_us
spi_bus_lock_timeouts
unexpected_resets
```

Cuando el rol probado sea Server se usarán además las estadísticas internas de `JWPLC_ModbusTCP`.

## Tasas objetivo

Cada caso se ejecutará primero con conexión TCP persistente a las siguientes cadencias:

| Intervalo objetivo | Tasa objetivo |
|---:|---:|
| 100 ms | 10 req/s |
| 50 ms | 20 req/s |
| 20 ms | 50 req/s |
| 10 ms | 100 req/s |
| 5 ms | 200 req/s |
| 2 ms | 500 req/s |
| 1 ms | 1000 req/s |
| sin espera | saturación / máximo posible |

Si un escalón deja de ser sostenible, no se asumirá que el siguiente puede considerarse válido aunque produzca un pico mayor.

## Tamaños de transferencia

### FC03 — Read Holding Registers

```text
1 register
16 registers
64 registers
125 registers
```

### FC16 — Write Multiple Registers

```text
1 register
16 registers
64 registers
123 registers
```

### FC01 — Read Coils

```text
8 bits
128 bits
512 bits
2000 bits
```

### FC15 — Write Multiple Coils

```text
8 bits
128 bits
512 bits
1968 bits
```

Las FC03/FC16 serán la referencia principal para throughput de datos; FC01/FC15 validarán el comportamiento con mapas de bits y empaquetado LSB-first.

## Duración

### Sweep inicial

Cada combinación tasa/tamaño:

```text
DURATION=30 s
```

### Confirmación de frontera

Los dos puntos alrededor del límite de estabilidad se repetirán por:

```text
DURATION=5 min
```

### Soak final recomendado

El mayor punto estable elegido para uso recomendado se ejecutará durante al menos:

```text
DURATION=30 min
```

El soak puede ampliarse si aparece jitter, reconexión o interacción con otros periféricos.

## Escenarios

### PERF-S1 — JWPLC Server aislado a nivel de aplicación

```text
PC Master -> JWPLC Modbus TCP Server
persistent TCP connection
```

Objetivo: medir el techo del Server y del parser/respuesta sobre W5500.

### PERF-C1 — JWPLC Client aislado a nivel de aplicación

```text
JWPLC Modbus TCP Client -> PC Server
persistent TCP connection
```

Objetivo: medir el techo de la state machine Client cooperativa.

### PERF-S2 / PERF-C2 — Runtime normal JWPLC

Repetir los puntos representativos manteniendo el runtime normal y periféricos integrados. No retirar periféricos del autoload por velocidad.

Se observarán al menos:

```text
TFT
RTC
FRAM
SD
Ethernet
buttons/runtime
```

### PERF-COEX — Carga industrial simultánea

Se ejecutará después de validar la coexistencia funcional A14.4:

```text
Modbus RTU activo
Modbus TCP activo
TFT activo
RTC activo
FRAM activo
SD activo
```

El objetivo no es maximizar throughput bruto, sino comprobar cuánto rendimiento TCP sostenible queda disponible sin afectar la estabilidad general.

## Criterio de punto estable

Un punto se considera `STABLE_PASS` sólo si durante la ventana completa cumple:

```text
unexpected_resets = 0
timeouts = 0
protocol_errors = 0
transport_errors = 0
unplanned_reconnects = 0
spi_bus_lock_timeouts = 0
achieved_req_s >= 95% de requested_req_s
```

La latencia p95/p99 y el máximo de service/loop gap deben quedar registrados; si crecen de forma abrupta al aumentar un escalón, ese comportamiento se marcará como inicio de saturación aunque todavía no existan errores.

Para el modo `sin espera`, donde no existe una tasa solicitada fija, se reportará sólo el máximo throughput observado y no se clasificará automáticamente como tasa recomendada.

## Resultados que deben quedar al cierre de Alpha14

```text
SERVER_MAX_STABLE_REQ_S
SERVER_MAX_PEAK_REQ_S
CLIENT_MAX_STABLE_REQ_S
CLIENT_MAX_PEAK_REQ_S
SERVER_RECOMMENDED_POLL_INTERVAL_MS
CLIENT_RECOMMENDED_POLL_INTERVAL_MS
FC03_125REG_MAX_STABLE_REQ_S
FC16_123REG_MAX_STABLE_REQ_S
FULL_RUNTIME_MAX_STABLE_REQ_S
RTU_TCP_COEX_MAX_STABLE_REQ_S
```

También debe existir una tabla completa de resultados con tasa, tamaño, latencias, errores y observaciones.

## Notas de interpretación

- La velocidad de enlace Ethernet del W5500 no equivale al throughput Modbus TCP.
- El SPI compartido, el tamaño de ADU, el tiempo de procesamiento y los demás periféricos influyen en el límite útil.
- No se publicará un valor de rendimiento máximo hasta tener evidencia física repetible.
- El valor recomendado para aplicaciones industriales será deliberadamente inferior al límite de saturación si la frontera presenta jitter elevado.
