# Alpha12 — P8A Modbus TCP Client API/Implementation Closure

Fecha: 2026-10-04

## Resultado

```text
GATE=P8A_MODBUS_TCP_CLIENT_API_IMPLEMENTATION_CLOSURE
RESULT=PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HEADER_IMPLEMENTATION_MISMATCH=NO
PUBLIC_API_CHANGED=NO
```

## Hallazgo corregido

La auditoría previa había concluido que el header declaraba FC01/02/04/05/15/16 sin implementación completa.

La revisión del árbol completo de `JWPLC_ModbusTCP/src` demostró que esa conclusión era un falso positivo: las implementaciones existían en tres translation units auxiliares creados durante el desarrollo incremental:

```text
JWPLC_ModbusTCP_Client_FC01_FC02.cpp
JWPLC_ModbusTCP_Client_FC04_FC05.cpp
JWPLC_ModbusTCP_Client_FC15_FC16.cpp
```

Por tanto:

```text
FALSE_MISSING_IMPLEMENTATION_REPORT=YES
ACTUAL_MISSING_FUNCTIONS=0
```

## API Client verificada

La API pública preservada contiene:

```text
FC01 requestReadCoils()
FC02 requestReadDiscreteInputs()
FC03 requestReadHoldingRegisters()
FC04 requestReadInputRegisters()
FC05 requestWriteSingleCoil()
FC06 requestWriteSingleRegister()
FC15 requestWriteMultipleCoils()
FC16 requestWriteMultipleRegisters()
```

## Consolidación

Para reducir fragmentación y evitar futuras auditorías parciales, las implementaciones del Client fueron consolidadas en:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp
```

Se eliminaron los tres translation units auxiliares.

No se modificaron las firmas públicas ni se introdujo una nueva API.

Source HEAD consolidado:

```text
ab4379a177492ee851c1afccf5398e649c55dd4f
```

## Prueba compile/link

Se añadió:

```text
tools/alpha12/p8a_modbus_tcp_client_api_smoke/p8a_modbus_tcp_client_api_smoke.ino
```

El sketch referencia las ocho operaciones públicas del Client para obligar a Arduino a compilar y enlazar todas sus definiciones.

Resultado reportado en entorno local JWPLC:

```text
FQBN=jwplc_local:esp32:jwplcbasic
COMPILE_EXIT=0
COMPILE_LINK_SMOKE=PASS
```

El aviso de actualización de Arduino CLI no forma parte del resultado del gate. No se adopta una versión RC durante el cierre de Alpha12.

## Impacto sobre freeze/benchmark

`JWPLC_ModbusTCP` permanece source-only en Alpha12. No se regeneran archives precompilados de otras librerías.

Sin embargo, la consolidación cambia source/package y el número de translation units, por lo que antes del release se deberá repetir:

```text
P7_RELEASE_LIKE_VALIDATION
FINAL_BUILD_SPEED_BENCHMARK
```

La regresión mínima física Modbus TCP Server/Client permanece en los gates finales del HEAD congelado y no se adelanta a P8A.

## Cierre

```text
P8A_STATIC_API_AUDIT=PASS
P8A_COMPILE_LINK_SMOKE=PASS
P8A_STATUS=PASS
NEXT=P8_EXAMPLES_API_REVIEW
```
