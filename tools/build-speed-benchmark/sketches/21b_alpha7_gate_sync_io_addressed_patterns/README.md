# Gate 7NB.3A-B — I/O sincronizado direccionado

Objetivo: validar que M2 gobierne patrones de salidas en M2/S1/S2 sin usar broadcast Modbus RTU y que la activacion/desactivacion ocurra practicamente al mismo tiempo.

## Modelo

Cada paso se divide en dos fases:

1. M2 escribe por FC06, de forma direccionada, la mascara de salidas deseada en S1 y S2.
2. M2 fija un instante futuro comun y arma cada Slave con un delay relativo calculado justo antes de enviar su trigger.
3. S1 y S2 esperan localmente hasta su deadline y actualizan el banco completo de salidas.
4. M2 actualiza su propio banco en el target comun.

La actualizacion local usa `JWPLC_writeOutputs(bitmap)` para escribir el banco Q0_0..Q0_7 como una imagen de 8 bits.

No se modifica `JWPLC_ModbusRTU` en este gate; solo se consume la API cooperativa `requestWriteSingleRegister()` ya disponible.

## Uso

Cargar el mismo sketch en las tres placas.

- S1: `ROLE S1`
- S2: `ROLE S2`
- M2: `ROLE M2`

En M2:

- `START`: ejecuta una secuencia visual con chase, alternancias, olas y figuras.
- `CLACK`: alterna `0x00/0xFF` en las tres placas para escuchar el golpe conjunto de los 24 relays.
- `STATUS`: muestra estado del gate, transacciones y CRC.
- `STOP`: detiene y apaga las salidas locales.

## Criterio inicial

PASS si:

- no aparecen `SYNC_TX_FAIL`;
- `syncFail=0`;
- CRC permanece en 0;
- los cambios mecanicos se perciben como un unico clack o sin desfase claramente audible;
- las secuencias de mascaras son coherentes en M2/S1/S2.

Si aun existe desfase audible, el siguiente paso es instrumentar el skew real de aplicacion y ajustar la compensacion `ARM_ONE_WAY_COMP_US`, sin tocar el parser Modbus RTU.
