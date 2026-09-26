# Alpha14 — A14.3 referencia operativa TCP-only

Fecha: `2026-09-12`

## Decisión

Para comparar de forma limpia el rendimiento TCP-only contra los perfiles de carga integrada de periféricos se fija una referencia operativa conservadora de:

```text
TCP_ONLY_OPERATIONAL_REFERENCE_REQ_S=1000
FC03_QUANTITY_REGISTERS=125
TCP_TOTAL_PAYLOAD_MBPS_THEORETICAL=2.168
MODBUS_USEFUL_DATA_MBPS_THEORETICAL=2.000
```

Esta referencia **no sustituye** la búsqueda del techo máximo estable del servidor. El estado de caracterización de frontera permanece:

```text
RATE_1140_5MIN=STABLE_PASS
RATE_1150_5MIN=SATURATION_FAIL_CLEAN
MAX_5MIN_STABLE_CANDIDATE_REQ_S=1140
SERVER_MAX_STABLE_REQ_S=NOT_FINAL_YET
```

## Motivo

El punto de 1000 req/s se elige como referencia operativa por ser redondo, reproducible y disponer de holgura respecto al candidato de 1140 req/s confirmado durante 5 min.

La holgura respecto a 1140 req/s es aproximadamente:

```text
(1140 - 1000) / 1140 = 12.28 %
```

Esto permite distinguir dos conceptos:

- **techo de rendimiento**: máximo estable que finalmente pueda cerrarse para FC03/125;
- **referencia operativa**: 1000 req/s usada para comparar TCP-only frente a `FULL_RUNTIME_REALISTIC` y `FULL_RUNTIME_STRESS`.

## Siguiente gate

Ejecutar:

```text
FC03/125
1000 req/s
30 min
TCP-only
```

Si el ensayo permanece limpio y sostiene el 100 % de la tasa solicitada, se usará como baseline físico para los perfiles integrados de TFT, RTC, FRAM, microSD, botonera y TCA/I/O.
