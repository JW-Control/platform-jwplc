# A14.2 — Compile gate Client FC04 / FC05

Fecha: `2026-09-11`

## Contexto

Rama:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

Incremento Client/Master sobre la state machine ya validada físicamente para FC03/FC06:

```text
FC04 — Read Input Registers
FC05 — Write Single Coil
```

## Evidencia Arduino CLI

```text
PLATFORM=jwplc_local:esp32 2.1.0-dev
SERVER_REGRESSION_EXIT=0
CLIENT_FC04_FC05_COMPILE_EXIT=0
A14_2_SERVER_AFTER_FC04_FC05=PASS
A14_2_CLIENT_FC04_FC05_COMPILE=PASS
```

## Conclusión

```text
A14_2_CLIENT_FC04_FC05_SOURCE=PASS
A14_2_CLIENT_FC04_FC05_COMPILE=PASS
SERVER_REGRESSION_AFTER_FC04_FC05=PASS
A14_2_CLIENT_FC04_FC05_RUNTIME=NOT_EXECUTED
```

El compile gate no sustituye la validación runtime. El siguiente gate debe comprobar FC04 y FC05 físicamente sobre una conexión TCP persistente, con Transaction ID incremental y sin errores de transporte/protocolo/SPI.
