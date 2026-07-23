# JWPLC Remote I/O RTU - Checklist PoC 1

## Estado de cierre

**PoC 1 validada en hardware real.**

La validación confirma que un **JWPLC Basic** puede operar como **Remote I/O Slave Modbus RTU** con ID fijo, exponiendo entradas digitales por `FC2`, feedback de salidas por `FC1` y escritura de salidas por `FC5`/`FC15`.

## Objetivo de la PoC 1

Validar un **JWPLC Basic Remote I/O Slave** con ID fijo por Modbus RTU / RS-485.

La PoC 1 no incluye FRAM ni commissioning. Primero se valida que el bus, el mapa y el I/O físico funcionen.

## Identificación

- [x] Confirmar branch base: `feature/v2.1.0-alpha.3-remote-io-rtu-poc1`.
- [x] Confirmar versión/alpha: `v2.1.0-alpha.3`.
- [x] Confirmar nombre del ejemplo o firmware: `JWPLC_RemoteIO_Slave_RTU`.
- [x] Confirmar placa/FQBN usada: `jwplc:esp32:jwplcbasic`.
- [x] Confirmar hardware físico usado como slave: JWPLC Basic.
- [x] Confirmar herramienta usada como master de prueba: QModMaster con adaptador USB-RS485.

## Configuración inicial validada

```txt
Slave ID: 2
Baudrate: 115200
Formato: 8N1
Puerto: Serial2
RX2: IO16
TX2: IO17
Driver: MAX13487 auto-dirección
Transporte usado por el ejemplo: JWPLC_RS485.begin(115200, SERIAL_8N1)
Helpers Modbus usados: JWPLC_ModbusRTU.checkCRC() / JWPLC_ModbusRTU.appendCRC()
```

## Compilación y subida

- [x] Compila en Arduino IDE.
- [ ] Compila con Arduino CLI.
- [x] Sube correctamente al JWPLC Basic slave.
- [x] Monitor serial abre sin bloquear el RTU.
- [x] El firmware reporta ID/baudrate/mapa al arrancar.

Log de arranque observado:

```txt
JWPLC Remote I/O Slave RTU - PoC 1
Slave ID: 2
Baudrate: 115200
Formato: 8N1
RS-485: Serial2 RX=IO16 TX=IO17
FC2  -> I0_0..I0_7
FC1  -> feedback Q0_0..Q0_7
FC5  -> write single Q0_x
FC15 -> write block Q0_0..Q0_7
RS485 enabled: yes
Ready: yes
Status: OK
Baud: 115200
Config: SERIAL_8N1
RX pin: 16
TX pin: 17
Last error: OK
[RTU] Slave listo
```

## Validación de APIs base

Antes y después de habilitar Modbus RTU:

- [x] `digitalRead(I0_0)` funciona mediante lectura RTU `FC2` del mapa `I0_0..I0_7`.
- [x] `digitalWrite(Q0_0, HIGH/LOW)` funciona mediante escritura RTU `FC5`.
- [ ] `digitalReadBlock(I0_X)` funciona.
- [ ] `digitalWriteBlock(Q0_X, bitmap)` funciona.
- [ ] `JWPLC_readInputs()` funciona.
- [ ] `JWPLC_writeOutputs(bitmap)` funciona.
- [x] Salidas quedan apagadas al arranque.
- [x] `EN_IO` queda en estado correcto para operación normal de I/O.

Nota: para PoC 1 se validó la ruta funcional real por `digitalRead()` / `digitalWrite()` desde el dispatcher RTU local del ejemplo. Las APIs de bloque quedan pendientes para una validación específica o para la futura integración formal en `JWPLC_ModbusRTU`.

## Pruebas Modbus RTU mínimas

Configuración de QModMaster:

```txt
Modo: Modbus RTU
Puerto: COM4, adaptador USB-RS485
Slave Addr: 2
Baudrate: 115200
Formato: 8 data bits, parity none, 1 stop bit
Scan rate: 1000 ms
Base Addr: 1
Endian: Little
```

Nota de direccionamiento:

```txt
QModMaster Base Addr = 1
QModMaster Start Address 1 = dirección Modbus real 0
QModMaster Start Address 8 = dirección Modbus real 7
QModMaster Start Address 20 = fuera del mapa Remote I/O actual
```

### FC2 - Discrete Inputs

- [x] Leer bloque `0..7` devuelve bitmap correcto de `I0_0..I0_7`.
- [x] Lectura por QModMaster usando `Start Address 1`, `Number of Inputs 8` operativa.
- [x] Leer fuera de mapa devuelve excepción `Illegal data address`.

Validación observada:

```txt
FC2 Read Discrete Inputs
Start Address: 1
Number of Inputs: 8
Resultado: OK, lectura de entradas operativa.
```

### FC15 - Write Multiple Coils

- [ ] Escribir `0x01` activa solo `Q0_0`.
- [ ] Escribir `0x02` activa solo `Q0_1`.
- [ ] Escribir `0x04` activa solo `Q0_2`.
- [ ] Escribir `0x08` activa solo `Q0_3`.
- [ ] Escribir `0x10` activa solo `Q0_4`.
- [ ] Escribir `0x20` activa solo `Q0_5`.
- [ ] Escribir `0x40` activa solo `Q0_6`.
- [ ] Escribir `0x80` activa solo `Q0_7`.
- [x] Escribir `0x00` apaga todas las salidas.
- [x] Escribir patrón alternado por tabla QModMaster funciona.
- [x] Escribir bloque `Q0_0..Q0_7` funciona.

Trama observada para apagar todo:

```txt
Tx: 02 0F 00 00 00 08 01 00 BE 80
Rx: 02 0F 00 00 00 08 54 3E
Sys: values written correctly.
```

### FC5 - Write Single Coil

- [x] Escribir offset `0` controla `Q0_0`.
- [x] Escritura individual `Q0_x` operativa.
- [x] Escribir fuera de `0..7` devuelve `Illegal data address`.

Validación observada:

```txt
FC5 Write Single Coil
Start Address: 1
Number of Coils: 1
Resultado: OK, salida individual operativa.

FC5 Write Single Coil
Start Address: 20
Resultado: OK, excepción Illegal data address.
```

### FC1 - Read Coils / feedback

- [x] Leer bloque `0..7` devuelve el último estado escrito en salidas.
- [x] Después de reiniciar, leer bloque `0..7` devuelve todas las salidas apagadas.
- [x] Leer fuera de mapa devuelve `Illegal data address`.

Validación observada:

```txt
FC1 Read Coils
Start Address: 1
Number of Coils: 8
Resultado: OK, feedback Q0_0..Q0_7 operativo.

FC1 Read Coils después de reinicio
Resultado: 0 0 0 0 0 0 0 0.

FC1 Read Coils
Start Address: 20
Resultado: OK, excepción Illegal data address.
```

## Seguridad de salidas

- [x] Al reiniciar, salidas quedan apagadas hasta recibir comando válido.
- [ ] Si se pierde comunicación, comportamiento documentado.
- [x] Comandos inválidos no dejan salidas en estado indeterminado.
- [x] No hay parpadeos no deseados durante arranque observado en esta prueba.

Nota: el comportamiento ante pérdida de comunicación queda pendiente para PoC posterior. En PoC 1 no se implementa watchdog de comunicación ni timeout de salidas.

## Prueba con varios equipos

No obligatoria para PoC 1, pero útil si hay hardware disponible:

- [ ] Dos slaves con IDs fijos diferentes responden en el mismo bus.
- [ ] IDs duplicados producen conflicto esperado y documentado.
- [ ] Se confirma que el caso de IDs duplicados se deja para commissioning.

## Documentación de cierre de PoC 1

Registrar:

- [x] Fecha de prueba: 2026-07-08.
- [x] Branch y commit: `feature/v2.1.0-alpha.3-remote-io-rtu-poc1`, firmware validado en commit de la rama previo a esta actualización documental.
- [x] Versión del package: `JWPLC/2.1.0` / etapa `v2.1.0-alpha.3`.
- [x] Hardware usado: JWPLC Basic como slave.
- [x] Cableado RS-485: PC con adaptador USB-RS485 hacia RS-485 del JWPLC Basic.
- [x] Baudrate/formato: 115200, 8N1.
- [x] Herramienta master usada: QModMaster.
- [x] Capturas o logs de lectura FC2: validadas por QModMaster.
- [x] Capturas o logs de escritura FC15/FC5: validadas por QModMaster.
- [x] Pendientes para PoC 2: FRAM/configuración persistente, commissioning UID/MAC, múltiples slaves, watchdog de comunicación y futura integración formal en `JWPLC_ModbusRTU`.

## Resultado de validación PoC 1

| Prueba | Function Code | Dirección QModMaster | Dirección Modbus real | Resultado |
|---|---:|---:|---:|---|
| Leer entradas `I0_0..I0_7` | FC2 | Start 1, Qty 8 | 0..7 | OK |
| Escribir salida individual `Q0_x` | FC5 | Start 1 | 0 | OK |
| Leer feedback `Q0_0..Q0_7` | FC1 | Start 1, Qty 8 | 0..7 | OK |
| Escribir salidas en bloque `Q0_0..Q0_7` | FC15 | Start 1, Qty 8 | 0..7 | OK |
| Reinicio seguro | - | - | - | OK, salidas apagadas |
| Dirección fuera de rango en lectura | FC1/FC2 | Start 20 | fuera de mapa | OK, Illegal data address |
| Dirección fuera de rango en escritura | FC5 | Start 20 | fuera de mapa | OK, Illegal data address |

## Criterio de cierre

La PoC 1 puede cerrarse cuando:

```txt
Un JWPLC Basic slave con ID fijo expone I0_0..I0_7 por FC2,
recibe escritura de Q0_0..Q0_7 por FC15/FC5,
y permite leer feedback por FC1 sin romper APIs Arduino normales.
```

**Estado:** criterio de cierre cumplido para la PoC 1 física básica.

## Conclusión

La PoC 1 queda validada como **Remote I/O Slave RTU mínimo** para JWPLC Basic.

Se mantiene como ejemplo aislado usando:

- `JWPLC_RS485` como transporte oficial RS-485.
- Helpers CRC de `JWPLC_ModbusRTU`.
- Dispatcher local para `FC1`, `FC2`, `FC5` y `FC15`.

La integración formal de coils/discrete inputs dentro de `JWPLC_ModbusRTU` queda como evolución posterior, después de validar PoC 2/PoC 3 y sin romper la API actual basada en holding registers.