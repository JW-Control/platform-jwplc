# Prueba física — adaptador combinacional v1 a v2

## Objetivo

Comprobar que los programas históricos del subconjunto combinacional v1 pueden convertirse al modelo de bloques y enlaces v2 sin cambiar sus resultados.

Subconjunto cubierto en esta fase:

```text
DigitalInput
NOT
AND de dos entradas
OR de dos entradas
```

Pendientes deliberados:

```text
DigitalOutput
SET/RESET
TON
retentivos
```

Estos tipos requieren primero su modelo equivalente dentro del motor v2.

## Componentes

```text
src/experimental/LogicV1ToV2Adapter.h
src/experimental/LogicV1ToV2Adapter.cpp
src/LogicV1ToV2Adapter.h
```

El adaptador:

- no reserva memoria dinámica;
- escribe sobre buffers entregados por la aplicación;
- conserva el número y orden de bloques;
- transforma `sourceA/sourceB` en enlaces v2;
- no introduce negaciones;
- conserva `resource` y `parameter`;
- limpia el descriptor destino cuando falla.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V1_V2_Adapter_RAM
```

No requiere comandos por Serial.

## Seguridad

```text
No llama JWPLC_LogicRuntime::begin()
No inicializa E/S desde el sketch
No contiene DigitalOutput ejecutable
No conmuta Q0
No abre ni escribe FRAM
No formatea almacenamiento
```

## Programa de regresión

```text
B00..B07  entradas digitales simuladas
B08        NOT B00
B09        B00 AND B01
B10        B08 OR B09
B11        B02 AND B03
B12        B10 OR B11
B13        NOT B12
```

La conversión debe producir:

```text
14 bloques
10 enlaces
```

## Comparación de ejecución

El sketch contiene un evaluador de referencia v1 limitado al mismo subconjunto. Para cada patrón:

1. evalúa el programa v1;
2. ejecuta el programa convertido en `LogicV2EnginePrototype`;
3. compara los 14 valores bloque por bloque;
4. comprueba la salida final B13.

Patrones:

```text
todas las entradas FALSE
rama final negada
Todas las entradas TRUE
```

## Rechazos verificados

```text
programa nulo
programa vacío
buffer de bloques insuficiente
buffer de enlaces insuficiente
DigitalOutput todavía no soportado
SET/RESET todavía no soportado
TON todavía no soportado
flags no soportados
fuente ausente
fuente propia o futura
```

## Resultado esperado

```text
Resultado: 49 PASS, 0 FAIL
ADAPTADOR COMBINACIONAL V1 A V2: PASS
```

## Criterio de avance

Con PASS físico:

1. cerrar compatibilidad combinacional v1 -> v2;
2. añadir `DigitalOutput` como bloque pass-through con recurso de salida;
3. añadir estado v2 para `SET/RESET` y `TON`;
4. ampliar el adaptador hasta cubrir todos los tipos históricos;
5. repetir regresión con tiempos y retentivos;
6. diseñar el codec persistente v2.