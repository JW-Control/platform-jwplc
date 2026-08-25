# JWPLC Modbus RTU — Master no bloqueante

## Estado

- Etapa: `v2.1.0-alpha.6`
- Rama: `v2.1.0-alpha.6/feature/modbus-rtu-nonblocking-master`
- Objetivo: agregar un motor Master Modbus RTU no bloqueante sin romper las APIs síncronas ya validadas.
- Estado actual: primera implementación FC03/FC06 completada en source; pendiente compilación y validación física.

## Motivación

El lado Slave/Server de `JWPLC_ModbusRTU` ya usa `task()/poll()` de forma cooperativa. En cambio, las operaciones Master heredadas (`readHoldingRegisters()` y `writeSingleRegister()`) esperan la respuesta dentro de la misma llamada y pueden bloquear hasta `timeoutMs`.

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

## API implementada en la primera iteración

```cpp
bool beginReadHoldingRegistersAsync(...);
bool beginWriteSingleRegisterAsync(...);
void task();

bool masterBusy() const;
bool masterDone() const;
bool masterSucceeded() const;
JWPLCModbusMasterState masterState() const;
JWPLCModbusRTUError masterResult() const;
void clearMasterResult();
```

Se agregan además los errores:

```cpp
JWPLC_MODBUS_BUSY
JWPLC_MODBUS_TRANSPORT_ERROR
```

Los valores anteriores del enum de errores se conservan en el mismo orden; los nuevos códigos se agregan al final.

## Máquina de estados implementada

```text
IDLE
  -> WAIT_RESPONSE
      -> DONE
      -> ERROR
```

`TIMEOUT`, `CRC_ERROR`, `EXCEPTION`, `INVALID_RESPONSE`, `BUFFER_OVERFLOW` y `TRANSPORT_ERROR` se reflejan como resultado de la transacción y llevan el estado a `ERROR`.

La transmisión inicial ocurre al iniciar la operación. `task()` recolecta bytes y detecta fin de trama mediante el frame gap, valida CRC/Slave ID/Function Code y completa la transacción sin esperar en un bucle activo hasta el timeout.

## Alcance exacto de "no bloqueante"

La espera de respuesta RTU deja de bloquear el `loop()` durante decenas, cientos o miles de milisegundos. El runtime debe llamar `JWPLC_ModbusRTU.task()` con frecuencia para avanzar la transacción.

El transporte actual `JWPLC_RS485.write()` todavía ejecuta `HardwareSerial::flush()` para completar físicamente la transmisión antes de retornar. Por tanto, esta primera iteración elimina el bloqueo de **espera de respuesta/timeout**, pero no pretende todavía convertir la serialización TX de unos pocos bytes en DMA/asíncrona completa.

Para Remote I/O a 115200 8N1 ese tiempo de transmisión es acotado y muy inferior al timeout de respuesta; aun así debe medirse antes de fijar `Tplc`/`Tremote` finales.

## Compatibilidad

Las APIs síncronas existentes se conservan:

```cpp
readHoldingRegisters(...)
writeSingleRegister(...)
```

No han sido eliminadas ni renombradas. Mientras una operación asíncrona está activa, las APIs síncronas rechazan una nueva transacción con `JWPLC_MODBUS_BUSY` para evitar corrupción del bus compartido.

El lado Slave/Server sigue usando el mismo `task()/poll()` y los handlers FC03/FC06/FC16 existentes.

## Source fallback durante desarrollo

Alpha5 distribuye `JWPLC_ModbusRTU` con `precompiled=full` y un archive ESP32. Ese archive todavía contiene la implementación anterior y no incluye los nuevos símbolos asíncronos.

Durante el desarrollo de Alpha6 se retiró temporalmente `precompiled=full` de `library.properties` para forzar compilación desde source y evitar enlazar accidentalmente el archive obsoleto.

Antes de cerrar/publicar Alpha6 se debe:

1. validar funcionalmente la nueva implementación desde source;
2. regenerar `src/esp32/libJWPLC_ModbusRTU.a` con la herramienta reproducible del proyecto;
3. auditar símbolos/ABI;
4. restaurar `precompiled=full` solo si el archive regenerado pasa los gates.

## Ejemplo de validación agregado

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/examples/
└── ModbusRTU_Master_NonBlocking/
    └── ModbusRTU_Master_NonBlocking.ino
```

El ejemplo inicia FC03 de forma asíncrona y mantiene un contador de aplicación (`appTicks`) que debe continuar incrementándose incluso cuando el Slave está desconectado y la transacción termina por timeout.

## Gate inicial

Antes de integrar Remote I/O sobre este motor deben pasar:

- compilación Arduino CLI;
- compilación Arduino IDE;
- FC03 async Master ↔ Slave;
- FC06 async Master ↔ Slave;
- timeout sin bloqueo del `loop()`;
- recuperación después de reconexión;
- reset de Slave durante transacción;
- reset de Master;
- prueba con contador de ciclos de aplicación durante timeout;
- cero regresiones en APIs síncronas existentes;
- cero regresiones en Slave `task()/poll()`.

## No incluido todavía

- FC01/FC02/FC05/FC15 dentro de la API base;
- scheduler multi-slave;
- Process Image;
- OpenPLC Backplane;
- watchdog de salidas Remote I/O;
- cambios de baudrate final;
- cambios de `platform.txt`, flash, particiones u OTA.
