# Alpha14 — P5 Master/Slave con HMI dirty redraw

Fecha: 2026-09-24  
Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Corrida P5 inicial descartada como qualification final

La primera ejecución de:

```txt
run_a14_p5_final_full_runtime_combined.bat
```

compiló y subió correctamente el firmware diagnóstico, pero el resolver detuvo
la prueba antes de la ventana TCP + RTU porque faltaban precondiciones físicas.

Observado:

```txt
ETH_READY=YES
ETH_LINK=UP
ETH_IP=192.168.0.159

RTU_READY=YES

DISPLAY_READY=YES

FRAM_READY=YES
FRAM_FAILS=0

SD_READY=NO
SD_APPEND_FAILS=46

RTC_PRESENT=YES
RTC_UNAVAILABLE=0
RTC_STALE=0

IO_INITIALIZED=YES
IO_STALE=0

BUTTONS_READY=YES
BUTTON_NOT_READY=0

SPI_PROBE_FAILS=0
```

La microSD no estaba insertada y el segundo JWPLC no estaba conectado para la
prueba RTU.

Clasificación:

```txt
P5_INITIAL_RUN=PRECONDITION_INCOMPLETE
PRODUCT_FAILURE=NO
ETHERNET_FAILURE=NO
RTU_LIBRARY_INIT_FAILURE=NO
FINAL_COMBINED_WINDOW_NOT_REACHED=YES
```

Los picos de loop/service observados durante el resolver no se usan como
resultado de qualification porque la ventana combinada todavía no había
comenzado y el DUT estaba respondiendo snapshots periódicos.

## Cambio de topología P5

Se reemplaza el modelo anterior, que usaba el peer como Master externo, por una
topología explícita más representativa:

```txt
COM14 = JWPLC principal
ROLE  = Modbus RTU MASTER
       + Modbus TCP Server
       + full runtime periférico

COM4  = segundo JWPLC
ROLE  = Modbus RTU SLAVE ID 2
```

RTU:

```txt
baud       = 115200
format     = 8N1
period     = 20 ms
target     = ~50 Hz
slave id   = 2
function   = FC03
verify HR1 = 0x55AA
```

El Master usa la API cooperativa:

```cpp
JWPLC_ModbusRTU.requestReadHoldingRegisters(...)
JWPLC_ModbusRTU.task()
```

No se usa la ruta síncrona bloqueante para generar la carga RTU.

## Nueva HMI para ambos equipos

Los dos sketches P5 usan la API moderna de `JWPLC_Display`:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
JWPLC_Display.setFields(...);
JWPLC_Display.setValue(...);
JWPLC_Display.setText(...);
JWPLC_Display.setBool(...);
```

Decisión:

```txt
DISPLAY_MODE=HMI_ON_DEMAND_DIRTY
PERIODIC_FILL_SCREEN=NO
LEGACY_jwplcUserDisplay_CALLBACKS=NO
```

La UI sólo invalida/redibuja fields cuyo contenido cambia. Esto evita repintar
la pantalla completa en cada periodo y reduce tanto parpadeo como tráfico SPI
innecesario.

### Master

Sketch:

```txt
tools/modbus-tcp-benchmark/firmware/
a14_p5_full_runtime_master/
a14_p5_full_runtime_master.ino
```

HMI:

```txt
MASTER
TCP OK
RTU OK
RTU FAIL
SD
ETH
```

Además conserva el workload full-runtime:

- TFT;
- Ethernet / Modbus TCP;
- microSD append/verify;
- FRAM write/read/restore;
- RTC;
- botones;
- TCA/I-O;
- SPI mutex probe.

### Slave

Sketch:

```txt
tools/modbus-tcp-benchmark/firmware/
a14_p5_rtu_slave/
a14_p5_rtu_slave.ino
```

HMI:

```txt
SLAVE 2
RX
TX
OK
CRC
HR0
```

Mapa mínimo de verificación:

```txt
HR0 = contador dinámico
HR1 = 0x55AA
```

## Secuencia de gates

### P5-A

Sólo source contract + compilación de ambos sketches.

```txt
UPLOAD=NO
```

Debe demostrar:

- HMI on-demand en Master y Slave;
- cero callbacks Display legacy;
- cero `fillScreen()` directo en ambos sketches;
- Master RTU cooperativo;
- target Slave ID 2;
- compile PASS de ambos.

### P5-B

Sólo después de P5-A PASS:

- subir Slave a COM4;
- subir Master/full-runtime a COM14;
- comprobar microSD insertada;
- comprobar RS-485 entre ambos;
- resolver IP del Master;
- ejecutar FC03/125 TCP @ 1000 req/s;
- mantener RTU Master ~50 Hz simultáneo;
- cross-check Master/Slave;
- validar TFT visual sin parpadeo;
- validar periféricos y errores.

Estado:

```txt
P5A=READY_TO_RUN
P5B=PENDING_P5A
```
