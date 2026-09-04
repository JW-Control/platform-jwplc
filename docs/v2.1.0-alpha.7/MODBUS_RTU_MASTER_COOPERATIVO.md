# Alpha7 — decisión Modbus RTU multidrop y Master cooperativo

## Estado

Este documento registra la decisión técnica tomada durante Alpha7 antes de retomar los gates de OpenPLC/Backplane/Remote I/O.

## 1. Problema multidrop observado

Topología de validación:

```text
M2 ---- S1 ---- S2
```

Con el parser previo, S1 y S2 acumulaban errores CRC aunque las transacciones de aplicación continuaban funcionando.

La instrumentación de source demostró que los supuestos frames CRC inválidos contenían en realidad dos ADU válidas acumuladas en el mismo buffer UART. Ejemplos observados:

```text
FC06: 16 bytes = request de 8 + response de 8
FC03: 25 bytes = request de 8 + response de 17
```

Cada ADU individual tenía CRC correcto.

### Causa raíz

`poll()` registraba el tiempo cuando el software leía los bytes del UART. Si request y response de otro nodo ya estaban pendientes en el FIFO cuando se ejecutaba `poll()`, ambas terminaban concatenadas en `_rxBuffer` y se validaban como una sola trama.

```text
ROOT_CAUSE=FOREIGN_REQUEST_RESPONSE_CONCATENATION
```

## 2. Corrección de framing

Se descartó un primer splitter basado en buscar el primer prefijo con CRC válido. Aunque eliminó los falsos CRC, una coincidencia CRC accidental en un prefijo corto produjo una excepción Modbus espuria.

La corrección aceptada obtiene la longitud desde la estructura del protocolo y utiliza CRC únicamente para validar esa longitud:

```text
FC01/02/03/04 request  = 8
FC01/02/03/04 response = 5 + byteCount
FC05/06 request/resp   = 8
FC0F/10 request        = 9 + byteCount
FC0F/10 response       = 8
Exception response     = 5
```

Commit base del fix:

```text
3eac5b3 fix(modbus-rtu): separar ADUs multidrop por estructura
```

## 3. Resultado físico

Con el framing estructural cargado en ambos Slaves y una prueba prolongada M2 + S1 + S2:

```text
S1 CRC=0
S1 EXCEPTIONS=0
S2 CRC=0
S2 EXCEPTIONS=0
M2 S1 W/R/V=0/0/0
M2 S2 W/R/V=0/0/0
S1 ONLINE=YES
S2 ONLINE=YES
```

La causa raíz del falso CRC multidrop queda cerrada desde source.

## 4. Política del Master desde Alpha7

Para un PLC no es aceptable que la pérdida de un Remote I/O congele el ciclo de aplicación durante cientos o miles de milisegundos. Por ello, el Master Modbus RTU pasa a ser cooperativo/no bloqueante por defecto.

### API principal

```cpp
requestReadHoldingRegisters(...)
requestWriteSingleRegister(...)
task()
masterBusy()
masterDone()
masterSucceeded()
masterResult()
clearMasterResult()
```

`request...()` retorna cuando la solicitud fue aceptada e iniciada. La transacción se completa posteriormente mediante llamadas frecuentes a `task()`.

### API bloqueante explícita

```cpp
readHoldingRegistersSync(...)
writeSingleRegisterSync(...)
```

Se reserva para commissioning, pruebas o aplicaciones simples donde bloquear sea aceptable.

Los nombres históricos sin sufijo se mantienen temporalmente como wrappers Sync durante Alpha7 para no romper ejemplos/rescates mientras se completa la migración. No deben usarse en código nuevo.

## 5. Contrato cooperativo

- `task()` debe ejecutarse con alta frecuencia;
- solo puede existir una transacción Master activa a la vez;
- el buffer destino de FC03 debe seguir válido hasta `masterDone()`;
- no iniciar otra transacción mientras `masterBusy()` sea verdadero;
- tras `masterDone()`, revisar resultado y llamar `clearMasterResult()`;
- OpenPLC, Backplane y Remote I/O utilizarán exclusivamente la ruta cooperativa.

## 6. Precompilación

El archive `libJWPLC_ModbusRTU.a` anterior queda temporalmente fuera durante la validación del nuevo motor porque no contiene los símbolos ni el parser Alpha7.

Antes del cierre de Alpha7 se debe:

1. regenerar `src/esp32/libJWPLC_ModbusRTU.a`;
2. auditar miembros y símbolos;
3. restaurar `precompiled=full`;
4. confirmar que Arduino IDE enlaza el archive nuevo;
5. repetir multidrop y recuperación usando el archive final.

## 7. Gates siguientes

```text
7NB  validar Master cooperativo FC03/FC06 y timeout sin bloquear
7G   desconexión/reconexión y reset de nodos
7H   revisar coherencia de estadísticas Master Sync/cooperativo
7I   regenerar/auditar archive precompilado
7J   repetir multidrop con archive final
```

Solo después se retoman los gates físicos OpenPLC/Backplane/Remote I/O.
