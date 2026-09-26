# Alpha14 — RTU-H1D — policy APB del package

Fecha: 2026-09-26

## Motivo

RTU-H1C demostro causalmente que el perfil automatico del core selecciona
REF_TICK a 230400 baud en ESP32 clasico y que ese reloj produce:

```txt
requested=230400
effective=231884
error=+0.6441 %
RTU_CLEAN=NO
```

Forzando APB con todo lo demas constante:

```txt
requested=230400
effective=230423
error=+0.0100 %
STARTED=11418
SUCCESS=11418
FAILED=0
TIMEOUTS=0
CRC=0
RTU_CLEAN=YES
```

## Hipotesis unica

El puerto RS-485 del JWPLC Basic v2 debe seleccionar APB antes de
`HardwareSerial::begin()` en ESP32 clasico.

No se cambia en H1D:

- frame gap;
- timeout;
- motor ASYNC;
- TX queued;
- W5500;
- topologia TCP;
- core.a;
- configuracion de flash.

## Candidato

`JWPLC_RS485.begin()` aplica:

```cpp
#if JWPLC_RS485_FORCE_APB_CLOCK
_serial->setClockSource(UART_CLK_SRC_APB);
#endif
```

El default queda activo solo para `CONFIG_IDF_TARGET_ESP32`.
Otros SoC conservan la seleccion del core hasta qualification propia.

La libreria expone `clockSourceString()` para que los snapshots no reporten
"AUTO" cuando la policy del package ya es APB.

## Matriz

Todos los casos usan:

```txt
8N1
ASYNC
TX QUEUED
frame gap = 500 us
TCP = OFF
RTU = UNPACED
```

Baud rates:

| Baud |
|---:|
| 115200 |
| 230400 |
| 250000 |
| 460800 |
| 500000 |

Cada caso dura 30 s por defecto.

## Criterio PASS

Los cinco casos deben cumplir simultaneamente:

```txt
MASTER_CLOCK=APB_FORCED
SLAVE_CLOCK=APB_FORCED
requested baud correcto
effective baud Master == Slave
FAILED=0
TIMEOUTS=0
MASTER_CRC=0
SLAVE_CRC=0
cross-count exacto
PERIPHERAL_FAILURE_COUNT=0
SD_DATALOG_FAILED_COMMITS=0
```

y ambas TFT deben permanecer fisicamente estables.

Cierre esperado:

```txt
RTUH1D_CLEAN_BAUDS=115200,230400,250000,460800,500000
A14_RTU_H1D=PASS_APB_POLICY_MATRIX
A14_RTU_H1D_APB_POLICY_MATRIX_GATE=PASS
```

## Regla de decision

Si H1D falla en cualquier baud, no se adopta la policy global APB y se vuelve a
una seleccion mas acotada basada en baud.

No aumentar timeout ni modificar frame gap para hacer pasar H1D: este gate solo
califica la fuente de reloj.
