# Prueba física — DigitalOutput v1 a v2 en RAM

## Objetivo

Añadir `DigitalOutput` al modelo RAM v2 y confirmar que los programas históricos conservan su semántica de salida sin conmutar todavía los relés físicos del JWPLC Basic.

## Alcance

```text
DigitalOutput v2:
- una entrada
- recurso Q0.x
- pass-through de la señal fuente
- consulta lógica desde el motor
- validación de recurso
- rechazo de salida duplicada
- negación opcional por pin v2
```

El adaptador v1 -> v2 ahora convierte:

```text
DigitalInput
DigitalOutput
NOT
AND
OR
```

`SET/RESET` y `TON` permanecen pendientes.

## Aislamiento

La prueba no conecta el motor experimental con `JWPLCLogicIO`:

```text
No llama JWPLC_LogicRuntime::begin()
No inicializa E/S desde el sketch
No escribe Q0 físicas
No abre ni escribe FRAM
No formatea almacenamiento
```

`DigitalOutput` se conserva como un bloque lógico consultable mediante:

```cpp
engine.digitalOutputValue(index);
```

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V1_V2_DigitalOutput_RAM
```

No requiere comandos por Serial.

## Programa de regresión

```text
B00  I0.0
B01  I0.1
B02  B00 AND B01
B03  NOT B00
B04  B02 OR B03
B05  Q0.0 <- B02
B06  Q0.1 <- B03
B07  Q0.2 <- B04
```

La adaptación debe producir:

```text
8 bloques
8 enlaces
3 DigitalOutput
```

## Patrones

```text
todo FALSE:
Q0.0 = FALSE
Q0.1 = TRUE
Q0.2 = TRUE

solo I0 TRUE:
Q0.0 = FALSE
Q0.1 = FALSE
Q0.2 = FALSE

I0 e I1 TRUE:
Q0.0 = TRUE
Q0.1 = FALSE
Q0.2 = TRUE
```

Se comparan todos los valores bloque por bloque contra un evaluador de referencia v1.

## Casos negativos

Se verifica:

- recurso de salida fuera del perfil;
- recurso `Q0.x` duplicado;
- entrada abierta en `DigitalOutput`;
- salida v1 sin fuente;
- salida v1 con fuente propia o futura.

También se comprueba que una conexión v2 negada en el pin de salida invierte únicamente esa conexión.

## Resultado esperado

```text
Resultado: 42 PASS, 0 FAIL
DIGITALOUTPUT V1 A V2 EN RAM: PASS
```

## Criterio de avance

Con PASS físico:

1. cerrar compatibilidad v1 -> v2 para `DigitalOutput`;
2. mantener todavía desacoplada la escritura física de Q0;
3. añadir estado por bloque al motor v2;
4. implementar `SET/RESET` con prioridad de RESET;
5. implementar `TON` usando tiempo explícito por scan;
6. ampliar el adaptador a todos los tipos v1.