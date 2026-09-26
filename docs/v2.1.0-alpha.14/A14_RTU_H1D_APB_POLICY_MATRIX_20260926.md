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


## Intento H1D-R1 — fallo de tooling/precompilado

El primer intento H1D no ejecuto la matriz fisica. El Slave fallo durante
el enlace porque `precompiled=full` selecciono el archive historico
`libJWPLC_ModbusRTU.a`, mientras los headers y el firmware de qualification
ya usan APIs incorporadas despues de ese archive, entre ellas:

```txt
motor(JWPLCModbusMotor)
setFrameGapUs(unsigned long)
setQueuedTxEnabled(bool)
queuedTxActive() const
effectiveBaudRate() const
```

Clasificacion:

```txt
H1D_R1_PRODUCT_RESULT=NOT_EXECUTED
H1D_R1_FAILURE=STALE_MODBUS_PRECOMPILED_ARCHIVE
APB_POLICY_RESULT=PENDING
```

Esto no es evidencia contra APB.

## H1D-R2 — compilacion source controlada

H1D-R2 reutiliza el mecanismo ya validado en F2/F3/H1/H1C:

1. calcula y guarda el SHA-256 del archive Modbus RTU existente;
2. copia el archive a un directorio temporal;
3. lo oculta solo durante compile/upload;
4. ejecuta P5B con `-AllowMissingModbusRtuArchiveCandidate`;
5. exige objetos frescos de `JWPLC_ModbusRTU.cpp` y `JWPLC_RS485.cpp`
   para Master y Slave;
6. restaura el archive original en `finally`;
7. exige que el SHA-256 restaurado sea identico al inicial;
8. recien entonces ejecuta la matriz H1D.

El archive historico no se adopta como artefacto final por este gate.
Despues de cerrar la policy funcional sera necesario regenerar y calificar
el precompilado antes de publicarlo.


## Decisión adoptada

Con la matriz H1D cerrada y los cinco baudrates limpios, se adopta para
JWPLC Basic v2 sobre ESP32 clásico la siguiente política:

```txt
JWPLC_RS485_UART_CLOCK=APB_FORCED
SCOPE=ESP32_CLASSIC
```

Motivos:

- corrige el caso 230400 caracterizado en H1C;
- 115200, 230400, 250000, 460800 y 500000 quedaron limpios;
- no introduce selección condicional por baudrate;
- deja una política determinista para el UART RS-485;
- ESP32-S3 y otros SoC quedan fuera de esta decisión hasta qualification propia.

La investigación de reloj queda cerrada para ESP32 clásico. El siguiente foco
de Alpha14 vuelve a 500000 baud y al throughput de JWPLC_FAST bajo convivencia
con TCP/full runtime.
