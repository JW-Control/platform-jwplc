# Prueba física — TON v1 a v2 en RAM

## Objetivo

Validar el temporizador a la conexión `TON` dentro del motor RAM v2 antes de regresar al desarrollo del mapa FBD en `JWPLC_LogicRuntime_UI`.

## Alcance

```text
TON v2:
- una entrada de disparo
- parámetro en milisegundos
- tiempo explícito por scan
- salida FALSE durante la cuenta
- salida TRUE al alcanzar el tiempo
- cancelación inmediata al caer la entrada
- cálculo seguro ante rollover de uint32_t
- consulta de timing, elapsed y remaining
- conversión desde el programa v1
```

## Decisiones

### Tiempo explícito

El motor no llama `millis()` internamente. El scan temporizado usa:

```cpp
engine.scan(inputs, inputCount, nowMs);
```

El overload sin `nowMs` sigue disponible para programas sin temporizadores. Cuando el programa contiene un TON, devuelve:

```text
TIME_REQUIRED
```

Esto evita que un TON quede congelado silenciosamente por recibir siempre el mismo tiempo.

### Estado RAM

El perfil Basic añade dos arreglos internos para 100 bloques:

```text
bool timing[100]          100 bytes
uint32_t startedAt[100]   400 bytes
```

El incremento teórico del motor es de 500 bytes respecto a la fase anterior. Debe confirmarse con las métricas físicas de compilación.

### Compatibilidad v1

El adaptador convierte:

```text
LogicBlockType::Ton
→ LogicV2BlockType::Ton
```

Conserva:

```text
sourceA    entrada de disparo
parameter  retardo en milisegundos
```

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V1_V2_Ton_RAM
```

No requiere comandos por Serial.

## Aislamiento

```text
No llama JWPLC_LogicRuntime::begin()
No escribe salidas físicas Q0
No abre ni escribe FRAM
No formatea almacenamiento
Las entradas son booleanos simulados en RAM
```

El autoload normal puede imprimir `JWPLC_Display inicializado`; esto no forma parte de la prueba del motor.

## Programa principal

```text
B00  I0.0 simulada
B01  TON B00, 1000 ms
B02  NOT B01
B03  Q0.0 lógica <- B01
```

La salida `Q0.0` solo se consulta dentro del motor mediante `digitalOutputValue(0)`.

## Secuencia temporal

```text
Entrada FALSE, t=100       TON=0, timing=0
Entrada TRUE,  t=1000      TON=0, timing=1, elapsed=0
Entrada TRUE,  t=1499      TON=0, timing=1, elapsed=499
Entrada TRUE,  t=1999      TON=0, timing=1, elapsed=999
Entrada TRUE,  t=2000      TON=1, timing=0, elapsed=1000
Entrada TRUE,  t=2500      TON=1
Entrada FALSE, t=2600      TON=0, estado temporal limpio
Entrada TRUE,  t=3000      segundo inicio
Entrada FALSE, t=3400      cancelación antes del tiempo
```

También se valida rollover:

```text
inicio       0xFFFFFF00
aún contando 0x000002E7  -> 999 ms
activado     0x000002E8  -> 1000 ms
```

La resta unsigned usada es:

```cpp
uint32_t elapsed = nowMs - startedAtMs;
```

## Casos adicionales

Se verifica:

- scan sin tiempo rechazado cuando existe TON;
- evaluador sin buffers temporales rechazado;
- cero o dos entradas rechazadas;
- entrada abierta rechazada;
- TON retentivo todavía rechazado;
- fuente ausente, propia o futura rechazada;
- TON de 0 ms activado en el mismo scan;
- disparo negado por pin;
- limpieza con `stop()` y reinicio con `start()`;
- consultas de tiempo sobre bloques no TON y fuera de rango.

## Resultado esperado

```text
Resultado: 144 PASS, 0 FAIL
TON V1 A V2 EN RAM: PASS
```

También se imprimirá:

```text
sizeof motor v2 con TON: ... bytes
almacen minimo con estados TON: 2824 bytes
```

## Criterio de avance

Con PASS físico:

1. registrar Flash y RAM;
2. cerrar TON v1 -> v2;
3. preparar una regresión integrada con todos los tipos históricos;
4. tras el PASS integrado, añadir el puente de lectura hacia la UI;
5. regresar al mapa FBD estable con puertos variables.
