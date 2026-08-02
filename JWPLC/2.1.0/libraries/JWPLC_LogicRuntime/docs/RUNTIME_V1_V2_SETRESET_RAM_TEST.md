# Prueba física — SET/RESET v1 a v2 en RAM

## Objetivo

Validar `SET/RESET` no retentivo dentro del motor RAM v2 y comprobar que conserva la semántica histórica del runtime v1.

## Semántica

```text
entrada 0 = S
entrada 1 = R
```

Reglas:

```text
R = TRUE                 -> salida FALSE
R = FALSE y S = TRUE     -> salida TRUE
R = FALSE y S = FALSE    -> conserva el estado anterior
S = TRUE y R = TRUE      -> salida FALSE, prioridad de RESET
```

El estado se conserva en el arreglo de valores del motor entre scans. No se añade todavía almacenamiento retentivo persistente.

## Alcance

```text
SET/RESET v2:
- exactamente dos entradas
- roles S y R
- prioridad de RESET
- estado durante RUN
- limpieza con stop()
- adaptación v1 -> v2
- encadenamiento con NOT y DigitalOutput
```

Pendientes:

```text
SET/RESET retentivo
exportación/importación de bitmap v2
FRAM
codec v2
TON
```

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V1_V2_SetReset_RAM
```

No requiere comandos por Serial.

## Seguridad

```text
No llama JWPLC_LogicRuntime::begin()
No inicializa E/S desde el sketch
No escribe Q0 físicas
No abre ni escribe FRAM
No formatea almacenamiento
```

`Q0.0` se consulta únicamente como salida lógica interna mediante `digitalOutputValue(0)`.

## Programa de regresión

```text
B00  I0.0 -> S
B01  I0.1 -> R
B02  SET/RESET(B00, B01)
B03  NOT B02
B04  Q0.0 <- B02
```

La adaptación debe producir:

```text
5 bloques
4 enlaces
B02 con inputCount = 2
B02 enlace 0 = S desde B00
B02 enlace 1 = R desde B01
```

## Secuencia evaluada

```text
S=0 R=0 -> FALSE inicial
S=1 R=0 -> TRUE
S=0 R=0 -> conserva TRUE
S=1 R=1 -> FALSE por prioridad de RESET
S=0 R=0 -> conserva FALSE
S=1 R=0 -> vuelve a TRUE
stop()   -> limpia FALSE
start()  -> inicia nuevamente en FALSE
```

Cada paso compara:

- evaluador de referencia v1;
- motor v2;
- todos los valores bloque por bloque;
- valor de B02;
- valor lógico de Q0.0.

## Casos negativos

Se verifica:

- una sola entrada;
- entrada S abierta;
- entrada R abierta;
- flag retentivo v2 todavía rechazado;
- adaptación de SET/RESET retentivo todavía rechazada;
- RESET sin fuente;
- RESET conectado al propio bloque o a un bloque futuro.

## Resultado esperado

```text
Resultado: 62 PASS, 0 FAIL
SETRESET V1 A V2 EN RAM: PASS
```

## Métricas requeridas

```text
Flash utilizada
RAM global utilizada
RAM restante
```

La RAM global debería permanecer en torno a 30268 bytes, porque SET/RESET reutiliza el arreglo `_values[]` ya reservado por el motor.

## Criterio de avance

Con PASS físico:

1. cerrar compatibilidad no retentiva de `SET/RESET`;
2. añadir almacenamiento temporal compacto para TON;
3. cambiar el scan v2 para recibir tiempo explícito;
4. adaptar TON v1 -> v2;
5. repetir regresión temporal;
6. añadir retentivos después de cerrar todos los tipos históricos.
