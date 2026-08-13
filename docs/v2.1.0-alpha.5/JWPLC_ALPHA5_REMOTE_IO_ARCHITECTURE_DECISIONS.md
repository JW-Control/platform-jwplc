# JWPLC Alpha5 — Decisiones de arquitectura Remote I/O y modelo temporal

**Proyecto:** JWPLC Basic / platform-jwplc  
**Alpha:** v2.1.0-alpha.5  
**Rama de trabajo:** `v2.1.0-alpha.5/feature/openplc-backplane-validation`  
**Estado del documento:** decisión base adoptada para implementación y validación  
**Fecha:** 2026-08-13  
**Referencia de código previa a este documento:** `b290f26` (`test(remote-io): agregar barrido individual de salidas RTU`)

---

## 1. Objetivo del documento

Este documento conserva las decisiones de diseño tomadas durante la validación de Alpha5 para que cualquier integrante del equipo pueda continuar el trabajo sin reconstruir el razonamiento desde cero.

El objetivo es definir una arquitectura Remote I/O escalable para JWPLC Basic sobre RS-485 / Modbus RTU, separando correctamente:

- adquisición física de señales;
- procesamiento local dentro de cada módulo;
- actualización de la imagen de proceso remota;
- ciclo de ejecución de la lógica PLC;
- diagnóstico;
- supervisión/fail-safe de comunicaciones;
- latencia real evento → acción.

Estas decisiones son la **base adoptada para Alpha5**, pero los valores temporales deberán medirse físicamente antes de convertirse en especificaciones definitivas de producto.

---

## 2. Contrato RTU base de Alpha5

Para la PoC y las primeras validaciones se mantiene:

| Parámetro | Valor base |
|---|---:|
| Transporte | RS-485 |
| Protocolo | Modbus RTU |
| UART | Serial2 |
| Baudrate | **115200 bit/s** |
| Formato | **8N1** |
| Slave ID de la PoC | **2** |
| Entradas digitales | FC02 |
| Feedback de salidas | FC01 |
| Escritura individual | FC05 |
| Escritura de bloque DO | FC15 |

El baudrate de 115200 se mantiene como base porque ya está validado físicamente y ofrece un punto de partida estable. No se debe aumentar la velocidad únicamente para compensar una arquitectura de polling ineficiente.

---

## 3. Estado validado de la Fase A — Arduino Master ↔ Arduino Slave

Hasta la fecha se ha validado:

```text
ARDUINO_MASTER_SLAVE_PROTOCOL=PASS
FC01=PASS
FC02=PASS
FC05=PASS
FC15=PASS
CRC=PASS
SLAVE_ID_2=PASS
115200_8N1=PASS

PHYSICAL_DI_8CH=PASS
PHYSICAL_DO_8CH=PASS
DI_BIT_MAPPING=PASS
DO_BIT_MAPPING=PASS
BIT_SHIFT=NONE
LOGIC_INVERSION=NONE
```

### 3.1 Entradas físicas

Las ocho entradas del Slave se verificaron individualmente:

```text
I0_0 -> 0x01
I0_1 -> 0x02
I0_2 -> 0x04
I0_3 -> 0x08
I0_4 -> 0x10
I0_5 -> 0x20
I0_6 -> 0x40
I0_7 -> 0x80
```

### 3.2 Salidas físicas

Se añadió al Master el comando `w` de walking test. Las ocho salidas se activaron de una en una y el feedback FC01 coincidió con el estado esperado:

```text
Q0_0 -> 0x01
Q0_1 -> 0x02
Q0_2 -> 0x04
Q0_3 -> 0x08
Q0_4 -> 0x10
Q0_5 -> 0x20
Q0_6 -> 0x40
Q0_7 -> 0x80
```

El walking test quedó versionado en `b290f26`.

### 3.3 Recuperación de comunicación

También se validó:

```text
RS485_DISCONNECT_DETECTED=PASS
TIMEOUT_ON_DISCONNECT=PASS
MASTER_NO_CRASH=PASS
MASTER_NO_FREEZE=PASS
RECONNECT_WITHOUT_RESET=PASS
COMMUNICATION_RECOVERY=PASS
CRC_AFTER_RECOVERY=PASS

RECOVERY_AFTER_SLAVE_RESET=PASS
SLAVE_OUTPUTS_OFF_AFTER_RESET=PASS

RECOVERY_AFTER_MASTER_RESET=PASS
```

Al reiniciar el Slave con `Q0_0=ON`, el Slave arrancó nuevamente con `Q0_0..Q0_7=OFF`.

---

## 4. Política actual y política objetivo ante pérdida del Master

Se verificó físicamente que, si el Master desaparece mientras una salida está activa, el Slave actual **mantiene el último estado**.

```text
MASTER_LOSS_CURRENT_POLICY=HOLD_LAST_STATE
```

Esto describe el comportamiento actual de la PoC, pero **no es la política objetivo del producto**.

Se adopta como política de diseño:

```text
TARGET_FAILSAFE_POLICY=SAFE_OFF_AFTER_COMM_TIMEOUT
```

Es decir:

1. El Slave mantiene una referencia temporal de comunicación válida con el Master.
2. Si se supera `Tsafety` sin la comunicación esperada, se declara pérdida de enlace.
3. El módulo fuerza las salidas remotas a estado seguro.
4. Para las DO normales de esta arquitectura, el estado seguro base será **OFF**.
5. Cuando el enlace regrese, el Master deberá volver a publicar explícitamente una imagen válida de salidas; no se debe restaurar ciegamente un estado antiguo.

Valor base adoptado para Alpha5:

```text
Tsafety DO = 100 ms
```

Con un `Tremote DI/DO = 20 ms`, esto equivale aproximadamente a tolerar cinco ciclos digitales perdidos consecutivos antes de entrar en fail-safe.

---

## 5. Polling: qué significa y cómo se usará

En Modbus RTU el Master inicia las transacciones. El Slave no necesita recibir repetidamente un comando individual como `Q0_0=ON` únicamente para conservar el estado.

La arquitectura normal debe basarse en **comunicación cíclica de imagen de proceso**.

Conceptualmente:

```text
Master/JWPLC                    Remote I/O
     |                              |
     |---- leer imagen DI --------->|
     |<------- respuesta -----------|
     |                              |
     |---- publicar imagen DO ----->|
     |<------- confirmacion --------|
     |                              |
     `---------- repetir -----------'
```

Para operación normal se prefiere:

- FC02 para obtener la imagen digital de entradas;
- FC15 para publicar la imagen completa de salidas;
- FC01 para readback/diagnóstico cuando corresponda;
- FC05 se conserva como función estándar y útil para pruebas, diagnóstico y control individual, pero no tiene por qué ser el mecanismo normal del ciclo completo.

La comunicación cíclica también funciona como señal de vida del Master para el watchdog del Slave.

---

## 6. Principio fundamental: separar los tiempos del sistema

No existe un único “tiempo de ciclo” que deba gobernar ADC, RS-485, Ladder, diagnóstico y seguridad.

Se adopta formalmente el siguiente modelo:

```text
Tsample   -> cada cuanto se adquiere fisicamente una señal
Tmodule   -> cada cuanto se procesa localmente dentro del modulo
Tremote   -> cada cuanto se sincroniza la imagen de proceso remota
Tplc      -> cada cuanto se ejecuta la logica PLC
Tdiag     -> cada cuanto se actualiza diagnostico no critico
Tsafety   -> cuanto tiempo se tolera perder comunicacion
Tresponse -> latencia real entre evento fisico y accion fisica
```

### Valores base adoptados para Alpha5

| Parámetro | Valor base | Significado |
|---|---:|---|
| `Tsample AI` | **1 ms / 1000 SPS** | Adquisición física por canal en el módulo analógico |
| `Tmodule` | **5 ms** | Procesamiento local de muestras |
| `Tremote DI/DO` | **20 ms** | Objetivo de sincronización digital remota |
| `Tremote AI/AO` | **50 ms** | Objetivo de sincronización analógica remota |
| `Tplc` | **5 ms** | Objetivo de ejecución de lógica PLC |
| `Tdiag` | **500 ms** | Diagnóstico y datos no críticos |
| `Tsafety DO` | **100 ms** | Timeout de comunicación antes de SAFE OFF |
| `Tresponse` | **a medir** | Evento físico → acción física real |

Estos valores son objetivos iniciales de arquitectura. No deben publicarse todavía como tiempos garantizados del producto sin benchmark físico.

---

## 7. `Tsample` — adquisición física

`Tsample` indica cada cuánto el hardware mide realmente la señal.

Para un futuro módulo JWPLC EXP AI 4:

```text
Tsample = 1 ms
=> 1000 muestras/s por canal
=> 1000 SPS/ch
```

Ejemplo de una señal:

```text
t=0 ms   5.01 V
t=1 ms   5.03 V
t=2 ms   5.04 V
t=3 ms   7.80 V  <- pico corto
t=4 ms   5.05 V
```

Aunque el Master recién sincronice esa AI más tarde, el módulo local sí debe ser capaz de observar el pico.

Conclusión:

> La capacidad de adquisición del módulo no debe quedar limitada por el tiempo de actualización del bus.

---

## 8. `Tmodule` — procesamiento local del módulo

El módulo puede procesar sus propias muestras sin convertirse en un PLC independiente.

Funciones candidatas locales:

- escalado;
- filtrado digital;
- promedio;
- mínimo y máximo;
- detección de límites HIGH/LOW;
- histéresis;
- latch de alarmas/eventos;
- contador de eventos;
- diagnóstico;
- buffer circular opcional para muestras raw.

Ejemplo:

```text
Tsample = 1 ms
Tmodule = 5 ms
```

En una ventana de 5 ms el módulo puede procesar cinco muestras y publicar, por ejemplo:

```text
Latest
Average
Min
Max
AlarmHigh
AlarmLow
```

No se busca inicialmente distribuir la lógica Ladder/FBD/ST del usuario entre módulos. La autoridad de control sigue siendo el JWPLC principal.

---

## 9. `Tremote` — sincronización de I/O remoto

`Tremote` define cada cuánto se renueva la información de un módulo dentro de la imagen de proceso central del JWPLC.

Base adoptada:

```text
DI/DO = 20 ms
AI/AO = 50 ms
```

Los tiempos pueden ser diferentes porque las señales tienen dinámicas distintas y porque el bus es un recurso compartido.

Ejemplo AI:

```text
0..49 ms: modulo adquiere 50 muestras a 1000 SPS
50 ms: Master solicita/publica imagen analogica
```

El Master no necesita transportar las cincuenta muestras raw en cada sincronización. Puede recibir el valor de proceso y metadatos relevantes.

---

## 10. `Tplc` — ciclo de ejecución de lógica

`Tplc` es el objetivo de scan del runtime PLC.

Base:

```text
Tplc = 5 ms
```

La lógica PLC **no debe bloquearse esperando a que termine un barrido de todos los Slaves RS-485**.

La arquitectura deseada es:

```text
                    +------------------+
Remote I/O Task --->|  Process Image   |<--- PLC Logic Task
                    +------------------+
```

La tarea Remote I/O renueva la Process Image cuando obtiene datos nuevos.

La lógica Ladder/FBD/ST trabaja cada `Tplc` usando el último valor consistente disponible.

Por lo tanto es normal que varios scans de PLC utilicen el mismo valor remoto hasta que `Tremote` produzca una actualización.

---

## 11. Process Image — desacoplamiento central

La imagen de proceso es la pieza que desacopla el runtime de la comunicación.

Ejemplo conceptual:

```text
JWPLC Process Image
-------------------
Local:
I0_0 ... I0_7
Q0_0 ... Q0_7

Remote Slot 1:
I1_0 ... I1_7
Q1_0 ... Q1_7

Remote Slot 2:
I2_0 ... I2_7
Q2_0 ... Q2_7

Analog Slot:
AIx_0 ... AIx_3
AOx_0 ... AOx_3

Status:
Slot1_OK
Slot2_OK
COMM_LOST
FAILSAFE_ACTIVE
...
```

OpenPLC debe terminar trabajando sobre esta imagen de proceso, no enviando transacciones Modbus directamente desde cada bloque de lógica del usuario.

Objetivo conceptual de mapeo IEC:

```text
%IX... -> entradas locales/remotas
%QX... -> salidas locales/remotas
%IW... -> entradas analógicas / words
%QW... -> salidas analógicas / words
```

El programa IEC no debería necesitar conocer si una señal concreta proviene del hardware local o de un módulo Remote I/O.

---

## 12. `Tdiag` — tráfico de diagnóstico

No todos los datos del módulo necesitan el mismo periodo que las I/O de proceso.

Datos típicos de diagnóstico:

- versión de firmware;
- UID/serial;
- temperatura interna;
- alimentación;
- uptime;
- número de CRC errors;
- número de timeouts;
- número de reconexiones;
- calibración;
- flags de salud.

Base:

```text
Tdiag = 500 ms
```

Puede aumentarse a 1 s o más para datos que no requieran rapidez.

El objetivo es reservar el ancho de banda prioritario para señales de proceso.

---

## 13. `Tresponse` — métrica final que importa al usuario

`Tresponse` no es una tarea independiente; es el resultado total de la cadena.

```text
Tresponse =
  deteccion fisica
+ adquisicion
+ espera de comunicacion de entrada
+ espera/ejecucion del scan PLC
+ espera de comunicacion de salida
+ actuacion fisica
```

Por ejemplo, una DI remota que produce una DO remota puede tardar varias decenas de milisegundos aunque `Tplc=5 ms`.

Por este motivo el datasheet futuro debe diferenciar claramente:

- frecuencia de adquisición;
- frecuencia de actualización de I/O;
- scan de lógica;
- latencia evento → acción.

`Tresponse` debe medirse físicamente en diferentes topologías y cargas.

---

## 14. 1000 SPS no significa 1000 actualizaciones/s por RS-485

Esta distinción queda adoptada explícitamente:

```text
1000 SPS de AI
!= 1000 updates/s por RS-485
!= scan PLC de 1 ms
```

Un módulo analógico puede adquirir a 1000 SPS/ch y publicar al Master cada 50 ms una imagen de proceso compacta.

Para fenómenos rápidos, el módulo puede conservar localmente:

- pico;
- mínimo/máximo;
- promedio;
- alarma enclavada;
- timestamp/contador;
- buffer de muestras para descarga bajo demanda.

Si una aplicación requiere una **acción física garantizada en ~1 ms**, no se debe colocar un bus Modbus RTU de múltiples Slaves en el lazo crítico. Esa aplicación requerirá una estrategia específica: hardware dedicado, función local, interrupción/contador rápido, enlace más determinista u otra arquitectura apropiada.

---

## 15. Escalabilidad objetivo

Como caso de estrés de diseño se adopta una topología hipotética de hasta:

```text
8 modulos digitales x 8 canales = 64 I/O digitales
8 modulos analogicos x 4 canales = 32 I/O analogicos
------------------------------------------------------
16 modulos de expansion
96 puntos de I/O totales
```

No se pretende sincronizar absolutamente todas las funciones a la misma frecuencia.

El scheduler deberá priorizar tráfico por clases:

```text
Prioridad alta:
- DI/DO de proceso

Prioridad media:
- AI/AO de proceso

Prioridad baja:
- diagnostico
- identidad
- estadisticas
- descarga de buffers raw
```

El caso de 16 módulos deberá probarse físicamente antes de declarar capacidad final.

---

## 16. Ancho de banda y decisión sobre baudrate

El crecimiento del número de módulos no depende únicamente del número de bits de I/O. En Modbus RTU hay múltiples transacciones secuenciales, framing, silencios, tiempos de respuesta y posibles retries.

Por tanto:

> La primera optimización debe ser el scheduler y la organización del tráfico, no aumentar automáticamente el baudrate.

Para Alpha5 se mantiene:

```text
BAUDRATE_BASE=115200
```

Más adelante podrán compararse físicamente 115200 y velocidades superiores compatibles con el hardware, pero únicamente después de cerrar estabilidad y temporización a 115200.

---

## 17. Comparación conceptual con PLC comerciales

La referencia a PLC compactos como Siemens LOGO! se utiliza únicamente como orientación de arquitectura y escala de expansión.

No se debe asumir que el bus lateral propietario de LOGO! es Modbus RTU ni que sus tiempos internos son equivalentes a los de JWPLC.

La conclusión útil para JWPLC es arquitectónica:

- el scan de lógica no tiene por qué ser igual al tiempo de actualización de una AI;
- los módulos pueden realizar adquisición y procesamiento local;
- la CPU trabaja con una imagen de proceso;
- las funciones realmente rápidas pueden requerir hardware/mecanismos especializados;
- una frecuencia alta de adquisición no obliga a transportar todas las muestras crudas en cada ciclo del backplane.

---

## 18. Ejemplo temporal hipotético completo

Configuración:

```text
Tsample AI   = 1 ms
Tmodule      = 5 ms
Tremote DI   = 20 ms
Tremote DO   = 20 ms
Tremote AI   = 50 ms
Tremote AO   = 50 ms
Tplc         = 5 ms
Tdiag        = 500 ms
Tsafety DO   = 100 ms
```

Caso:

```text
t=7 ms
  Sensor remoto DI cambia a HIGH.

t=20 ms
  La tarea Remote I/O obtiene la nueva DI y actualiza Process Image.

t=20..25 ms
  El siguiente scan PLC ve la DI y cambia la Q correspondiente en Process Image.

t=25..40 ms aprox.
  La tarea Remote I/O publica la nueva imagen DO al modulo de salida.

Resultado:
  la latencia evento -> accion es de varias decenas de ms,
  aunque el scan PLC sea de 5 ms.
```

Mientras tanto un módulo AI puede haber tomado una muestra cada 1 ms durante todo ese intervalo.

---

## 19. Reglas de implementación derivadas

1. No bloquear el runtime PLC esperando respuestas de todos los Slaves.
2. Mantener una Process Image central coherente.
3. Ejecutar Remote I/O como tarea/scheduler desacoplado.
4. Separar tráfico de proceso y diagnóstico.
5. Mantener FC05/FC01 como funciones compatibles, aunque el ciclo normal pueda usar FC02 + FC15.
6. Implementar `Tsafety` en los módulos de salida.
7. El estado seguro base de DO será OFF.
8. Tras una pérdida de enlace, no restaurar automáticamente una imagen DO antigua.
9. Mantener adquisición AI rápida dentro del módulo aunque `Tremote` sea mayor.
10. No confundir `sampling rate`, `I/O update rate`, `PLC scan` y `Tresponse` en documentación comercial.
11. No declarar valores temporales definitivos antes de medirlos.
12. No modificar el Backplane/OpenPLC antes de cerrar las validaciones de protocolo requeridas por Alpha5.

---

## 20. Siguiente trabajo inmediato en Alpha5

Antes de avanzar a OpenPLC se debe continuar sobre el Arduino Slave conocido:

### 20.1 Implementar fail-safe

Objetivo inicial:

```text
Tsafety = 100 ms
SAFE_STATE = Q0_0..Q0_7 OFF
```

Validar al menos:

- comunicación normal no dispara fail-safe;
- pérdida del Master dispara fail-safe;
- pérdida física A/B dispara fail-safe;
- reset del Master dispara fail-safe mientras no haya tráfico;
- regreso del Master recupera la comunicación;
- salidas solo vuelven a activarse cuando el Master publica una imagen nueva válida.

### 20.2 Instrumentar tráfico cíclico

Añadir una prueba automática que permita medir:

```text
duracion
requests
responses OK
timeouts
CRC errors
Modbus exceptions
reconnections
failsafe activations
```

### 20.3 Soak test

Ejecutar al menos:

```text
>= 30 minutos
```

con tráfico cíclico y sin intervención manual.

Registrar resultado como evidencia reproducible.

### 20.4 Después de cerrar Fase A

Seguir el orden Alpha5:

```text
A. Arduino Master -> Arduino Slave
B. OpenPLC Master -> Arduino Slave
C. Arduino Master -> OpenPLC Slave
D. OpenPLC Master -> OpenPLC Slave
E. Backplane / Remote Devices / OpenPLC
```

No adelantar cambios de Backplane para ocultar fallas de protocolo.

---

## 21. Estado de decisiones

| Decisión | Estado |
|---|---|
| RS-485 / Modbus RTU como base de PoC | Adoptado |
| 115200 8N1 como baudrate base | Adoptado |
| Process Image desacoplada del scan PLC | Adoptado |
| `Tplc = 5 ms` objetivo inicial | Adoptado para validación |
| `Tremote DI/DO = 20 ms` | Adoptado para validación |
| `Tremote AI/AO = 50 ms` | Adoptado para validación |
| `Tsample AI = 1 ms / 1000 SPS` | Adoptado como objetivo de módulos AI |
| `Tmodule = 5 ms` | Adoptado como objetivo inicial |
| `Tdiag = 500 ms` | Adoptado como objetivo inicial |
| `Tsafety DO = 100 ms` | Adoptado para primera implementación |
| Fail-safe DO = OFF | Adoptado |
| HOLD_LAST_STATE actual | Confirmado en PoC, no deseado como política final |
| `Tresponse` | Pendiente de benchmark físico |
| Capacidad final 16 módulos | Pendiente de benchmark físico |
| Baudrate superior a 115200 | No necesario para Alpha5; evaluar después |

---

## 22. Resumen corto para transferencia

La arquitectura JWPLC Remote I/O **no debe hacer depender el ciclo PLC del tiempo necesario para recorrer el bus RS-485**.

La estrategia adoptada es:

```text
modulo adquiere rapido
        ↓
procesa localmente
        ↓
publica una Process Image compacta
        ↓
RS-485 la sincroniza de forma ciclica
        ↓
JWPLC mantiene Process Image central
        ↓
PLC ejecuta su logica a su propio Tplc
```

Valores base actuales:

```text
Tsample AI    1 ms / 1000 SPS
Tmodule       5 ms
Tremote DI/DO 20 ms
Tremote AI/AO 50 ms
Tplc          5 ms
Tdiag         500 ms
Tsafety DO    100 ms -> SAFE OFF
Tresponse     medir fisicamente
Baudrate      115200 8N1
```

La siguiente tarea es implementar el fail-safe de 100 ms en el Slave RTU y validarlo antes del soak test y antes de introducir OpenPLC.
