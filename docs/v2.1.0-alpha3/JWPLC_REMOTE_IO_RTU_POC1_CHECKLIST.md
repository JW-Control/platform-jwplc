# JWPLC Remote I/O RTU - Checklist PoC 1

## Objetivo de la PoC 1

Validar un **JWPLC Basic Remote I/O Slave** con ID fijo por Modbus RTU / RS-485.

La PoC 1 no incluye FRAM ni commissioning. Primero se valida que el bus, el mapa y el I/O físico funcionen.

## Identificación

- [ ] Confirmar branch base.
- [ ] Confirmar versión/alpha.
- [ ] Confirmar nombre del ejemplo o firmware.
- [ ] Confirmar placa/FQBN usada.
- [ ] Confirmar hardware físico usado como slave.
- [ ] Confirmar herramienta usada como master de prueba.

## Configuración inicial sugerida

```txt
Slave ID: 2
Baudrate: 115200
Formato: 8N1
Puerto: Serial2
RX2: IO16
TX2: IO17
Driver: MAX13487 auto-dirección
```

## Compilación y subida

- [ ] Compila en Arduino IDE.
- [ ] Compila con Arduino CLI.
- [ ] Sube correctamente al JWPLC Basic slave.
- [ ] Monitor serial abre sin bloquear el RTU.
- [ ] El firmware reporta ID/baudrate/mapa al arrancar.

## Validación de APIs base

Antes y después de habilitar Modbus RTU:

- [ ] `digitalRead(I0_0)` funciona.
- [ ] `digitalWrite(Q0_0, HIGH/LOW)` funciona.
- [ ] `digitalReadBlock(I0_X)` funciona.
- [ ] `digitalWriteBlock(Q0_X, bitmap)` funciona.
- [ ] `JWPLC_readInputs()` funciona.
- [ ] `JWPLC_writeOutputs(bitmap)` funciona.
- [ ] Salidas quedan apagadas al arranque.
- [ ] `EN_IO` queda en estado correcto.

## Pruebas Modbus RTU mínimas

### FC2 - Discrete Inputs

- [ ] Leer offset `0` devuelve estado de `I0_0`.
- [ ] Leer offset `1` devuelve estado de `I0_1`.
- [ ] Leer offset `2` devuelve estado de `I0_2`.
- [ ] Leer offset `3` devuelve estado de `I0_3`.
- [ ] Leer offset `4` devuelve estado de `I0_4`.
- [ ] Leer offset `5` devuelve estado de `I0_5`.
- [ ] Leer offset `6` devuelve estado de `I0_6`.
- [ ] Leer offset `7` devuelve estado de `I0_7`.
- [ ] Leer bloque `0..7` devuelve bitmap correcto.

### FC15 - Write Multiple Coils

- [ ] Escribir `0x01` activa solo `Q0_0`.
- [ ] Escribir `0x02` activa solo `Q0_1`.
- [ ] Escribir `0x04` activa solo `Q0_2`.
- [ ] Escribir `0x08` activa solo `Q0_3`.
- [ ] Escribir `0x10` activa solo `Q0_4`.
- [ ] Escribir `0x20` activa solo `Q0_5`.
- [ ] Escribir `0x40` activa solo `Q0_6`.
- [ ] Escribir `0x80` activa solo `Q0_7`.
- [ ] Escribir `0x00` apaga todas las salidas.
- [ ] Escribir `0xFF` activa todas las salidas.

### FC5 - Write Single Coil

- [ ] Escribir offset `0` controla `Q0_0`.
- [ ] Escribir offset `7` controla `Q0_7`.
- [ ] Escribir fuera de `0..7` devuelve error o se ignora de forma segura.

### FC1 - Read Coils / feedback

- [ ] Leer offset `0` devuelve feedback de `Q0_0`.
- [ ] Leer offset `7` devuelve feedback de `Q0_7`.
- [ ] Leer bloque `0..7` devuelve el último estado escrito en salidas.

## Seguridad de salidas

- [ ] Al reiniciar, salidas quedan apagadas hasta recibir comando válido.
- [ ] Si se pierde comunicación, comportamiento documentado.
- [ ] Comandos inválidos no dejan salidas en estado indeterminado.
- [ ] No hay parpadeos no deseados durante arranque.

## Prueba con varios equipos

No obligatoria para PoC 1, pero útil si hay hardware disponible:

- [ ] Dos slaves con IDs fijos diferentes responden en el mismo bus.
- [ ] IDs duplicados producen conflicto esperado y documentado.
- [ ] Se confirma que el caso de IDs duplicados se deja para commissioning.

## Documentación de cierre de PoC 1

Registrar:

- [ ] Fecha de prueba.
- [ ] Branch y commit.
- [ ] Versión del package.
- [ ] Hardware usado.
- [ ] Cableado RS-485.
- [ ] Baudrate/formato.
- [ ] Herramienta master usada.
- [ ] Capturas o logs de lectura FC2.
- [ ] Capturas o logs de escritura FC15/FC5.
- [ ] Pendientes para PoC 2.

## Criterio de cierre

La PoC 1 puede cerrarse cuando:

```txt
Un JWPLC Basic slave con ID fijo expone I0_0..I0_7 por FC2,
recibe escritura de Q0_0..Q0_7 por FC15/FC5,
y permite leer feedback por FC1 sin romper APIs Arduino normales.
```

