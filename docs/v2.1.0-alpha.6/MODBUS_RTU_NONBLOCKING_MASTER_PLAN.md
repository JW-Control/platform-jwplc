# JWPLC Modbus RTU — Master no bloqueante

## Estado

- Etapa: `v2.1.0-alpha.6`
- Rama: `v2.1.0-alpha.6/feature/modbus-rtu-nonblocking-master`
- Objetivo: agregar un motor Master Modbus RTU no bloqueante sin romper las APIs síncronas ya validadas.

## Motivación

El lado Slave/Server de `JWPLC_ModbusRTU` ya usa `task()/poll()` de forma cooperativa. En cambio, las operaciones Master actuales (`readHoldingRegisters()` y `writeSingleRegister()`) esperan la respuesta dentro de la misma llamada y pueden bloquear hasta `timeoutMs`.

Remote I/O necesita que el runtime PLC pueda seguir ejecutándose mientras la comunicación RTU avanza en segundo plano.

## Principios de diseño

1. Mantener compatibilidad con Arduino IDE.
2. No romper APIs existentes.
3. Conservar `JWPLC_RS485` como transporte.
4. No cambiar todavía el contrato físico validado de RS-485.
5. No introducir tareas FreeRTOS obligatorias en esta primera etapa.
6. Implementar un motor cooperativo dirigido por `task()/poll()`.
7. Permitir una sola transacción Master activa por instancia en esta primera versión.
8. Separar claramente inicio, progreso, finalización, timeout y error.

## API objetivo mínima

Se agregará un estado de transacción Master y operaciones asíncronas mínimas:

```cpp
bool beginReadHoldingRegistersAsync(...);
bool beginWriteSingleRegisterAsync(...);
void task();

bool masterBusy() const;
bool masterDone() const;
bool masterSucceeded() const;
JWPLCModbusRTUError masterResult() const;
void clearMasterResult();
```

Los nombres definitivos se validarán contra el código antes de cerrar la etapa.

## Máquina de estados prevista

```text
IDLE
  -> WAIT_RESPONSE
      -> DONE_OK
      -> DONE_ERROR
      -> TIMEOUT
```

La transmisión inicial ocurre al iniciar la operación. `task()` recolecta bytes y detecta fin de trama mediante el frame gap, valida CRC/Slave ID/Function Code y completa la transacción sin esperar en bucles activos.

## Compatibilidad

Las APIs síncronas existentes se conservarán. En esta etapa podrán seguir usando su implementación actual; una migración posterior para convertirlas en wrappers del motor no bloqueante solo se hará después de validar equivalencia funcional.

## Gate inicial

Antes de integrar Remote I/O sobre este motor deben pasar:

- compilación Arduino CLI;
- compilación Arduino IDE;
- FC03 async Master ↔ Slave;
- FC06 async Master ↔ Slave;
- timeout sin bloqueo del `loop()`;
- recuperación después de reconexión;
- prueba con contador de ciclos de aplicación durante timeout;
- cero regresiones en APIs síncronas existentes.

## No incluido todavía

- FC01/FC02/FC05/FC15 dentro de la API base;
- scheduler multi-slave;
- Process Image;
- OpenPLC Backplane;
- watchdog de salidas Remote I/O;
- cambios de baudrate final;
- cambios de `platform.txt`, flash, particiones u OTA.
