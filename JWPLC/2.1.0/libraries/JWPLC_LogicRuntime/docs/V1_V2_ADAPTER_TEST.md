# Prueba RAM — adaptador v1 a v2

## Objetivo

Comprobar que los programas históricos combinacionales v1 pueden convertirse al modelo de bloques y enlaces v2 sin cambiar sus resultados.

Subconjunto principal de regresión:

```text
DigitalInput
NOT
AND de dos entradas
OR de dos entradas
```

Desde la revisión siguiente también se verifica que `DigitalOutput` ya sea reconocido por el adaptador como bloque de una entrada. Su semántica completa se valida en un sketch separado.

Pendientes deliberados:

```text
SET/RESET
TON
retentivos
```

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

La conversión produce:

```text
14 bloques
10 enlaces
```

## Comparación de ejecución

Para cada patrón:

1. evalúa el programa v1 mediante una referencia local;
2. ejecuta el programa convertido en `LogicV2EnginePrototype`;
3. compara los 14 valores bloque por bloque;
4. comprueba la salida final B13.

Patrones:

```text
todas las entradas FALSE
rama final negada
todas las entradas TRUE
```

## Rechazos y compatibilidad

Se verifica:

```text
programa nulo
programa vacío
buffer de bloques insuficiente
buffer de enlaces insuficiente
SET/RESET todavía no soportado
TON todavía no soportado
flags no soportados
fuente ausente
fuente propia o futura
```

También se comprueba que un programa mínimo con `DigitalOutput` requiere exactamente un enlace y deja de clasificarse como tipo no soportado.

## Resultado esperado actual

```text
Resultado: 42 PASS, 0 FAIL
ADAPTADOR COMBINACIONAL V1 A V2: PASS
```

La versión anterior de esta prueba fue validada físicamente con `49 PASS, 0 FAIL` antes de incorporar `DigitalOutput`. Ese resultado histórico está registrado en:

```text
docs/V1_V2_COMBINATIONAL_ADAPTER_RESULTS.md
```

## Criterio de avance

La compatibilidad combinacional queda cerrada. La siguiente prueba independiente valida:

```text
DigitalOutput
recurso Q0.x
salida duplicada
pass-through
consulta lógica
```

Después se incorporarán `SET/RESET` y `TON` al motor v2.