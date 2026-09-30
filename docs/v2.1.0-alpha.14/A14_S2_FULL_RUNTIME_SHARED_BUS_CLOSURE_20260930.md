# Alpha14 — S2 Full Runtime Shared Bus — cierre 2026-09-30

## Resultado

```text
A14_S2_FULL_RUNTIME_SHARED_BUS=PASS
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
PHYSICAL_STABILITY=PASS
```

Topología validada:

- Master/DUT: COM14.
- Slave: COM4.
- Enlace funcional Master/Slave: RS-485.
- Modbus RTU: 115200 8N1.
- Slave ID: 2.
- W5500: 26 MHz.
- FIFO_REUSE: activo solo para el gate.
- DIRECT_RX: OFF.
- Relés físicos: no accionados.
- Duración: 600 s.

## Ethernet bidireccional

```text
ETH_RX_BYTES=236004416
ETH_ECHO_BYTES=236004416
ETH_RX_MBPS=3.146726
ETH_ECHO_TX_MBPS=3.146726

ETH_CORRUPTION_ERRORS=0
ETH_TRANSPORT_ERRORS=0
ETH_SPI_LOCK_ERRORS=0

ETH_BEGIN_WRITE_MAX_US=2449
ETH_POLL_WRITE_MAX_US=2333

ETH_SPI_HOLD_MAX_US=3269
ETH_SPI_HOLD_TOTAL_US=497852388
ETH_SPI_HOLD_COUNT=778763

LINK_FINAL=UP
```

La prueba PC confirmó integridad completa del echo:

```text
PC_SENT_BYTES=240197632
PC_ECHO_BYTES=236004416
PC_ECHO_INTEGRITY=PASS
```

La diferencia entre bytes enviados por PC y bytes aceptados/echo por el DUT corresponde al tráfico aún en cola cuando termina la ventana. La integridad se compara contra los bytes realmente recibidos/echo por el DUT.

## Modbus RTU

Cada ciclo:

1. FC15 Write Multiple Coils.
2. FC01 Read Coils.
3. validación exacta del patrón.
4. FC02 Read Discrete Inputs.

Resultado:

```text
MODBUS_CYCLES_OK=16047
MODBUS_FAILURES=0
MODBUS_PATTERN_MISMATCHES=0
MODBUS_MAX_TRANSACTION_US=52928

MASTER_RX_FRAMES=48142
MASTER_TX_FRAMES=48143
MASTER_REQUESTS_OK=48142
MASTER_CRC_ERRORS=0
MASTER_TIMEOUTS=0

SLAVE_RX_FRAMES=48143
SLAVE_TX_FRAMES=48143
SLAVE_REQUESTS_OK=48143
SLAVE_CRC_ERRORS=0
```

No hubo fallo de transporte RTU, CRC ni timeout.

## FRAM

Master:

```text
FRAM_OK=1198
FRAM_FAIL=0
```

Slave:

```text
FRAM_OK=1200
FRAM_FAIL=0
```

El gate usa scratch con backup/restore.

## RTC

Master:

```text
RTC_OK=599
RTC_FAIL=0
```

Slave:

```text
RTC_OK=600
RTC_FAIL=0
```

## microSD

```text
SD_OK=299
SD_FAIL=0
```

El gate crea, escribe, lee, compara y elimina un archivo temporal propio.

## TCA / I/O

Master:

```text
IO_SAMPLES=28668
```

Slave:

```text
IO_SAMPLES=30005
```

## Botonera

Master:

```text
BUTTON_DOWN_SAMPLES=85
```

Slave:

```text
BUTTON_DOWN_SAMPLES=74
```

Validación física de botón OK en ambos nodos: PASS.

## TFT

Validación visual durante el soak:

```text
TFT_PHYSICAL=PASS
PHYSICAL_STABILITY=PASS
```

Ambas TFT permanecieron activas y sin congelarse.

## Loop / jitter

Master:

```text
MAX_LOOP_US=69468
LONG_LOOP_CRITICAL=0
```

Slave:

```text
MAX_LOOP_US=3563
LONG_LOOP_CRITICAL=0
```

El Master mostró un peor loop de 69.468 ms. Esto no produjo timeout RTU, corrupción Ethernet ni lock SPI, pero es una señal de jitter relevante.

Comparado con la corrida S2 previa, donde SD no se ejecutó por un bug del harness, el aumento de latencia aparece al incluir el probe real de microSD. No se atribuye causalmente a SD sin un A/B dedicado, pero debe vigilarse antes de validar un runtime PLC fijo de 20 ms.

La peor transacción Modbus RTU fue 52.928 ms. También se conserva como dato de jitter, aunque no produjo fallos.

## Decisión S2

S2 demuestra convivencia simultánea, durante 10 minutos, de:

- Ethernet RX.
- Ethernet TX async.
- Modbus RTU Master/Slave.
- TFT.
- microSD.
- FRAM.
- RTC.
- botonera.
- TCA/I/O.

Con:

```text
SPI_LOCK_ERRORS=0
ETH_CORRUPTION_ERRORS=0
ETH_TRANSPORT_ERRORS=0
MODBUS_FAILURES=0
MODBUS_PATTERN_MISMATCHES=0
MODBUS_CRC_ERRORS=0
MODBUS_TIMEOUTS=0
FRAM_FAIL=0
RTC_FAIL=0
SD_FAIL=0
MASTER_RESETS=0
SLAVE_RESETS=0
```

### Baseline técnica aceptada para el siguiente frente

Se conserva como base:

- W5500 a 26 MHz.
- batch RX conservador equivalente al baseline validado.
- commit RX inmediato.
- TX async aditivo.
- APIs Arduino legacy intactas.

`FIFO_REUSE` queda aprobado por datos y convivencia física para promoción al comportamiento normal del package, sujeto a gate de promoción/default.

`SINGLE_STATUS` conserva evidencia favorable de P8, pero debe integrarse limpiamente en producto antes de considerarse baseline normal; no crear cache persistente.

## Próxima secuencia

1. Gate de promoción de `FIFO_REUSE` como default.
2. Integración productiva mínima de `SINGLE_STATUS`.
3. Revalidación corta de promoción.
4. H3E-R:
   - 1000 TCP requests/s;
   - RTU ~50 Hz / scan ~20 ms;
   - periféricos activos;
   - 0 fallos.
5. P4.1 DLEN_REUSE.
6. P4.2 COPY_OUT 64 B.
7. Combinar solo winners.
8. Evaluar SAME-OWNER DMA solo si sigue justificado.
