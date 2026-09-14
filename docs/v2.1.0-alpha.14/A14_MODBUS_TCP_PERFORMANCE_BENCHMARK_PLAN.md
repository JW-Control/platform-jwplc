# Alpha14 — Benchmark de rendimiento Modbus TCP

Fecha: `2026-09-12`

## Objetivo

Medir el rendimiento real y sostenible de `JWPLC_ModbusTCP` sobre el W5500 integrado del JWPLC Basic, sin confundir la velocidad física Ethernet con la frecuencia útil de transacciones Modbus.

El objetivo principal de Alpha14 queda expresado en dos unidades complementarias:

```text
req/s = frecuencia de transacciones Modbus TCP
Mbps  = throughput útil transportado dentro de TCP
```

El resultado debe responder cuatro preguntas distintas:

```text
MAX_STABLE_REQUEST_RATE = mayor tasa sostenida sin errores ni degradación material
MAX_PEAK_REQUEST_RATE   = máximo throughput observado en saturación
MAX_STABLE_TCP_MBPS     = mayor throughput TCP payload sostenido y repetible
MAX_PEAK_TCP_MBPS       = máximo throughput TCP payload observado en saturación
```

No se eliminarán periféricos del autoload ni se modificará la arquitectura normal del JWPLC únicamente para mejorar el benchmark.

## Definición de Mbps

Para evitar ambigüedad, se registrarán tres métricas separadas:

```text
TCP_TX_PAYLOAD_MBPS
TCP_RX_PAYLOAD_MBPS
TCP_TOTAL_PAYLOAD_MBPS
```

`TCP_TOTAL_PAYLOAD_MBPS` será la suma de bytes de aplicación entregados a TCP en ambas direcciones, multiplicada por 8 y dividida entre el tiempo real de la ventana.

También se registrará:

```text
MODBUS_USEFUL_DATA_MBPS
```

Esta última métrica representa únicamente los datos de proceso transportados por la función Modbus, excluyendo MBAP, function code, byte count y demás overhead de protocolo.

Estas cifras no se presentarán como velocidad física de enlace Ethernet ni como throughput on-wire completo. Si se requiere posteriormente una referencia de W5500/TCP crudo sin Modbus, se hará como benchmark separado.

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
tcp_tx_payload_bytes_s
tcp_rx_payload_bytes_s
tcp_total_payload_bytes_s
tcp_tx_payload_mbps
tcp_rx_payload_mbps
tcp_total_payload_mbps
modbus_useful_data_bytes_s
modbus_useful_data_mbps
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

Si el sweep corto alcanza `1000 req/s` sin localizar frontera, se ejecutará primero `sin espera` para medir el techo natural y luego se elegirán escalones adicionales alrededor del límite observado.

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

### Qualification sweep

Antes del benchmark formal se permiten ventanas cortas de:

```text
DURATION=5 s
```

Su objetivo es localizar la frontera aproximada. Un `PASS` de 5 s no se publicará como tasa estable final.

### Saturación exploratoria

Cuando no se encuentre frontera hasta `1000 req/s`, se ejecutará `sin espera` durante al menos:

```text
DURATION=10 s
```

por tamaño representativo, para medir throughput pico y elegir los siguientes escalones.

### Sweep formal

Cada combinación elegida alrededor de la frontera:

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

Objetivo: medir el techo del Server y del parser/respuesta sobre W5500 con el autoload normal del package, pero sin carga de aplicación añadida sobre los demás periféricos.

Resultados principales:

```text
SERVER_MAX_STABLE_REQ_S
SERVER_MAX_PEAK_REQ_S
SERVER_MAX_STABLE_TCP_MBPS
SERVER_MAX_PEAK_TCP_MBPS
```

### PERF-C1 — JWPLC Client aislado a nivel de aplicación

```text
JWPLC Modbus TCP Client -> PC Server
persistent TCP connection
```

Objetivo: medir el techo de la state machine Client cooperativa.

Resultados principales:

```text
CLIENT_MAX_STABLE_REQ_S
CLIENT_MAX_PEAK_REQ_S
CLIENT_MAX_STABLE_TCP_MBPS
CLIENT_MAX_PEAK_TCP_MBPS
```

### PERF-S2 / PERF-C2 — Runtime integrado con carga de aplicación

Después de medir el techo TCP aislado se repetirá el benchmark con una carga simultánea deliberadamente exigente pero segura y representativa de una aplicación real JWPLC.

No se retirará ningún periférico del autoload.

El perfil integrado deberá mantener actividad observable en:

```text
TFT
RTC
FRAM
microSD
botonera
TCA/I/O
Ethernet / W5500
```

La carga integrada seguirá estas reglas:

- TFT: actualización periódica real de contenido dinámico; no se usará un `fillScreen()` infinito artificial como única carga.
- FRAM: ciclos de lectura/escritura/verificación sobre una zona reservada exclusivamente al benchmark.
- microSD: escritura periódica de bloques de log y lectura/verificación, con sincronización controlada; no se reutilizará un archivo del usuario.
- RTC: lectura periódica normal.
- botonera: conservar el servicio normal y registrar su freshness/edad; no se requerirá pulsación humana continua para que el benchmark sea válido.
- TCA/I/O: mantener lectura/servicio frecuente y medir freshness; no se conmutarán relés mecánicos a alta frecuencia para generar carga artificial.
- SPI compartido: registrar timeouts y latencias de ownership.

Se definirán dos perfiles:

```text
FULL_RUNTIME_REALISTIC
FULL_RUNTIME_STRESS
```

`FULL_RUNTIME_REALISTIC` representará una aplicación normal activa.

`FULL_RUNTIME_STRESS` elevará las frecuencias de TFT, FRAM, SD y TCA dentro de límites seguros para medir cuánto throughput TCP queda disponible bajo presión de periféricos.

El resultado comparará explícitamente:

```text
TCP_ONLY_MAX_STABLE_MBPS
FULL_RUNTIME_REALISTIC_MAX_STABLE_MBPS
FULL_RUNTIME_STRESS_MAX_STABLE_MBPS
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
botonera / TCA activos
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

En los escenarios integrados también deberán permanecer sanos los contadores/freshness de los periféricos activados.

La latencia p95/p99 y el máximo de service/loop gap deben quedar registrados; si crecen de forma abrupta al aumentar un escalón, ese comportamiento se marcará como inicio de saturación aunque todavía no existan errores.

Para el modo `sin espera`, donde no existe una tasa solicitada fija, se reportará sólo el máximo throughput observado y no se clasificará automáticamente como tasa recomendada.

## Resultados que deben quedar al cierre de Alpha14

```text
SERVER_MAX_STABLE_REQ_S
SERVER_MAX_PEAK_REQ_S
SERVER_MAX_STABLE_TCP_MBPS
SERVER_MAX_PEAK_TCP_MBPS
CLIENT_MAX_STABLE_REQ_S
CLIENT_MAX_PEAK_REQ_S
CLIENT_MAX_STABLE_TCP_MBPS
CLIENT_MAX_PEAK_TCP_MBPS
SERVER_RECOMMENDED_POLL_INTERVAL_MS
CLIENT_RECOMMENDED_POLL_INTERVAL_MS
FC03_125REG_MAX_STABLE_REQ_S
FC03_125REG_MAX_STABLE_TCP_MBPS
FC16_123REG_MAX_STABLE_REQ_S
FC16_123REG_MAX_STABLE_TCP_MBPS
TCP_ONLY_MAX_STABLE_MBPS
FULL_RUNTIME_REALISTIC_MAX_STABLE_MBPS
FULL_RUNTIME_STRESS_MAX_STABLE_MBPS
FULL_RUNTIME_MAX_STABLE_REQ_S
RTU_TCP_COEX_MAX_STABLE_REQ_S
RTU_TCP_COEX_MAX_STABLE_MBPS
```

También debe existir una tabla completa de resultados con tasa, tamaño, Mbps, latencias, errores y observaciones.

## Notas de interpretación

- La velocidad de enlace Ethernet del W5500 no equivale al throughput Modbus TCP.
- El SPI compartido, el tamaño de ADU, el tiempo de procesamiento y los demás periféricos influyen en el límite útil.
- No se publicará un valor de rendimiento máximo hasta tener evidencia física repetible.
- El valor recomendado para aplicaciones industriales será deliberadamente inferior al límite de saturación si la frontera presenta jitter elevado.
- El benchmark aislado y el benchmark integrado se reportarán por separado; no se mezclará el techo del protocolo con el rendimiento disponible bajo una carga de aplicación real.
