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

## P5-A — resultado

Gate:

```txt
run_a14_p5a_master_slave_hmi_compile.bat
```

Resultado físico/local reportado:

```txt
MASTER_SOURCE_CONTRACT=PASS
SLAVE_SOURCE_CONTRACT=PASS

MASTER_COMPILE_EXIT=0
MASTER_BIN_COUNT=4

SLAVE_COMPILE_EXIT=0
SLAVE_BIN_COUNT=4

FINAL_SPI_HZ=26000000
TRACKED_DIRTY_FINAL=0
STAGED_COUNT_FINAL=0

A14_P5A_MASTER_HMI=USER_REFRESH_ON_DEMAND
A14_P5A_SLAVE_HMI=USER_REFRESH_ON_DEMAND
A14_P5A_MASTER_RTU=COOPERATIVE_50HZ_TARGET
A14_P5A_SLAVE_ID=2
A14_P5A_MASTER_SLAVE_HMI_COMPILE=PASS
```

Conclusión:

```txt
P5A=CLOSED_PASS
```

## P5-B — contrato físico final

Después del PASS de P5-A se endurece el Master diagnóstico:

```txt
RTU Master auto-start desde boot
R estadístico conserva RTU activo
snapshot publica ETH_READY / ETH_LINK / ETH_IP
snapshot publica COMBINED_RUNTIME_READY
```

El resolver exige antes de medir:

```txt
FULL_RUNTIME_READY=YES
COMBINED_RUNTIME_READY=YES
SERVER_READY=YES
SD_READY=YES
DISPLAY_READY=YES
DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY
DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND
ETH_READY=YES
ETH_LINK=UP
RTU_READY=YES
RTU_ROLE=MASTER
RTU_TARGET_SLAVE_ID=2
RTU_TRAFFIC_ENABLED=YES
RTU_REQUESTS_SUCCESS>=10
RTU_REQUESTS_FAILED=0
RTU_VERIFY_FAILS=0
RTU_CRC_ERRORS=0
RTU_MASTER_TIMEOUTS=0
```

Esto convierte microSD, Ethernet, HMI y comunicación física RTU con el Slave 2
en precondiciones obligatorias antes de abrir la ventana formal.

### Sincronización de contadores

El runner P5-B reutiliza el qualification runner TCP ya validado.

Cuando el runner TCP ejecuta su `R` estadístico del Master, P5-B intercepta
esa misma llamada y ejecuta inmediatamente:

```txt
MASTER RESET STATS
SLAVE  RESET STATS
```

antes de iniciar el reloj de la medición TCP.

No se abre COM14 desde dos procesos simultáneamente.

### Ventana formal

```txt
DURATION=60 s
TCP=FC03/125 @ 1000 req/s
RTU=FC03 hacia Slave 2 @ periodo 20 ms
TFT Master=dirty redraw on demand
TFT Slave=dirty redraw on demand
SD/FRAM/RTC/buttons/TCA-I/O=activos
W5500 SPI=26 MHz
```

Criterios RTU principales:

```txt
Master started >= 45 * duration
Master rejected=0
Master failed=0
Master verify fails=0
Master CRC=0
Master timeout=0
RTU achieved=45..52 Hz

Slave RX/TX/OK >= 45 * duration
Slave CRC=0
Slave exceptions=0
Slave HR1=0x55AA
Master/Slave cross-count delta <=12
```

`RTU_PERIODS_SKIPPED` se conserva como telemetría y no como gate.

### Observación visual obligatoria

Al terminar se pregunta por separado:

```txt
MASTER COM14 estable / sin flicker
SLAVE  COM4  estable / sin flicker
```

Estado:

```txt
P5A=CLOSED_PASS
P5B=READY_TO_RUN
```

## P5-B intento 1 — preflight RTU perturbado por snapshot serial

La primera ejecución física de P5-B llegó correctamente hasta el preflight:

```txt
SLAVE_UPLOAD_EXIT=0
MASTER_UPLOAD_EXIT=0

FULL_RUNTIME_READY=YES
SERVER_READY=YES
ETH_READY=YES
ETH_LINK=UP
ETH_IP=192.168.0.159
COMBINED_RUNTIME_READY=YES

DISPLAY_READY=YES
DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY
DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND

FRAM_READY=YES
FRAM_FAILS=0

SD_READY=YES
SD_APPEND_FAILS=0
SD_VERIFY_FAILS=0

RTC_PRESENT=YES
RTC_UNAVAILABLE=0
RTC_STALE=0

IO_INITIALIZED=YES
IO_STALE=0

BUTTONS_READY=YES
BUTTON_NOT_READY=0

SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

Sin embargo, el preflight RTU mostró:

```txt
RTU_REQUESTS_STARTED=1688
RTU_REQUESTS_SUCCESS=1636
RTU_REQUESTS_FAILED=52
RTU_CRC_ERRORS=1
RTU_MASTER_TIMEOUTS=51
RTU_PERIODS_SKIPPED=676

LOOP_GAP_MAX_US=154316
RTU_SERVICE_GAP_MAX_US=154323
```

El gate se detuvo antes de la ventana formal de 60 s.

### Diagnóstico

El resolver usaba repetidamente el snapshot completo `S` para polling de
readiness.

Ese snapshot imprime varios KB por Serial a 115200. Mientras se ejecuta
`printSnapshot()`, el loop cooperativo no puede volver a:

```cpp
JWPLC_ModbusRTU.task();
```

La coincidencia entre:

```txt
LOOP_GAP_MAX_US=154316
RTU_SERVICE_GAP_MAX_US=154323
```

es evidencia directa de que el mecanismo de observación estaba generando
ventanas largas sin servicio RTU.

Clasificación:

```txt
P5B_WINDOW_FORMAL_STARTED=NO
SD_FAILURE=NO
ETHERNET_FAILURE=NO
DISPLAY_FAILURE=NO
PERIPHERAL_FAILURE=NO
RTU_PRODUCT_FAILURE=NOT_ESTABLISHED
HARNESS_OBSERVER_EFFECT=YES
```

Los 51 timeouts y el CRC observado en este intento no se usan como resultado de
producto. Debe repetirse con instrumentación no intrusiva.

### Corrección

Se añade al Master un comando:

```txt
P
```

que produce únicamente el contrato compacto:

```txt
FULL_RUNTIME_READY
COMBINED_RUNTIME_READY
SERVER_READY
ETH_READY
ETH_LINK
ETH_IP
SD_READY
DISPLAY_READY
DISPLAY_RENDER_MODE
DISPLAY_REFRESH_MODE
RTU_READY
RTU_ROLE
RTU_TARGET_SLAVE_ID
RTU_TRAFFIC_ENABLED
RTU_REQUESTS_SUCCESS
RTU_REQUESTS_FAILED
RTU_VERIFY_FAILS
RTU_CRC_ERRORS
RTU_MASTER_TIMEOUTS
PERIPHERAL_FAILURE_COUNT
A14_P5_PREFLIGHT=END
```

El resolver P5-B ahora realiza cada intento así:

```txt
R
espera quieta 3 s sin snapshots
P una sola vez
evalúa
```

Si el intento falla, el siguiente vuelve a ejecutar `R`, por lo que cualquier
perturbación causada por imprimir `P` queda fuera del siguiente periodo
observado.

### Snapshot limpio de fin de ventana

El runner TCP histórico necesita snapshots completos al finalizar.

El primer snapshot completo inmediatamente después de los 60 s contiene los
contadores RTU capturados antes de que el propio print serial pueda perturbar el
loop.

P5-B ahora intercepta y conserva exactamente ese snapshot:

```txt
P5B_MASTER_MEASUREMENT_SNAPSHOT=
CAPTURED_BEFORE_POST_SNAPSHOT_PERTURBATION
```

Snapshots completos posteriores siguen disponibles para compatibilidad del
qualification runner, pero no se usan para calificar RTU.

Estado:

```txt
P5B_ATTEMPT_1=HARNESS_OBSERVER_EFFECT
P5B_FORMAL_WINDOW=NOT_RUN
P5B_CORRECTED=READY_TO_RERUN
```

## P5-B intento 2 — preflight compacto limpio pero timeout RTU insuficiente

Con el preflight compacto `P` ya activo, la segunda ejecución física mostró:

```txt
FULL_RUNTIME_READY=YES
COMBINED_RUNTIME_READY=YES
SERVER_READY=YES
ETH_READY=YES
ETH_LINK=UP
SD_READY=YES
DISPLAY_READY=YES
DISPLAY_RENDER_MODE=HMI_ON_DEMAND_DIRTY
DISPLAY_REFRESH_MODE=USER_REFRESH_ON_DEMAND
PERIPHERAL_FAILURE_COUNT=0
```

RTU durante la ventana quieta de preflight:

```txt
RTU_REQUESTS_SUCCESS=140
RTU_REQUESTS_FAILED=10
RTU_VERIFY_FAILS=0
RTU_CRC_ERRORS=0
RTU_MASTER_TIMEOUTS=10
```

Conclusión:

```txt
F047_OBSERVER_EFFECT=FIXED
P5B_FORMAL_WINDOW_STARTED=NO
PHYSICAL_LINK_RTU=WORKING
CURRENT_RTU_TIMEOUT_15MS=TOO_AGGRESSIVE_FOR_FULL_RUNTIME
```

No se cambia aún el timeout por decisión manual.

## P5-C — calibration sweep RTU timeout

Objetivo:

determinar el menor timeout RTU que mantenga el periodo objetivo de 20 ms sin
errores bajo el full-runtime actual y con margen frente al peor service-gap
observado.

Variantes:

```txt
15 ms
25 ms
35 ms
50 ms
```

Cada variante:

- recompila únicamente una copia temporal del Master;
- mantiene Slave ID 2;
- conserva todos los periféricos del full-runtime;
- mantiene RTU period = 20 ms;
- no genera carga TCP todavía;
- resetea Master y Slave;
- mide 15 s sin snapshots durante la ventana;
- detiene RTU antes de capturar telemetría;
- cruza Master success contra Slave RX/TX/OK.

Criterio de candidato:

```txt
rejected=0
failed=0
verify_fails=0
crc_errors=0
master_timeouts=0
slave_crc=0
slave_exceptions=0
peripheral_failures=0
achieved_hz=45..52
cross_count_delta<=12
timeout_headroom >= 5 ms
```

El headroom se calcula contra:

```txt
ceil(RTU_SERVICE_GAP_MAX_US / 1000)
```

Regla de selección:

```txt
LOWEST_ZERO_ERROR_45TO52HZ_WITH_5MS_HEADROOM
```

Estado:

```txt
P5A=CLOSED_PASS
P5B_ATTEMPT_1=HARNESS_OBSERVER_EFFECT
P5B_ATTEMPT_2=RTU_TIMEOUT_CALIBRATION_REQUIRED
P5C=READY_TO_RUN
```

## P5-C — cierre PASS y selección de timeout

El sweep de calibración produjo:

```txt
15 ms -> FAIL
  47.986 Hz
  31 timeouts
  31 failed
  service gap = 15153 us
  headroom = -1 ms

25 ms -> PASS
  50.047 Hz
  0 timeouts
  0 failed
  service gap = 17815 us
  headroom = 7 ms
  cross-count exacto

35 ms -> PASS
  50.017 Hz
  0 timeouts
  0 failed
  service gap = 16915 us
  headroom = 18 ms
  cross-count exacto

50 ms -> PASS
  50.047 Hz
  0 timeouts
  0 failed
  service gap = 18520 us
  headroom = 31 ms
  cross-count exacto
```

Resultado:

```txt
P5C_SELECTED_TIMEOUT_MS=25
P5C_SELECTION_RULE=LOWEST_ZERO_ERROR_45TO52HZ_WITH_5MS_HEADROOM
A14_P5C_RTU_TIMEOUT_CALIBRATION=PASS
```

La evidencia de 15 ms además confirma que el enlace físico no era la causa:

```txt
Master started=753
Master success=722
Master timeout=31

Slave RX=753
Slave TX=753
Slave OK=753
Slave CRC=0
```

El Slave recibió y contestó las 753 solicitudes; el Master declaró timeout en
31 de ellas antes de volver a procesar la respuesta.

Decisión:

```txt
P5_RTU_TIMEOUT_MS=25
P5_RTU_PERIOD_MS=20
P5C=CLOSED_PASS
```

25 ms se fija únicamente en el firmware diagnóstico P5. No se modifica la
librería productiva `JWPLC_ModbusRTU`.

## Próximo gate

P5-B se repite con:

```txt
TCP FC03/125 @ 1000 req/s
RTU period = 20 ms
RTU timeout = 25 ms
duration = 60 s
full runtime = activo
Master/Slave HMI = USER_REFRESH_ON_DEMAND
W5500 SPI = 26 MHz
```

El gate P5-B ahora verifica explícitamente que el Master compilado contiene:

```txt
RTU_TIMEOUT_MS=25
```

y el qualification runner también exige ese mismo valor en el snapshot formal.

Estado:

```txt
P5A=CLOSED_PASS
P5C=CLOSED_PASS
P5B=READY_TO_RERUN_WITH_25MS
```

## P5-B — cierre PASS con TCP1000 + RTU50

La repetición física con `RTU_TIMEOUT_MS=25` cerró en PASS.

Preflight:

```txt
P5_DUT_READY=YES
P5_FULL_RUNTIME_READY=YES
P5_COMBINED_RUNTIME_READY=YES
P5_SD_READY=YES
P5_DISPLAY_HMI_DIRTY=YES
P5_RTU_READY=YES
P5_RTU_TIMEOUT_MS=25
P5_RTU_PEER_SLAVE2=PASS
P5_PREFLIGHT_MODE=COMPACT_QUIET
```

Ventana formal de 60 s:

```txt
TCP requested = 1000 req/s
TCP achieved  = 985.34 req/s
TCP achieved  = 98.534 %
TCP result    = STABLE_PASS

TCP timeouts          = 0
TCP transport errors  = 0
TCP protocol errors   = 0
TCP bus lock timeouts = 0

latency avg = 1007.7 us
latency P95 = 1540.8 us
latency P99 = 9058.5 us
latency max = 30090.1 us

loop gap avg = 798 us
loop gap max = 29110 us
```

Periféricos:

```txt
DISPLAY_READY=YES
FRAM_READY=YES
FRAM_FAILS=0
SD_READY=YES
SD_APPEND_FAILS=0
SD_VERIFY_FAILS=0
RTC_PRESENT=YES
RTC_UNAVAILABLE=0
RTC_STALE=0
IO_INITIALIZED=YES
IO_STALE=0
BUTTONS_READY=YES
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

RTU Master:

```txt
RTU_TIMEOUT_MS=25
RTU_TRAFFIC_DURATION_MS=60062
RTU_REQUESTS_STARTED=3001
RTU_REQUESTS_REJECTED=0
RTU_REQUESTS_COMPLETED=3001
RTU_REQUESTS_SUCCESS=3001
RTU_REQUESTS_FAILED=0
RTU_VERIFY_FAILS=0
RTU_CRC_ERRORS=0
RTU_MASTER_TIMEOUTS=0
RTU_LAST_ERROR=OK
RTU_ACHIEVED_HZ=49.965
```

RTU Slave:

```txt
RTU_RX_FRAMES=3012
RTU_TX_FRAMES=3012
RTU_REQUESTS_OK=3012
RTU_CRC_ERRORS=0
RTU_EXCEPTIONS_SENT=0
RTU_LAST_ERROR=OK
```

Cross-count:

```txt
tail tolerance = 12
RX delta = 11
TX delta = 11
OK delta = 11
RTU_CROSS_COUNT_PASS=YES
```

La diferencia de 11 frames entra dentro de la tolerancia de cola y corresponde
al desfase entre el snapshot limpio del Master y el snapshot posterior del Slave.

HMI:

```txt
MASTER_TFT_PHYSICAL_PASS=True
SLAVE_TFT_PHYSICAL_PASS=True
TFT_PHYSICAL_PASS=True
```

Invariantes finales:

```txt
FINAL_SPI_HZ=26000000
TRACKED_DIRTY_FINAL=0
STAGED_COUNT_FINAL=0
A14_P5B_PRODUCT_SOURCE_MUTATION=NO
A14_P5B_PHYSICAL_MASTER_SLAVE_COMBINED=PASS
```

Conclusión:

```txt
P5B=CLOSED_PASS
F048_MITIGATION_WITH_25MS=VALIDATED_UNDER_TCP1000
```

### Nota sobre RTU_SERVICE_GAP_MAX_US

Durante la ventana combinada se observó:

```txt
RTU_SERVICE_GAP_MAX_US=29958
RTU_TIMEOUT_MS=25
RTU_MASTER_TIMEOUTS=0
```

Esto no invalida el PASS: el service-gap máximo mide el intervalo entre dos
ejecuciones consecutivas del servicio RTU en cualquier estado. Un gap >25 ms
sólo produciría timeout si coincidiera con una transacción Master pendiente.
La corrida formal demuestra que el peor gap observado no coincidió con una
ventana pendiente que venciera.

Por esa razón se mantiene 25 ms para el siguiente gate y se exige un long-run
antes de cerrar definitivamente la fase full-runtime.

## P5-D — long run

Se añade un gate de 600 s que reutiliza exactamente P5-B:

```txt
duration = 600 s
TCP = FC03/125 @ 1000 req/s
RTU = ~50 Hz
RTU timeout = 25 ms
W5500 SPI = 26 MHz
full runtime peripherals = activos
Master + Slave HMI dirty = activos
```

No introduce nuevas variables ni modifica producto.

Gate:

```txt
run_a14_p5d_full_runtime_long_run.bat
```

Estado:

```txt
P5A=CLOSED_PASS
P5C=CLOSED_PASS
P5B=CLOSED_PASS
P5D=READY_TO_RUN
```


## P5-D intento 1 — long run 600 s

La primera corrida de 10 minutos se ejecutó con:

```txt
TCP target = 1000 req/s
RTU target = 50 Hz
RTU timeout = 25 ms
W5500 SPI = 26 MHz
full runtime = activo
HMI dirty Master/Slave = activo
```

Resultado TCP:

```txt
ACHIEVED_REQ_S=912.95
ACHIEVED_PCT=91.295
RESULT=SATURATION_FAIL_CLEAN

TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0

LATENCY_AVG_US=1087.6
LATENCY_P95_US=1571.2
LATENCY_P99_US=14590.6
LATENCY_MAX_US=35877.3
LOOP_GAP_MAX_US=35375
```

Los periféricos permanecieron sanos:

```txt
FULL_RUNTIME_READY=YES
FRAM_FAILS=0
SD_APPEND_FAILS=0
SD_VERIFY_FAILS=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

RTU agregado:

```txt
RTU_TRAFFIC_DURATION_MS=600073
RTU_REQUESTS_STARTED=29986
RTU_REQUESTS_COMPLETED=29987
RTU_REQUESTS_SUCCESS=29986
RTU_REQUESTS_FAILED=1
RTU_MASTER_TIMEOUTS=1
RTU_CRC_ERRORS=0
RTU_VERIFY_FAILS=0
RTU_ACHIEVED_HZ=49.971
```

El Slave terminó con:

```txt
RTU_RX_FRAMES=30004
RTU_TX_FRAMES=30004
RTU_REQUESTS_OK=30004
RTU_CRC_ERRORS=0
RTU_EXCEPTIONS_SENT=0
```

### Interpretación

P5-D no se cierra:

```txt
TCP_RATE_PASS=NO
A14_P5B_AUTOMATED=REVIEW
P5D_LONG_RUN=NOT_CLOSED
```

Sin embargo, el caso TCP fue limpio. La caída es de capacidad sostenida, no de
integridad de transporte.

La diferencia:

```txt
60 s previo = 985.34 req/s
600 s intento 1 = 912.95 req/s
```

no permite decidir todavía entre:

```txt
A) degradación progresiva con el tiempo;
B) variabilidad entre sesiones;
C) interferencia periódica/ráfagas largas del full-runtime.
```

Se requiere segmentación temporal dentro de la misma conexión.

### Recurrencia F045 en lifecycle RTU

El resultado:

```txt
STARTED=29986
COMPLETED=29987
```

demuestra que el reset estadístico pudo ocurrir con una transacción previa aún
en vuelo.

El comando `R` detenía/reiniciaba el generador de tráfico, pero no esperaba a
que la transacción pendiente terminara antes de resetear contadores.

Esto es una recurrencia del principio F045:

```txt
COUNTER_RESET_IS_NOT_A_QUIESCENCE_BARRIER
```

No se crea un nuevo número de falla.

### Corrección P5-D-R1

El lifecycle queda:

```txt
X
wait 100 ms
R Master
R Slave
G
start formal window
...
end formal window
X
wait 100 ms
snapshot Master
snapshot Slave
```

Consecuencias:

- no hay request RTU anterior contaminando el inicio;
- RTU queda congelado antes de ambos snapshots;
- el cross-count esperado pasa de tolerancia 12 a exactitud 0;
- el snapshot serial completo ya no puede fabricar timeouts RTU posteriores.

### Segmentación TCP

El mismo long-run de 600 s ahora reporta, sin snapshots intermedios:

```txt
LONG_BUCKET INDEX=1  START_S=0   END_S=60  ...
LONG_BUCKET INDEX=2  START_S=60  END_S=120 ...
...
LONG_BUCKET INDEX=10 START_S=540 END_S=600 ...
```

Cada bucket conserva:

```txt
REQ_S
OK
LAT_AVG_US
P95_US
P99_US
MAX_US
```

Objetivo de la repetición:

```txt
si todos los buckets arrancan ~910:
    session-level saturation / variabilidad

si comienza ~980 y cae progresivamente:
    time-dependent degradation

si existen buckets altos/bajos aislados:
    interferencia periódica / jitter de runtime
```

No se cambia:

```txt
W5500 SPI
RTU timeout
periféricos
producto
target TCP
duración
```

Estado:

```txt
P5B_60S=CLOSED_PASS
P5D_ATTEMPT_1=SATURATION_FAIL_CLEAN
P5D_R1=READY_TO_RERUN_SEGMENTED
```


## P5-D-R1 — resultado segmentado

La repetición de 600 s con lifecycle RTU quiescente produjo:

```txt
ACHIEVED_REQ_S=912.75
ACHIEVED_PCT=91.275
RESULT=SATURATION_FAIL_CLEAN

TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
TCP_CLEAN=YES
TCP_RATE_PASS=NO
```

Buckets de 60 s:

```txt
1  = 937.13 req/s
2  = 922.48 req/s
3  = 903.63 req/s
4  = 895.53 req/s
5  = 888.40 req/s
6  = 911.67 req/s
7  = 941.00 req/s
8  = 921.05 req/s
9  = 907.13 req/s
10 = 899.47 req/s
```

Interpretación:

```txt
TIME_DEPENDENT_MONOTONIC_DEGRADATION=NO
SESSION_LEVEL_1000RPS_SATURATION=YES
PERIODIC_OR_RUNTIME_VARIABILITY=YES
```

La recuperación del minuto 7 descarta una degradación monotónica simple.

### RTU después de corregir F045

El lifecycle quiescente corrigió el contador contaminado:

```txt
STARTED=29975
COMPLETED=29975
```

pero permanecieron tres timeouts reales:

```txt
SUCCESS=29972
FAILED=3
MASTER_TIMEOUTS=3
MASTER_CRC=0
VERIFY_FAILS=0

SLAVE_RX=29975
SLAVE_TX=29975
SLAVE_OK=29975
SLAVE_CRC=0
SLAVE_EXCEPTIONS=0
```

Esto confirma recurrencia de F048 bajo ventana larga:

```txt
RTU_25MS_LONG_RUN=INSUFFICIENT_WITH_CURRENT_WORKLOAD
F045_LIFECYCLE_CONTAMINATION=FIXED
```

No se crea una clase nueva para estos timeouts.

## Hallazgo de workload SD

Al auditar el firmware P5 se detectó que la prueba full-runtime todavía usaba:

```cpp
JWPLCFile
write()
flush()
close()
open(FILE_READ)
seek()
read()
reopen(FILE_APPEND)
```

cada 1/5 s, en vez de la API bufferizada actual:

```cpp
JWPLCDataLog
```

El package ya integra el manager DataLog y lo atiende automáticamente desde
`jwplcSystemTask()` mediante:

```cpp
jwplcDataLogTickCallback();
JWPLC_SD.serviceDataLogs();
```

Por tanto, bajar el objetivo TCP o aumentar el timeout RTU antes de medir el
runtime con la API SD actual sería prematuro.

### P5-D-R2 — única variable

Se cambia únicamente el workload microSD del firmware diagnóstico:

```txt
ANTES:
JWPLCFile directo
write cada 1 s
flush cada 5 registros
close/read/reopen cada 5 s

AHORA:
JWPLCDataLog
buffer = 4096 B
commit threshold = 512 B
commit timeout = 5000 ms
servicio automático por jwplcSystemTask
```

Se mantienen sin cambios:

```txt
TCP target = 1000 req/s
duration = 600 s
RTU period = 20 ms
RTU timeout = 25 ms
W5500 SPI = 26 MHz
FRAM / RTC / buttons / TCA-I/O = activos
HMI Master/Slave = dirty on-demand
lifecycle RTU quiescente
```

La escritura lógica sigue ocurriendo a 32 B cada segundo. El loop ya no realiza
el write/flush/readback físico síncrono. El manager DataLog agrupa y persiste los
datos en segundo plano.

El snapshot publica:

```txt
SD_WORKLOAD_MODE=BUFFERED_DATALOG
SD_DATALOG_ACTIVE
SD_DATALOG_BUFFER_BYTES
SD_DATALOG_PENDING_BYTES
SD_DATALOG_COMMIT_THRESHOLD_BYTES
SD_DATALOG_COMMIT_TIMEOUT_MS
SD_DATALOG_ACCEPTED_BYTES
SD_DATALOG_COMMITTED_BYTES
SD_DATALOG_FAILED_COMMITS
```

El gate exige actividad real y:

```txt
SD_DATALOG_FAILED_COMMITS=0
SD_DATALOG_COMMITTED_BYTES>0
```

Objetivo de P5-D-R2:

```txt
determinar si el workload SD bloqueante explicaba:
- la saturación sostenida ~913 req/s;
- los service gaps >25 ms;
- los 3 timeouts RTU del long-run.
```

Estado:

```txt
P5B_60S=CLOSED_PASS
P5D_R1=SATURATION_FAIL_CLEAN
F045_RECURRENCE=FIXED
F048_25MS_LONG_RUN=REPRODUCED
P5D_R2_BUFFERED_SD=READY_TO_RUN
```


## P5-D-R2 — DataLog bufferizado expone core precompilado stale

La corrida de 600 s con `JWPLCDataLog` produjo un contraste muy fuerte:

```txt
TCP:
ACHIEVED_REQ_S=999.99
ACHIEVED_PCT=99.999
TCP_CLEAN=YES
TCP_RATE_PASS=YES

RTU:
STARTED=30002
COMPLETED=30002
SUCCESS=30002
FAILED=0
TIMEOUTS=0
CRC=0
VERIFY_FAILS=0
RTU_ACHIEVED_HZ=50.001
RTU_CROSS_COUNT_PASS=YES
```

Los 10 buckets TCP quedaron aproximadamente en 1000 req/s:

```txt
999.97
999.95
1000.08
1000.00
998.48
1001.38
999.98
1000.15
999.97
999.93
```

Sin embargo, la SD no realizó ningún commit físico:

```txt
SD_DATALOG_ACTIVE=YES
SD_DATALOG_BUFFER_BYTES=4096
SD_DATALOG_PENDING_BYTES=4096
SD_DATALOG_ACCEPTED_BYTES=4096
SD_DATALOG_COMMITTED_BYTES=0
SD_DATALOG_FAILED_COMMITS=0

SD_APPEND_CYCLES=600
SD_APPEND_FAILS=472
SD_VERIFY_CYCLES=120
SD_VERIFY_FAILS=95
```

La aritmética es consistente con un ring de 4096 B que nunca se drena:

```txt
4096 B / 32 B por registro = 128 registros aceptados
600 ciclos - 128 aceptados = 472 append fails
```

El manager DataLog no intentó commits: `failedCommits=0`, por lo que no se
trata de una SD que rechaza writes, sino de ausencia de servicio automático.

### Causa confirmada por historia de fuentes

El `core.a` actualmente usado por JWPLC Basic fue actualizado por última vez
en Alpha11:

```txt
core.a last commit:
64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
2026-09-09
```

En ese commit, `jwplcSystemTask()` no contiene:

```cpp
jwplcDataLogTickCallback();
```

La integración del servicio DataLog al runtime llegó después:

```txt
7e465e8e5e6efbbf0a15ef1c77c4042ed124825e
2026-09-14
feat(alpha14): integrar DataLog al runtime automatico
```

y añadió en `main.cpp`:

```cpp
#if JWPLC_HAS_SD
    jwplcDataLogTickCallback();
#endif
```

Por tanto el source actual y el archive precompilado divergen funcionalmente.

### F050

```txt
F050=PRECOMPILED_CORE_STALE_AFTER_RUNTIME_SOURCE_CHANGE
```

El hash antiguo:

```txt
6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68
```

deja de considerarse candidato final inmutable. Se conserva como baseline
histórica de NB3/P3, pero P5 ha demostrado que no representa el runtime source
actual.

No se reemplaza silenciosamente. Primero se ejecutará P5-F:

1. confirmar árbol limpio y hash baseline;
2. comprobar que el commit del archive precede al commit que añadió el callback;
3. regenerar `core.a` desde `cores/jwcontrol` con el mismo perfil
   `jwplcbasic`;
4. verificar el camino normal `jwcontrol_precompiled_stub + core.a`;
5. subir un probe físico de DataLog;
6. no llamar manualmente `JWPLC_SD.serviceDataLogs()`;
7. exigir `commitCount>0`, `committedBytes>0`, cero failed commits y cero
   write fails;
8. dejar el nuevo `core.a` sólo como working-tree candidate, sin stage ni
   commit, hasta revisar el resultado en chat.

Estado:

```txt
P5D_R2_TCP_1000RPS=PASS
P5D_R2_RTU50=PASS
P5D_R2_SD_PHYSICAL_COMMIT=FAIL_NOT_SERVICED
F049=P5_DIAGNOSTIC_CORRECTED
F050=CONFIRMED
P5F=READY_TO_RUN
```


## P5-F intento 1 — Get-FileHash no disponible

El primer intento P5-F se detuvo antes de cualquier rebuild:

```txt
TRACKED_DIRTY_BEFORE=0
STAGED_BEFORE=0

Get-FileHash:
CommandNotFoundException
```

El fallo ocurrió en la primera lectura del SHA baseline:

```txt
P5F_CORE_SHA_BEFORE=<no alcanzado>
REBUILD_CORE=<no iniciado>
CORE_A_MUTATION=NO
```

Clasificación:

```txt
F051=POWERSHELL_GET_FILEHASH_UNAVAILABLE_ON_LEGACY_HOST
PRODUCT_FAILURE=NO
CORE_REBUILD_STARTED=NO
```

### Corrección

No se instala ningún módulo ni se exige actualizar PowerShell.

Los tres scripts que participan en el camino P5-F dejan de depender de
`Get-FileHash`:

```txt
tools/modbus-tcp-benchmark/gates/
  a14_p5f_rebuild_core_datalog_autoservice.ps1

tools/build-speed-benchmark/
  Build-JWPLCPrecompiledCore.ps1
  Verify-JWPLCPrecompiledCore.ps1
```

Se añade una función local basada únicamente en .NET:

```powershell
[System.Security.Cryptography.SHA256]::Create()
[System.IO.File]::OpenRead(...)
```

La semántica se conserva:

```txt
SHA256 exacto del archivo
comparación baseline/candidato
sin dependencia de cmdlets opcionales
```

Estado:

```txt
P5F_ATTEMPT_1=TOOLING_COMPATIBILITY_FAIL
CORE_A_MUTATION=NO
P5F_R1=READY_TO_RERUN
```


## P5-F intento 2 — compile_commands path format

El segundo intento superó F051 y confirmó correctamente:

```txt
P5F_CORE_SHA_BEFORE=
6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68

P5F_SOURCE_DATALOG_TICK_CALL_COUNT=1

P5F_CORE_LAST_COMMIT=
64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218

P5F_DATALOG_CALLBACK_INTRO_COMMIT=
7e465e8e5e6efbbf0a15ef1c77c4042ed124825e

P5F_CORE_COMMIT_BEFORE_CALLBACK_COMMIT=True
P5F_CALLBACK_COMMIT_IN_CORE_HISTORY=False
```

Por tanto la hipótesis de F050 quedó nuevamente corroborada antes del rebuild.

El build fuente sí comenzó, pero se detuvo en:

```txt
Get-CompileDatabaseInfo
Path.GetFullPath(entry.file)
NotSupportedException:
"No se admite el formato proporcionado de la ruta de acceso."
```

El fallo ocurre durante la auditoría de `compile_commands.json`, antes de que
el candidato se copie al archive oficial.

Estado del artefacto:

```txt
SOURCE_BUILD_STARTED=YES
COMPILE_DB_AUDIT=FAIL
CORE_A_REPLACED=NO
BOARDS_LOCAL_RESTORED=YES
```

### F052

```txt
F052=COMPILE_DB_PATH_NORMALIZATION_ASSUMED_WINDOWS_GETFULLPATH
```

La clasificación de TUs sólo necesita distinguir:

```txt
/cores/jwcontrol/
/cores/jwcontrol_precompiled_stub/
```

No necesita resolver `entry.file` a una ruta física absoluta.

Corrección:

- no usar `Path.GetFullPath(entry.file)`;
- no usar `Path.IsPathRooted(entry.file)`;
- no usar `Path.GetFileName(entry.file)` para estos TUs;
- limpiar comillas residuales;
- normalizar `\` a `/`;
- si el file es relativo, concatenar textualmente `directory/file`;
- clasificar por regex sobre texto normalizado.

Se aplica tanto a:

```txt
Build-JWPLCPrecompiledCore.ps1
Verify-JWPLCPrecompiledCore.ps1
```

### Corrección adicional de observabilidad

`Invoke-NativeCaptured` del gate P5-F enviaba el stdout del child PowerShell
por el pipeline de retorno. Por ello:

```txt
P5F_CORE_REBUILD_EXIT=
<log completo ...> 1
```

en lugar de un entero.

No fue la causa del fallo, pero se corrige para que:

```txt
P5F_CORE_REBUILD_EXIT=<int>
P5F_CORE_VERIFY_EXIT=<int>
```

y el stdout continúe visible y guardado en log sin contaminar el return value.

Estado:

```txt
P5F_ATTEMPT_2=TOOLING_COMPILE_DB_COMPATIBILITY_FAIL
CORE_A_REPLACED=NO
F052=CONFIRMED_AND_CORRECTED
P5F_R2=READY_TO_RERUN
```


## P5-F intento 3 — sintaxis rota en script anidado

El tercer intento superó:

```txt
P5F_CORE_SHA_BEFORE=6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68
P5F_SOURCE_DATALOG_TICK_CALL_COUNT=1
P5F_CORE_COMMIT_BEFORE_CALLBACK_COMMIT=True
P5F_CALLBACK_COMMIT_IN_CORE_HISTORY=False
```

pero el script anidado:

```txt
tools/build-speed-benchmark/Build-JWPLCPrecompiledCore.ps1
```

no pudo parsearse.

Error:

```txt
ParserError
line 361
cadena sin terminador
llave de cierre faltante
```

La línea dañada provenía del patch textual que reemplazó la detección de
`peripherals_init.cpp`:

```txt
.Replace('\', '/') -match '/peripherals_init\.cpp
```

y perdió terminadores durante la transformación.

El fallo ocurre antes de ejecutar el cuerpo del builder, por lo que:

```txt
CORE_REBUILD_STARTED=NO
CORE_A_REPLACED=NO
PRODUCT_MUTATION=NO
```

### F053

```txt
F053=NESTED_POWERSHELL_SCRIPT_SYNTAX_REGRESSION_NOT_PREFLIGHTED
```

La debilidad principal no es sólo la línea mal transformada. El wrapper P5-F
validaba sintaxis únicamente de:

```txt
a14_p5f_rebuild_core_datalog_autoservice.ps1
```

pero no de los dos scripts PowerShell que éste ejecuta:

```txt
Build-JWPLCPrecompiledCore.ps1
Verify-JWPLCPrecompiledCore.ps1
```

### Corrección

La clasificación de nombres de TU deja de usar regex construidas mediante
patches frágiles y pasa a:

```powershell
([string]$_).
    Replace([char]92, [char]47).
    EndsWith("/peripherals_init.cpp")
```

y:

```powershell
([string]$_).
    Replace([char]92, [char]47).
    EndsWith("/precompiled_core_stub.c")
```

También se usan `[char]92` y `[char]47` para normalizar separadores, evitando
escapes ambiguos al generar scripts.

El wrapper P5-F ahora valida sintaxis, antes de cualquier ejecución, de los tres
scripts:

```txt
a14_p5f_rebuild_core_datalog_autoservice.ps1
Build-JWPLCPrecompiledCore.ps1
Verify-JWPLCPrecompiledCore.ps1
```

Si cualquiera falla, el wrapper termina antes de que P5-F pueda tocar el
archive.

Estado:

```txt
P5F_ATTEMPT_3=NESTED_SCRIPT_SYNTAX_FAIL
F053=CONFIRMED_AND_CORRECTED
CORE_A_REPLACED=NO
P5F_R3=READY_TO_RERUN
```


### Restauración estructural posterior

Al revisar el estado posterior al intento 3 se detectó que
`Verify-JWPLCPrecompiledCore.ps1` había quedado con bloques duplicados por las
transformaciones textuales anteriores.

Para evitar seguir parchando sobre una base corrupta se tomó como referencia la
última versión estructuralmente sana:

```txt
d76fe851c811cbb422a0c12d59fe0296d0b998ba
```

y se reconstruyeron ambos scripts desde esa base:

```txt
Build-JWPLCPrecompiledCore.ps1
Verify-JWPLCPrecompiledCore.ps1
```

Reaplicando únicamente:

```txt
- SHA-256 por .NET;
- parser compile_commands textual/portable;
- clasificación de TU mediante -like;
- sin Get-FileHash;
- sin GetFullPath(entry.file);
- sin IsPathRooted(entry.file).
```

Estado estructural actual:

```txt
Build:
  Get-Sha256Hex count = 1
  Get-CompileDatabaseInfo count = 1
  líneas ≈ 448

Verify:
  Get-Sha256Hex count = 1
  Get-CompileDatabaseInfo count = 1
  líneas ≈ 415

Wrapper:
  valida sintaxis de gate + builder + verifier
```

Esto se considera corrección de F053, no una clase nueva.

### P5-F intento 4 — recurrencia F052 en auditoría de TU

La reejecución física sobre `fcf087f6` superó los tres preflights PowerShell y
confirmó nuevamente el baseline stale, pero el builder abortó después del build
fuente con:

```txt
Se esperaba exactamente 1 peripherals_init.cpp compilado; obtenido: 0
P5F_CORE_REBUILD_EXIT=1
```

Diagnóstico sobre el `compile_commands.json` real de la misma corrida:

```txt
COMPILE_DB_ENTRIES=80
FILE_CORE_COUNT=64
COMMAND_CORE_COUNT=80
PERIPHERALS_FILE_FIELD_COUNT=1
PERIPHERALS_CANDIDATE_COUNT=1
PERIPHERALS_COMMAND_COUNT=1
PERIPHERALS_OBJECT_COUNT=2
peripherals_init.cpp.o = PRESENT
```

Conclusión:

```txt
PRODUCT_FAILURE=NO_EVIDENCE
SOURCE_BUILD_PERIPHERALS_INIT=CONFIRMED
CORE_A_REPLACED=NO
F054=NOT_CREATED
F052=RECURRENCE_HARDENED
```

La causa fue una segunda interpretación de `SourceFiles` después de que
`Get-CompileDatabaseInfo()` ya había clasificado correctamente los TUs.

Corrección:

```txt
- contar peripherals_init.cpp dentro del mismo parser/candidateText;
- contar precompiled_core_stub.c dentro del mismo parser/candidateText;
- usar EndsWith(..., OrdinalIgnoreCase);
- exponer PeripheralsInitCount y PrecompiledStubCount;
- no volver a filtrar SourceFiles/StubFiles para esos invariantes.
```

P5-F sigue siendo el único gate abierto; no se avanzó a upload ni probe físico.

### P5-F intento 5 — el path es válido; aislar transporte de CompileDb

El diagnóstico R2 sobre la corrida `20260925_084826` demostró que el valor real
de `entry.file` es un path Windows normal y que las tres representaciones
evaluadas cumplen la condición esperada:

```txt
HIT_COUNT=1
RAW_REGEX=True
FILE_REGEX=True
CANDIDATE_REGEX=True
RAW_ENDSWITH=True
FILE_ENDSWITH=True
CANDIDATE_ENDSWITH=True
```

El TU exacto termina en:

```txt
/cores/jwcontrol/peripherals_init.cpp
```

Por tanto queda descartado que el segundo `0` sea causado por el shape del path.

La frontera restante está entre `Get-CompileDatabaseInfo()` y el objeto que
retorna `Invoke-ArduinoCompile()`. Para eliminar esa ambigüedad, builder y
verifier ahora:

```txt
1. capturan Get-CompileDatabaseInfo en $compileDbInfo;
2. imprimen tipo y contadores inmediatamente;
3. asignan CompileDb = $compileDbInfo;
4. mantienen el gate posterior usando esa misma instancia.
```

No se crea todavía F054: la clase final se decide con la siguiente evidencia
del mismo P5-F.

### P5-F intento 6 — F054: array JSON encapsulado como una sola entrada

La corrida sobre `1eac82d5` añadió observabilidad justo después de
`Get-CompileDatabaseInfo()` y produjo:

```txt
compile_commands parser:
type=System.Management.Automation.PSCustomObject
entries=1
jwcontrol=1
stub=0
peripherals_init=0
precompiled_stub=0
```

Esto contradice la inspección directa del mismo tipo de artefacto, que había
demostrado un `compile_commands.json` con 80 entradas y un
`peripherals_init.cpp` real.

Además, el diagnóstico exacto del path había demostrado:

```txt
RAW_ENDSWITH=True
FILE_ENDSWITH=True
CANDIDATE_ENDSWITH=True
```

Por tanto el fallo ya no pertenece a F052 de normalización de paths.

Se crea:

```txt
F054=POWERSHELL_CONVERTFROMJSON_ARRAY_COLLAPSED_TO_SINGLE_COMPILE_DB_ENTRY
```

Causa:

```txt
La forma devuelta por ConvertFrom-Json en el host PowerShell usado por el gate
puede llegar como colección directa o como array encapsulado dentro de una sola
salida. El parser asumía una sola forma y terminó iterando el compile DB como
una única entrada.
```

Corrección:

```txt
- capturar primero $parsedEntries;
- normalizar explícitamente la colección;
- aplanar un nivel cuando la salida sea Array/IEnumerable sin propiedad file;
- exigir al menos una entrada normalizada;
- mantener la clasificación textual portable de F052;
- mantener los contadores directos de TU añadidos en P5-F.
```

Estado:

```txt
PRODUCT_FAILURE=NO_EVIDENCE
CORE_A_REPLACED=NO
UPLOAD_REACHED=NO
F052=REMAINS_CORRECTED_FOR_PATHS
F054=CONFIRMED_AND_CORRECTED
P5F=READY_TO_RERUN
```

### P5-F cierre y adopción del core precompilado

El gate P5-F completó PASS físico de punta a punta después de corregir F054.

Resultado del core regenerado:

```txt
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
P5F_PROBE_COMPILE_EXIT=0
P5F_PROBE_UPLOAD_EXIT=0
P5F_COMMITTED_BYTES=12288
P5F_COMMIT_COUNT=24
P5F_FAILED_COMMITS=0
P5F_WRITE_FAILS=0
P5F_MANUAL_SERVICE_CALL=NO
A14_P5F_DATALOG_RESULT=PASS
P5F_AUTOSERVICE_PHYSICAL=PASS
```

SHA adoptado:

```txt
OLD_CORE_SHA256=6EDF40D105936318A2FD8A84D7F0724571657910E8D92E8538640EC613F4DD68
NEW_CORE_SHA256=8BCE2CD02F93D6E303E91CB900E465BA36661196D37003E7FAF140D3DC5359FF
```

El candidato fue adoptado en:

```txt
1cd012be41fa27797fc28caf9f54b8eba480a94f
fix(alpha14): actualizar core precompilado con autoservicio DataLog
```

El hash protegido de `common.ps1` se actualiza al nuevo core validado antes
de repetir P5-D. El hash viejo se conserva únicamente como evidencia histórica
del stale core que permitió confirmar F050.

Estado:

```txt
P5_F_STATUS=CLOSED_PASS
F050=ROOT_CAUSE_CONFIRMED
F054=CLOSED
CORE_PRECOMPILED_ADOPTED=YES
NEXT=P5_D_RERUN_600S_WITH_PHYSICAL_DATALOG
```

### P5-E — caracterización final de margen Modbus TCP

Después de P5-D PASS, el objetivo deja de ser perseguir throughput por sí mismo.
La prioridad es confirmar que el target industrial de 1000 req/s tiene margen y
que cualquier optimización que se adopte quede dentro del package/core o una API
JWPLC simple.

Regla de diseño:

```txt
NO_USER_COMPLEX_SCHEDULER=YES
NO_REQUIRED_DOUBLE_TASK_IN_SKETCH=YES
PACKAGE_CORE_FIRST=YES
PUBLIC_API_SIMPLE=YES
```

Secuencia definida:

```txt
P5-E1 = full-runtime FC03/125 unpaced ceiling, diagnóstico sin mutación.
P5-E2 = A/B de servicio TCP interno sólo si E1 queda por debajo de 1000 req/s.
P5-E3 = congelar objetivo soportado, API/configuración y documentación.
Después = cierre P5 / cierre documental Alpha14.
```

Criterio P5-E1:

```txt
- mismo firmware full-runtime;
- Display + FRAM + RTC + SD DataLog + botones + I/O;
- RTU Master 50 Hz / timeout 25 ms;
- W5500 26 MHz;
- FC03 125 registros;
- un único request TCP pendiente;
- sin rate limiter/sleep;
- ventana 60 s;
- cero errores TCP/Modbus;
- RTU y periféricos deben seguir PASS;
- DataLog debe tener committed bytes > 0 y failed commits = 0.
```

Interpretación de headroom:

```txt
>= 1100 req/s  -> COMFORTABLE, cancelar P5-E2.
1000..1099     -> POSITIVE, cancelar P5-E2.
< 1000 req/s   -> BELOW_1000, revisar P5-E2 interno.
```

P5-E1 es de caracterización: no modifica producto ni cambia APIs.

### P5-E1-R1 — repeatability long-run

Para descartar que el techo de 1017.170 req/s observado en P5-E1 sea una
muestra aislada, se define una repetición continua de 600 s sin mutación de
producto.

```txt
DURATION_S=600
BUCKET_SECONDS=60
TCP_PACING=NONE
TCP_OUTSTANDING_REQUESTS=1
FC03_QUANTITY_REGISTERS=125
FULL_RUNTIME=YES
RTU_TARGET_HZ=50
SD_DATALOG=BUFFERED_AUTOSERVICE
PRODUCT_SOURCE_MUTATION=NO
```

El runner P5-E1 conserva el resultado agregado y añade diez buckets de 60 s:

```txt
P5E1_BUCKET INDEX=n START_S=... END_S=... OK=... REQ_S=...
```

Criterio de interpretación:

```txt
- agregado >= 1000 req/s: capacidad sostenida positiva;
- cero errores TCP/Modbus;
- RTU 50 Hz sin timeout/CRC;
- DataLog con commits y sin failed commits;
- periféricos sin fallos;
- revisar buckets para descartar degradación progresiva.
```

P5-E1-R1 no sustituye P5-D; confirma repetibilidad del techo unpaced.

### P5-E2 — candidato cached socket state

P5-E1-R1 de 600 s confirmó un techo full-runtime sostenido de:

```txt
ACHIEVED_REQ_S=981.171
USEFUL_MBPS=1.9623
TOTAL_MODBUS_MBPS=2.1272
TCP_CLEAN=YES
RTU_ACHIEVED_HZ=50.000
SD_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
```

El objetivo de producto para JWPLC Basic queda acotado a 1000 req/s sostenidos,
con margen deseado aproximado de 1020..1050 req/s. No se persigue más throughput
en Alpha14 una vez alcanzado ese rango.

Candidato P5-E2:

```txt
SCOPE=JWPLC_ModbusTCP internal server RX hot path
PUBLIC_API_CHANGE=NO
USER_LOOP_COMPLEXITY_CHANGE=NO
CORE_A_REBUILD_REQUIRED=NO
W5500_SPI_HZ=26000000
```

Optimización:

```txt
1. No consultar EthernetClient::connected() cuando available() ya indicó RX.
2. Después de read(), descontar bytes del available ya conocido.
3. Volver a consultar available() sólo al agotar el saldo conocido y si el ADU
   todavía no está completo.
```

Motivo: cada llamada de estado al socket implica trabajo SPI/W5500. El request
FC03/125 del benchmark llega como un ADU pequeño de 12 bytes y el parser lo
divide primero en MBAP de 6 bytes y luego PDU; evitar sondeos redundantes reduce
overhead por transacción sin alterar framing, budgets, mutex ni API.

Gate:

```txt
P5-E2 candidate qualification = 60 s unpaced full-runtime.
Target deseado = 1020..1050 req/s.
Si mejora claramente, repetir 600 s antes de adoptar/cerrar.
Si no mejora, revisar/revertir el candidato antes de otra estrategia.
```

### P5-E2 intento 1 — F055: preflight dependiente de comentario Unicode

El primer intento P5-E2 se detuvo antes de compilar/medir:

```txt
DEFER_CONNECTED_WHEN_RX_PENDING=True
CACHE_AVAILABLE_BYTES_AFTER_READ=True
REFRESH_AVAILABLE_ONLY_WHEN_EXHAUSTED=False
P5E2_SOURCE_CONTRACT_FAILED_REFRESH_AVAILABLE_ONLY_WHEN_EXHAUSTED
```

La inspección del source remoto confirmó que el bloque candidato sí estaba
presente. El falso negativo provenía de validar una frase de comentario con
carácter Unicode acentuado mediante `String.Contains()` en Windows PowerShell.

Clasificación:

```txt
F055=COMMENT_UNICODE_DEPENDENT_PREFLIGHT_FALSE_NEGATIVE
TYPE=TOOLING_FAILURE
PRODUCT_FAILURE=NO
P5E2_CANDIDATE_EXECUTED=NO
```

Corrección:

```txt
- dejar de validar comentarios;
- contar exactamente dos llamadas _client.available();
- exigir exactamente una llamada _client.connected();
- exigir availableBytes -= received;
- mantener el chequeo estructural if (availableBytes <= 0).
```

El candidato de producto P5-E2 no se modifica en este intento; se repite el
mismo gate de 60 s después de corregir únicamente el preflight.

### P5-E2 candidato 2 — autoservicio package-core pre/post loop

El candidato `cached socket state` quedó funcionalmente PASS pero no confirmó
ganancia frente al baseline corto:

```txt
P5-E1 baseline 60 s = 1017.170 req/s
P5-E2 cached 60 s   = 1006.592 req/s
GAIN_CONFIRMED=NO
```

Por aislamiento experimental, ese cambio de hot-path se revierte a la lógica
baseline antes del candidato 2.

Objetivo del candidato 2:

```txt
TARGET_BASIC=1020..1050 req/s sostenibles
PUBLIC_API_CHANGE=NO
USER_MANUAL_TASK_REQUIRED=NO
CORE_INTEGRATION=YES
```

Diseño:

```txt
package-core loopTask:
    jwplcModbusTCPLoopServiceCallback()
    loop() del usuario
    jwplcModbusTCPLoopServiceCallback()

JWPLC_ModbusTCP enlazado:
    callback fuerte -> JWPLC_ModbusTCP.task()

JWPLC_ModbusTCP no enlazado:
    callback débil vacío
```

El firmware full-runtime elimina su llamada manual a
`JWPLC_ModbusTCP.task()`. Por tanto, la medición valida la experiencia deseada
del package: el usuario configura mapas + `beginServer()` y el servicing
queda a cargo del ecosistema JWPLC.

Como `main.cpp` forma parte de `core.a`, el gate reconstruye y verifica un
core candidato local. El binario queda dirty de forma controlada y NO se adopta
ni se protege con nuevo hash hasta revisar el resultado físico.

### P5-E2 intento 2a — F056: escapes de backslash corrompieron wrapper BAT

El primer lanzamiento del candidato `CORE_PRE_POST_LOOP_AUTOSERVICE` se detuvo
durante la validación de sintaxis, antes de reconstruir `core.a`.

Salida clave:

```txt
POWERSHELL_SYNTAX=PASS
Resolve-Path : Caracteres no válidos en la ruta de acceso.
```

La inspección del wrapper remoto mostró que secuencias Windows con backslash
habían sido interpretadas al generar el archivo: `\t` se convirtió en tab,
`\b` en backspace y el tramo `..\..\..` quedó mutilado.

Clasificación:

```txt
F056=GENERATED_BAT_BACKSLASH_ESCAPE_CORRUPTION
TYPE=TOOLING_FAILURE
PRODUCT_FAILURE=NO
CORE_REBUILD_STARTED=NO
P5E2_CANDIDATE_EXECUTED=NO
```

Corrección:

```txt
- regenerar el BAT preservando backslashes literalmente;
- usar rutas directas desde %~dp0;
- comprobar existencia de cada PS1 antes de invocar el parser;
- verificar ausencia de caracteres de control en el wrapper generado.
```

El source del candidato P5-E2 no cambia.

### P5-E2-R1 — autoservicio package-core long-run 600 s

El candidato 2 alcanzó 1025.750 req/s durante 60 s con TCP limpio, RTU 50 Hz,
DataLog sin fallos y TFT estable. Antes de adoptar el nuevo `core.a`, se exige
una repetición larga con el mismo binario candidato:

```txt
CORE_SHA256=4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566
DURATION_S=600
BUCKET_SECONDS=60
FC03_QUANTITY=125
TCP_PACING=NONE
TCP_OUTSTANDING_REQUESTS=1
RTU_TARGET_HZ=50
W5500_SPI_HZ=26000000
USER_MANUAL_TCP_TASK_REQUIRED=NO
PRODUCT_ADOPTION=NOT_YET
```

Criterio:

```txt
>= 1020 req/s agregado : objetivo deseado cumplido.
1000..1019 req/s       : meta Basic de 1000 sostenidos cumplida.
< 1000 req/s           : mejora insuficiente para adopción por rendimiento.
```

Se mantienen como condiciones obligatorias cero errores TCP/Modbus, RTU sin
timeout/CRC, DataLog sin failed commits, periféricos sin fallos y TFT física
estable.

El runner corrige además el artefacto de bucket extra: los buckets se basan en
la duración nominal solicitada. Un request iniciado antes del deadline pero
completado unas fracciones después queda en el último bucket válido. Por tanto,
600 s / 60 s debe producir exactamente 10 buckets.

### P5-CAP1 — capacidad Ethernet con carga industrial activa

Objetivo: separar dos métricas:

```txt
MAX_REQ_S = máximo de transacciones Modbus TCP por segundo.
MAX_USEFUL_MBPS = máximo caudal útil de registros FC03 por segundo.
```

Se conserva el mismo full-runtime validado y el mismo core autoservice candidato.
No se reduce carga de periféricos.

Perfil:

```txt
TFT telemetry           = 10 Hz, USER_REFRESH_ON_DEMAND / dirty redraw
I/O sample              = 50 Hz
Buttons                 = 50 Hz
Modbus RTU Master       = 50 Hz
FRAM                    = 4 ciclos/s
FRAM per cycle          = 32 B write + 32 B read + 32 B restore
FRAM payload            = 384 B/s
RTC cache sample        = 4 Hz
microSD record          = 1 Hz x 32 B = 32 B/s lógicos
DataLog buffer          = 4096 B
DataLog threshold       = 512 B
DataLog timeout         = 5000 ms
SD verify/status        = 0.2 Hz
SPI mutex probe         = 10 Hz
W5500                   = 26 MHz
```

Sweep FC03 unpaced, un request pendiente:

```txt
Q = 1, 8, 16, 32, 64, 125 registers
duration = 30 s por punto
```

Para FC03:

```txt
request application bytes      = 12 B
response application bytes     = 9 + 2*Q B
useful register bytes/response = 2*Q B
transaction application bytes  = 21 + 2*Q B
```

Q pequeño favorece req/s; Q grande favorece Mbps útiles. P5-CAP1 caracteriza
ambos máximos bajo carga periférica representativa sin modificar el producto.

### P5-CAP2 — último test: bloques lógicos 125/250/500/1000

CAP1 mostró una relación casi lineal entre cantidad de registros FC03 y tiempo de
transacción. Ajuste descriptivo sobre los seis puntos medidos:

```txt
latencia media aproximada = 450.7 us + 4.25 us * registros
R2 aproximado = 0.998
```

Como FC03 limita cada request a 125 registros, CAP2 mantiene requests estándar
y construye bloques lógicos:

```txt
125 regs  = 1 x FC03(125)
250 regs  = 2 x FC03(125)
500 regs  = 4 x FC03(125)
1000 regs = 8 x FC03(125)
```

El mapa Holding del firmware de benchmark se amplía de 125 a 1000 registros sólo
para esta caracterización. No cambia la librería ni el producto.

Métricas principales:

```txt
block scans/s
requests/s
registers/s
useful Mbps
latencia media/P95/P99 del scan lógico completo
RTU Hz
SD/periféricos
```

Objetivo: comprobar si el techo observado de aproximadamente 125k registros/s se
mantiene al agrupar varios FC03 máximos en scans lógicos mayores. Este será el
último test de capacidad antes del cierre P5.

### P5-RTU/TCP BALANCE — techo RTU sin sacrificar Ethernet

El baseline RTU de 50 Hz estaba impuesto por un periodo de 20 ms; no era un
techo medido. Se añade instrumentación sólo al firmware de benchmark para variar
el target RTU en runtime, manteniendo 50 Hz como default.

Sweep conjunto:

\`\`\`txt
TCP = FC03/125 unpaced, 1 outstanding
RTU = OFF, 50, 100, 150, 200, 250, 300 Hz y UNPACED
baud = 115200 8N1
slave frame gap = 2 ms durante el sweep
duration = 30 s por punto
\`\`\`

Se registra RTU achieved Hz, TCP req/s, retención/caída TCP frente a RTU OFF,
latencias TCP, RTU CRC/timeouts/failures, SD y periféricos.

El runner reporta el máximo RTU observado conservando al menos 99%, 95% y 90%
del throughput Ethernet sin RTU. La selección final se hará con la curva medida,
no maximizando un bus a costa del otro.

