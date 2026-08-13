# JWPLC Alpha5 — Arquitectura temporal y decisiones de diseño para Remote I/O

Fecha de decisión: **2026-08-13**

Repositorio:

```txt
JW-Control/platform-jwplc
```

Branch de trabajo:

```txt
v2.1.0-alpha.5/feature/openplc-backplane-validation
```

Commit previo relevante:

```txt
b290f26 test(remote-io): agregar barrido individual de salidas RTU
```

## 1. Objetivo de este documento

Este documento registra las decisiones de arquitectura, temporización, escalabilidad y seguridad acordadas durante la validación de **JWPLC Remote I/O por RS-485 / Modbus RTU**.

Debe usarse como documento de transferencia para cualquier persona que continúe el trabajo de Alpha5, de forma que no sea necesario reconstruir nuevamente las decisiones desde conversaciones anteriores.

Las prioridades siguen siendo:

1. estabilidad;
2. compatibilidad Arduino IDE / Arduino CLI;
3. no romper APIs ya probadas;
4. mantener OpenPLC separado del funcionamiento Arduino normal;
5. registrar decisiones y resultados de banco;
6. cerrar cada nivel antes de avanzar al siguiente.

---

# 2. Estado actual validado de Alpha5

## 2.1 Configuración PoC vigente

La PoC Arduino ↔ Arduino actualmente validada usa:

```txt
Transporte: JWPLC_RS485
Protocolo: Modbus RTU
UART: Serial2
RX: GPIO16
TX: GPIO17
Baudrate: 115200
Formato: 8N1
Slave ID de banco: 2
```

El `Slave ID = 2` es el valor de la PoC actual y **no debe interpretarse todavía como dirección final de producto**.

Funciones Modbus validadas en la PoC:

```txt
FC01  Read Coils
FC02  Read Discrete Inputs
FC05  Write Single Coil
FC15  Write Multiple Coils
```

El Slave de la PoC también contiene soporte adicional de funciones Modbus, pero Alpha5 está cerrando primero el contrato mínimo requerido para Remote I/O digital.

---

## 2.2 Pruebas Arduino Master ↔ Arduino Slave cerradas

Estado registrado:

```txt
ARDUINO_MASTER_SLAVE_PROTOCOL=PASS
FC01=PASS
FC02=PASS
FC05=PASS
FC15=PASS
CRC=PASS
SLAVE_ID_2=PASS
115200_8N1=PASS
```

### Entradas digitales físicas

Se verificaron las ocho entradas una por una:

```txt
I0_0 -> 0x01
I0_1 -> 0x02
I0_2 -> 0x04
I0_3 -> 0x08
I0_4 -> 0x10
I0_5 -> 0x20
I0_6 -> 0x40
I0_7 -> 0x80
```

Resultado:

```txt
FC02_PHYSICAL_INPUT_MAPPING=PASS
PHYSICAL_DI_8CH=PASS
BIT_SHIFT=NONE
LOGIC_INVERSION=NONE
```

### Salidas digitales físicas

Se agregó al Master el comando de prueba:

```txt
w : walking test Q0_0..Q0_7 individual
```

Se verificaron las ocho salidas una por una con FC05 y readback por FC01:

```txt
Q0_0 -> 0x01
Q0_1 -> 0x02
Q0_2 -> 0x04
Q0_3 -> 0x08
Q0_4 -> 0x10
Q0_5 -> 0x20
Q0_6 -> 0x40
Q0_7 -> 0x80
```

Resultado:

```txt
FC05_8CH=PASS
FC01_FEEDBACK_8CH=PASS
PHYSICAL_DO_8CH=PASS
OUTPUT_BIT_MAPPING=PASS
ARDUINO_MASTER_SLAVE_PHYSICAL_IO=PASS
```

Todas las salidas quedan apagadas al finalizar el walking test.

---

## 2.3 Recuperación de comunicación validada

### Desconexión y reconexión RS-485

Con ambos equipos encendidos se desconectó el enlace RS-485 y se confirmó:

```txt
BUS_DISCONNECT_DETECTED=PASS
TIMEOUT_ON_DISCONNECT=PASS
MASTER_NO_CRASH=PASS
MASTER_NO_FREEZE=PASS
RECONNECT_WITHOUT_RESET=PASS
COMMUNICATION_RECOVERY=PASS
CRC_AFTER_RECOVERY=PASS
```

La comunicación se recuperó al siguiente intercambio válido después de reconectar el bus, sin reiniciar Master ni Slave.

### Reset del Slave

Se encendió `Q0_0`, se confirmó `0x01`, se reinició solamente el Slave y se observó un timeout temporal durante el reinicio.

Al terminar el arranque del Slave:

```txt
FC01 -> 0x00
FC02 -> respuesta válida
```

Resultado:

```txt
RESET_SLAVE=PASS
TIMEOUT_DURING_SLAVE_RESET=PASS
RECOVERY_AFTER_SLAVE_RESET=PASS
SLAVE_OUTPUTS_OFF_AFTER_RESET=PASS
SLAVE_STARTUP_OUTPUT_STATE=0x00
```

### Reset del Master

Con el Slave activo se reinició exclusivamente el Master.

Después del arranque:

```txt
FC01=PASS
FC02=PASS
FC05=PASS
```

Resultado:

```txt
RESET_MASTER=PASS
MASTER_RESTART=PASS
RECOVERY_AFTER_MASTER_RESET=PASS
```

---

# 3. Política actual y política objetivo ante pérdida del Master

## 3.1 Comportamiento actual medido

Se encendió `Q0_0` y luego se desconectó/apagó únicamente el Master.

El Slave mantuvo físicamente la salida activa.

Comportamiento actual:

```txt
MASTER_LOSS_CURRENT_POLICY=HOLD_LAST_STATE
```

Este comportamiento corresponde a la PoC actual y **no se adopta como política final de producto**.

## 3.2 Política objetivo aprobada

Se adopta como política objetivo:

```txt
SAFE_OFF_AFTER_COMM_TIMEOUT
```

Es decir:

```txt
pérdida prolongada de comunicación válida
-> FAILSAFE_ACTIVE
-> Q0_0..Q0_7 = OFF
```

Valor base acordado para Alpha5:

```txt
Tsafety DO = 100 ms
```

Con un `Tremote DI/DO = 20 ms`, equivale aproximadamente a tolerar cinco ciclos digitales consecutivos sin comunicación válida.

El valor deberá validarse en banco antes de convertirse en especificación definitiva de producto.

---

# 4. Principio de arquitectura temporal

No debe existir un único tiempo de ciclo que gobierne adquisición, lógica PLC, RS-485 y diagnóstico.

Se adopta formalmente la separación:

```txt
Tsample   -> tiempo entre adquisiciones físicas
Tmodule   -> procesamiento interno del módulo
Tremote   -> actualización de la imagen remota
Tplc      -> ciclo de ejecución de la lógica PLC
Tdiag     -> actualización de diagnóstico
Tsafety   -> timeout de seguridad por pérdida de comunicación
Tresponse -> latencia real evento físico -> acción física
```

Estos tiempos son independientes y deben medirse/documentarse por separado.

---

# 5. Configuración temporal base adoptada

La siguiente configuración se adopta como **objetivo base de arquitectura** para las siguientes pruebas de Alpha5:

| Parámetro | Valor base | Función |
|---|---:|---|
| `Tsample AI` | **1 ms / 1000 SPS por canal** | adquisición física del ADC |
| `Tmodule` | **5 ms** | filtrado, escalado, min/max, alarmas y procesamiento local |
| `Tremote DI/DO` | **20 ms** | sincronización de imagen digital remota |
| `Tremote AI/AO` | **50 ms** | sincronización de imagen analógica remota |
| `Tplc` | **5 ms** | objetivo de ejecución del runtime PLC |
| `Tdiag` | **500 ms** | diagnóstico, contadores y estado no crítico |
| `Tsafety DO` | **100 ms** | pérdida de comunicación -> DO remotas OFF |
| `Tresponse` | **a medir** | latencia real evento -> acción |

Estos valores son **targets iniciales de diseño**, no especificaciones comerciales cerradas.

---

# 6. Significado de cada tiempo

## 6.1 `Tsample` — adquisición física

Indica cada cuánto el módulo mide realmente la señal física.

Objetivo para una futura expansión analógica:

```txt
Tsample = 1 ms
1000 SPS por canal
```

Ejemplo:

```txt
t=0 ms   5.01 V
t=1 ms   5.03 V
t=2 ms   5.04 V
t=3 ms   7.80 V  <- pico
t=4 ms   5.05 V
```

Aunque el Master no consulte el módulo hasta decenas de milisegundos después, el módulo debe poder detectar y conservar información relevante de ese pico.

**1000 SPS no significa 1000 actualizaciones/s por RS-485.**

---

## 6.2 `Tmodule` — procesamiento interno

Es el periodo de tareas locales propias del módulo, no de una lógica PLC distribuida.

Funciones previstas:

```txt
escalado
filtrado
promedio
mínimo
máximo
peak
alarmHigh
alarmLow
histeresis
latch de eventos
estado/diagnóstico local
```

Valor base:

```txt
Tmodule = 5 ms
```

La expansión debe ser inteligente respecto de su propia adquisición, pero la **autoridad de control de máquina sigue siendo el JWPLC principal**.

No se plantea ejecutar un Ladder independiente completo en cada módulo de expansión.

---

## 6.3 `Tremote` — actualización de I/O remoto

Es el periodo con el que el Backplane actualiza la imagen de proceso central a partir de los módulos remotos.

Valores base:

```txt
DI/DO: 20 ms
AI/AO: 50 ms
```

Los módulos no tienen por qué usar todos el mismo periodo.

Una implementación futura puede permitir prioridades o periodos por slot, dentro de límites que mantengan estable el bus.

---

## 6.4 `Tplc` — scan de la lógica PLC

Es el periodo objetivo con el que el runtime ejecuta Ladder/FBD/ST.

Valor base:

```txt
Tplc = 5 ms
```

Regla principal:

> El scan PLC no debe quedar bloqueado esperando el recorrido secuencial del RS-485.

El runtime trabaja sobre la última **Process Image** válida disponible.

---

## 6.5 `Tdiag` — diagnóstico

No todo dato del módulo necesita actualizarse con la misma prioridad que las I/O.

Ejemplos:

```txt
firmware version
modelo
UID
uptime
temperatura interna
CRC errors
timeouts
voltaje de alimentación
estado de calibración
```

Valor base:

```txt
Tdiag = 500 ms
```

Algunos datos podrán actualizarse incluso más lentamente si no afectan el control.

---

## 6.6 `Tsafety` — timeout de seguridad

Indica cuánto tiempo puede pasar sin comunicación válida antes de aplicar el estado seguro.

Valor base:

```txt
Tsafety DO = 100 ms
```

Política:

```txt
sin comunicación válida durante Tsafety
-> COMM_LOST = 1
-> FAILSAFE_ACTIVE = 1
-> DO remotas = OFF
```

Al recuperar comunicación, el Master debe volver a publicar explícitamente una imagen válida de salidas.

No se debe reactivar automáticamente una salida únicamente porque su último valor antes del fallo era `ON`.

---

## 6.7 `Tresponse` — respuesta real del sistema

Es la métrica final que realmente percibe el usuario.

Incluye, según el camino de señal:

```txt
adquisición física
+ espera de actualización remota
+ scan PLC
+ espera de publicación de salida
+ actuación física
```

Debe medirse experimentalmente.

No debe confundirse con `Tplc`, `Tsample` ni `Tremote`.

---

# 7. Process Image central

La pieza central de la arquitectura será una **imagen de proceso desacoplada del transporte RS-485**.

Conceptualmente:

```txt
Remote Modules
      |
      v
Remote I/O task
      |
      v
Process Image central
      ^
      |
PLC Runtime / Ladder / FBD / ST
```

El programa IEC no debe conversar directamente con cada Slave.

La tarea Remote I/O actualiza la Process Image y la lógica utiliza el último valor válido.

Ejemplo conceptual:

```txt
Local:
I0_0 ... I0_7
Q0_0 ... Q0_7

Slot 1:
I1_0 ... I1_7
Q1_0 ... Q1_7

Slot 2:
I2_0 ... I2_7

Slot AI:
AIx_0
AIx_1
AIx_2
AIx_3

Status:
Slot1_OK
Slot2_OK
SlotAI_OK
COMM_LOST
FAILSAFE_ACTIVE
```

OpenPLC deberá integrarse sobre esta abstracción y no reemplazar el flujo Arduino normal.

---

# 8. Arquitectura para módulos analógicos de 1000 SPS

## 8.1 Decisión principal

Se mantiene el objetivo:

```txt
hasta 1000 SPS por canal localmente
```

pero se separa explícitamente de la frecuencia con la que el Master recibe la Process Image.

Arquitectura:

```txt
AI física
   |
   v
ADC local 1000 SPS/ch
   |
   v
procesamiento local
   |
   +--> latest
   +--> filtered
   +--> min/max/peak
   +--> alarms/event latch
   +--> buffer opcional
   |
   v
Process Image del módulo
   |
   v
RS-485 a Tremote AI/AO
```

## 8.2 Buffer de muestras

Se considera deseable separar dos planos:

```txt
PROCESS IMAGE
- valor actual
- status
- alarmas
- min/max/peak

DATA BUFFER
- muestras raw
- captura de eventos
- históricos cortos
- diagnóstico
```

El buffer raw se descargaría bajo demanda y no formaría parte obligatoria de cada ciclo del Backplane.

## 8.3 Eventos rápidos

Si una condición dura menos que `Tremote`, el módulo puede conservarla mediante:

```txt
peak
min/max
alarm latch
event flag
timestamp o contador de eventos
```

Si una aplicación requiere una acción física garantizada del orden de 1 ms, no debe depender de una ruta completa:

```txt
AI remota -> RS-485 -> PLC -> RS-485 -> DO remota
```

Ese caso requiere procesamiento/acción local, hardware dedicado o una arquitectura de comunicación de mayor determinismo.

---

# 9. Comunicación cíclica y uso de funciones Modbus

## 9.1 Polling

Polling significa que el Master consulta/escribe periódicamente los módulos.

No implica que el usuario tenga que programar repetidamente comandos individuales de ON/OFF.

La comunicación periódica pertenece a la infraestructura del Backplane.

## 9.2 Operación digital propuesta

Para la operación normal del Remote I/O se prefiere conceptualmente una imagen cíclica:

```txt
FC02 -> leer imagen de entradas digitales
FC15 -> publicar imagen completa de salidas digitales
```

`FC05` se conserva para:

```txt
diagnóstico
pruebas manuales
compatibilidad Modbus
escritura individual cuando corresponda
```

`FC01` se conserva para:

```txt
readback
diagnóstico
validación de estado
```

No es obligatorio ejecutar FC01 en absolutamente cada ciclo si la arquitectura final no lo requiere.

La política exacta del scheduler debe verificarse mediante mediciones de carga y latencia.

---

# 10. Escalabilidad objetivo del RS-485

Se adopta como caso de estrés/escala objetivo inicial:

```txt
8 módulos digitales de 8 canales = 64 I/O digitales
8 módulos analógicos de 4 canales = 32 I/O analógicos
Total = 16 módulos / 96 I/O
```

Este caso sirve como referencia de dimensionamiento, no como límite final garantizado todavía.

Baudrate base acordado para continuar Alpha5:

```txt
115200 baud
```

El transceptor MAX13487E permite velocidades superiores, pero **no se aumentará el baudrate como sustituto de una arquitectura eficiente**.

Primero deben optimizarse y medirse:

```txt
scheduler
prioridades
Tremote digital
Tremote analógico
frame gaps
timeouts
retries
fail-safe
diagnóstico secundario
uso real de bus
```

Solo después se compararán velocidades superiores.

---

# 11. Principios de diseño que no deben romperse

## 11.1 No bloquear el scan PLC por RS-485

Incorrecto:

```txt
consultar Slave 1
esperar
consultar Slave 2
esperar
...
consultar Slave 16
esperar
recién ejecutar Ladder
```

Objetivo:

```txt
Task PLC            -> ejecuta a su propio periodo
Task Remote I/O     -> recorre y actualiza módulos
Process Image       -> desacopla ambas tareas
```

## 11.2 Sampling rate != I/O update rate

Debe conservarse esta regla:

```txt
1000 SPS AI
!= 1000 actualizaciones/s por RS-485
!= scan PLC de 1 ms
```

## 11.3 Módulo inteligente != PLC distribuido

Los módulos pueden ejecutar funciones de adquisición y diagnóstico local, pero la lógica de máquina no debe fragmentarse arbitrariamente entre expansiones.

## 11.4 Estado seguro explícito

Las salidas remotas no deben quedar indefinidamente en `HOLD_LAST_STATE` al perder la autoridad del Master.

Política objetivo actual:

```txt
SAFE_OFF_AFTER_COMM_TIMEOUT
```

---

# 12. Ejemplo hipotético de funcionamiento completo

Configuración:

```txt
Tplc        = 5 ms
DI Tremote  = 20 ms
DO Tremote  = 20 ms
AI Tsample  = 1 ms
AI Tmodule  = 5 ms
AI Tremote  = 50 ms
Tdiag       = 500 ms
Tsafety     = 100 ms
```

Supongamos:

```txt
EXP DI8:
I1_0 = sensor de pieza

EXP DO8:
Q2_0 = electroválvula

EXP AI4:
AI3_0 = presión
```

Secuencia digital:

```txt
t=7 ms   sensor I1_0 cambia a HIGH

t=20 ms  Remote I/O lee DI y actualiza Process Image

t=20..25 ms
         siguiente scan PLC ve I1_0=1
         lógica determina Q2_0=1

t=25..40 ms
         tarea Remote I/O publica imagen DO
         módulo acciona Q2_0
```

La latencia real será medida como `Tresponse`.

Durante el mismo intervalo, el módulo AI puede haber adquirido decenas de muestras, aunque el valor de proceso se publique cada 50 ms.

---

# 13. Siguiente trabajo inmediato de Alpha5

El siguiente cambio debe ser incremental y limitado al Slave de prueba.

## Paso 1 — Fail-safe

Implementar y validar:

```txt
Tsafety = 100 ms
SAFE_OFF_AFTER_COMM_TIMEOUT
```

Criterios mínimos:

```txt
- comunicación normal no dispara fail-safe;
- pérdida del Master apaga DO al superar Tsafety;
- el Slave no se bloquea;
- la recuperación de comunicación funciona;
- las salidas no se reactivan con un estado viejo;
- el Master debe volver a publicar una imagen válida.
```

## Paso 2 — tráfico cíclico instrumentado

Agregar una prueba de comunicación sostenida que registre:

```txt
duración
requests
responses OK
timeouts
CRC errors
Modbus exceptions
recoveries
```

Duración inicial objetivo:

```txt
>= 30 minutos
```

## Paso 3 — medir temporalmente

Registrar experimentalmente:

```txt
Tremote efectivo
jitter
uso del bus
Tresponse DI remoto -> DO remoto
recuperación ante desconexión
recuperación ante reset
```

## Paso 4 — OpenPLC

Solo después de cerrar Arduino ↔ Arduino:

```txt
B. OpenPLC Master -> Arduino Slave
C. Arduino Master -> OpenPLC Slave
D. OpenPLC Master -> OpenPLC Slave
E. Backplane -> Remote Devices -> OpenPLC
```

No modificar el Backplane para resolver un problema que todavía no haya sido aislado en las capas anteriores.

---

# 14. Estado de decisión

## Decisiones adoptadas

```txt
[DECIDIDO] 115200 como baudrate base de Alpha5
[DECIDIDO] Process Image desacoplada del transporte
[DECIDIDO] Tplc objetivo base = 5 ms
[DECIDIDO] Tremote DI/DO base = 20 ms
[DECIDIDO] Tremote AI/AO base = 50 ms
[DECIDIDO] Tsample AI objetivo = 1 ms / 1000 SPS por canal
[DECIDIDO] Tmodule base = 5 ms
[DECIDIDO] Tdiag base = 500 ms
[DECIDIDO] Tsafety DO base = 100 ms
[DECIDIDO] fail-safe objetivo = DO OFF
[DECIDIDO] adquisición rápida local separada del envío por RS-485
[DECIDIDO] eventos rápidos pueden conservarse mediante latch/min/max/peak/buffer
[DECIDIDO] la lógica PLC no debe bloquearse esperando RS-485
[DECIDIDO] 16 módulos / 96 I/O como caso de escala objetivo inicial
```

## Valores aún por caracterizar

```txt
[PENDIENTE] Tresponse real
[PENDIENTE] jitter real de Tremote
[PENDIENTE] carga real del bus con múltiples módulos
[PENDIENTE] número máximo de módulos garantizable
[PENDIENTE] scheduler definitivo
[PENDIENTE] política final de retries
[PENDIENTE] frame gap definitivo
[PENDIENTE] conveniencia de 250000 / 500000 baud
[PENDIENTE] mapa/API final de módulos analógicos
[PENDIENTE] formato del buffer raw de AI
[PENDIENTE] comportamiento exacto de recuperación de fail-safe
```

---

# 15. Regla para futuras transferencias

Cuando otra persona continúe este desarrollo, debe partir de este documento y del estado real del branch antes de proponer cambios.

No se debe volver a asumir que:

```txt
scan PLC = sampling ADC = polling RS-485
```

ni que aumentar el baudrate resuelve por sí solo la escalabilidad.

La arquitectura aprobada separa explícitamente adquisición, procesamiento local, comunicación remota, ejecución PLC, diagnóstico, seguridad y respuesta real del sistema.
