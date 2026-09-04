# Alpha7 — A7.1 Arduino ↔ Arduino Remote I/O

## Estado

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=CLOSED
RESULT=PASS
```

Validación física realizada con dos JWPLC Basic sobre RS-485 usando la API oficial `JWPLC_ModbusRTU` compilada desde source.

## Configuración del banco

- Master Arduino: ID local 247.
- Remote I/O Slave: ID 2.
- Modbus RTU: 115200, 8N1.
- Funciones probadas: FC01, FC02, FC05 y FC15.
- Refresco de imagen DO durante holds: 40 ms.
- Fail-safe del Slave: 100 ms sin escritura DO válida.
- Sin cargas reales conectadas a los relés durante el gate.

## Resultado funcional

```text
FC01_READ_COILS=PASS
FC02_READ_DISCRETE_INPUTS=PASS
FC05_WRITE_SINGLE_COIL=PASS
FC15_WRITE_MULTIPLE_COILS=PASS
MASTER_COOPERATIVE=PASS
DO_REFRESH_40MS=PASS
```

Secuencia automática A7.1:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
PASS_COUNT=58
FAIL_COUNT=0
```

Después del primer RUN:

```text
MASTER_RX_TX_OK=104/104/104
MASTER_CRC_TIMEOUT=0/0
LAST_ERROR=OK
```

## Walking outputs físico

Se recorrieron Q0_0..Q0_7 mediante FC15, manteniendo una sola salida activa por paso y verificando feedback con FC01.

```text
ALPHA7_A7_1_WALKING_OUTPUTS=PASS
MASTER_RX_TX_OK=432/432/432
SLAVE_RX_TX_OK=432/432/432
MASTER_CRC=0
SLAVE_CRC=0
MASTER_TIMEOUTS=0
SLAVE_EXCEPTIONS=0
```

Al terminar el walking y cesar los refresh DO, el Slave entró correctamente en fail-safe manteniendo `Q=0x00`.

## Pérdida y recuperación de bus

Durante un walking output se desconectó físicamente el bus RS-485.

Master:

```text
[FAIL] FC15 refresh DO result=5 error=Timeout
READY=YES
BUSY=NO
RX/TX/OK=689/690/689
CRC/TIMEOUT=0/1
```

Slave:

```text
[FAILSAFE] 101 ms sin FC05/FC15 -> Q=0
```

Tras reconectar A/B, sin resetear el Master, un nuevo `RUN` volvió a completar:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
PASS_COUNT=58
FAIL_COUNT=0
```

Conclusión:

```text
BUS_DISCONNECT_DETECTED=PASS
FAILSAFE_ON_BUS_LOSS=PASS
MASTER_TIMEOUT_NO_FREEZE=PASS
RECONNECT_WITHOUT_MASTER_RESET=PASS
```

## Reset del Slave y recuperación

Durante un walking output se reseteó únicamente el Slave.

El Slave reinició en estado seguro:

```text
POWERON_RESET
MODBUS_BEGIN=PASS
SLAVE_ID=2
Q=0x00
```

El Master detectó la ausencia mediante timeout y quedó operativo:

```text
READY=YES
BUSY=NO
RX/TX/OK=967/969/967
CRC/TIMEOUT=0/2
LAST_ERROR=Timeout
```

Sin resetear el Master, después del retorno del Slave un nuevo `RUN` completó correctamente:

```text
ALPHA7_A7_1_ARDUINO_ARDUINO=PASS
PASS_COUNT=58
FAIL_COUNT=0
FC01=PASS
FC02=PASS
FC05=PASS
FC15=PASS
DO_REFRESH_40MS=PASS
```

Conclusión:

```text
SLAVE_RESET_DETECTED=PASS
SLAVE_OUTPUTS_SAFE_AFTER_RESET=PASS
MASTER_SURVIVES_SLAVE_RESET=PASS
RECOVERY_WITHOUT_MASTER_RESET=PASS
```

## Conclusión A7.1

La ruta Arduino Master ↔ Arduino Remote I/O Slave queda cerrada para Alpha7 usando `JWPLC_ModbusRTU` oficial y Master cooperativo.

El siguiente gate es A7.2: OpenPLC Master → Arduino Remote I/O Slave, reutilizando el mismo Slave oficial validado aquí.
