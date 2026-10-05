# A14.2 — Client FC15/FC16 compile

Fecha: `2026-09-11`

## Resultado

```text
SERVER_REGRESSION_EXIT=0
CLIENT_FC15_FC16_COMPILE_EXIT=0
A14_2_SERVER_AFTER_FC15_FC16_SOURCE=PASS
A14_2_CLIENT_FC15_FC16_COMPILE=PASS
FC15_FC16_PARSER_RUNTIME=NOT_IMPLEMENTED_YET
MBLOCK_POC_TOUCHED=NO
```

## Build del probe Client completo

```text
PROGRAM_BYTES=405349
PROGRAM_PERCENT=9
GLOBAL_BYTES=29044
GLOBAL_PERCENT=8
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
```

El probe referencia FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16 en el mismo sketch. El ejemplo Server también compila sin regresión.

## Alcance validado

- API pública FC15/FC16 compila.
- Construcción de request FC15/FC16 compila.
- FC15 limita `quantity` a `1..1968` bits y usa `sourcePacked` LSB-first.
- FC16 limita `quantity` a `1..123` registros.
- El parser de respuesta FC15/FC16 todavía no se considera implementado/validado en runtime.
