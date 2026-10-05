# Alpha14 — A14.3 FC03/125 referencia TCP-only de 30 minutos

Fecha: `2026-09-12`

## Objetivo

Validar una referencia operativa redonda y conservadora para comparar posteriormente el rendimiento de Modbus TCP frente al runtime con periféricos activos.

Esta prueba **no** redefine el techo máximo estable del servidor. El candidato de frontera a 5 minutos continúa siendo `1140 req/s`, con `1150 req/s` clasificado como saturación limpia. Para la comparación de carga real se adopta `1000 req/s` por disponer de holgura frente a la frontera.

## Configuración

```text
FUNCTION=FC03
QUANTITY_REGISTERS=125
REQUESTED_REQ_S=1000
DURATION_S=1800
REFERENCE_CRITERION=CLEAN_AND_ACHIEVED_GTE_99.5_PERCENT
EXPECTED_TCP_TOTAL_PAYLOAD_MBPS=2.168
EXPECTED_USEFUL_DATA_MBPS=2.000
```

## Resultado físico

```text
REFERENCE_REQUESTED_REQ_S=1000
REFERENCE_ACHIEVED_REQ_S=1000.00
REFERENCE_ACHIEVED_PCT=100.0000
REFERENCE_REQUESTS_EXPECTED=1800000
REFERENCE_REQUESTS_OK=1800000
REFERENCE_REQUEST_DEFICIT=0

REFERENCE_TOTAL_MBPS=2.1680
REFERENCE_USEFUL_MBPS=2.0000

REFERENCE_LATENCY_AVG_US=843.8
REFERENCE_LATENCY_P95_US=1182.8
REFERENCE_LATENCY_P99_US=1263.8
REFERENCE_LATENCY_MAX_US=29496.4

REFERENCE_LOOP_GAP_AVG_US=232
REFERENCE_LOOP_GAP_MAX_US=5268

REFERENCE_CONNECT_ATTEMPTS=1
REFERENCE_CLEAN=YES
REFERENCE_TIMEOUTS=0
REFERENCE_TRANSPORT_ERRORS=0
REFERENCE_PROTOCOL_ERRORS=0
REFERENCE_BUS_LOCK_TIMEOUTS=0
REFERENCE_CROSS_COUNT_PASS=YES

TCP_ONLY_30MIN_REFERENCE_PASS=YES
A14_3_FC03_125_30MIN_TCP_ONLY_REFERENCE=PASS_PHYSICAL
```

## Interpretación

Durante 30 minutos se completaron exactamente `1,800,000 / 1,800,000` transacciones FC03 de 125 registros, sin déficit de solicitudes y sin errores de transporte, protocolo o mutex SPI.

La referencia operativa para la comparación posterior queda fijada en:

```text
TCP_ONLY_OPERATIONAL_REFERENCE_REQ_S=1000
TCP_ONLY_OPERATIONAL_REFERENCE_TOTAL_MBPS=2.168
TCP_ONLY_OPERATIONAL_REFERENCE_USEFUL_MBPS=2.000
```

Los valores de Mbps anteriores representan payload de aplicación TCP/Modbus y datos útiles Modbus, no la velocidad física del enlace Ethernet.

## Estado de frontera

```text
MAX_5MIN_STABLE_CANDIDATE_REQ_S=1140
RATE_1150_5MIN=SATURATION_FAIL_CLEAN
SERVER_MAX_STABLE_REQ_S=NOT_FINAL_YET
```

La referencia de `1000 req/s` se utiliza deliberadamente como punto de operación con holgura para comparar en igualdad de condiciones contra `FULL_RUNTIME_REALISTIC` y `FULL_RUNTIME_STRESS`.

## Siguiente paso

```text
NEXT=A14.3_FULL_RUNTIME_REALISTIC_PREP
FULL_RUNTIME_PERIPHERALS=NOT_EXECUTED
```

El siguiente benchmark deberá mantener el mismo tráfico FC03/125 a `1000 req/s` mientras se ejercitan de forma simultánea TFT, FRAM, microSD, RTC, botonera y TCA/I/O, midiendo además frescura de servicios y contención del SPI compartido.
