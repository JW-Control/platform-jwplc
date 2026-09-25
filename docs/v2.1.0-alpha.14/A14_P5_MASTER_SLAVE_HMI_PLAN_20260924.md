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
