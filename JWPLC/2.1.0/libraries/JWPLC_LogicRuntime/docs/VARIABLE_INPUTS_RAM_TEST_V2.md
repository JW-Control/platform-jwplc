# Prueba RAM — entradas variables y negación por pin v2

## Objetivo

Validar el modelo propuesto para funciones básicas compatibles con el uso habitual de LOGO! antes de modificar:

- `LogicBlockDefinition` v1;
- `LogicEngine` estable;
- codec persistente;
- slots A/B;
- retentivos;
- mapa FBD.

## Alcance implementado

La prueba usa una API experimental aislada:

```text
src/experimental/LogicVariableInputPrototype.h
src/experimental/LogicVariableInputPrototype.cpp
```

El modelo incluye:

```text
bloque v2 de 12 bytes
enlace de entrada de 2 bytes
2 a 8 entradas por compuerta
negación individual por enlace
entrada abierta X
constantes HI y LO
AND / OR / NAND / NOR / XOR / NOT
```

El runtime v1, el codec v1 y la FRAM no se modifican.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_Variable_Inputs_RAM
```

No requiere comandos por Serial.

## Seguridad

```text
No llama runtime.begin().
No inicializa E/S desde el sketch.
No contiene bloques DigitalOutput.
No conmuta Q0.
No abre, formatea ni escribe la FRAM.
No cambia el mapa persistente.
```

El mensaje posterior `JWPLC_Display inicializado` puede aparecer por el autoload normal del package.

## Programa de prueba

```text
B00..B07  entradas lógicas de prueba I0..I7
B08        AND de cuatro entradas
B09        AND con B00, !B01, X y HI
B10        OR con B00, B01, X y LO
B11        NAND de cuatro entradas
B12        NOR de cuatro entradas
B13        XOR de cuatro entradas
B14        NOT B00
B15        AND de ocho entradas
```

Las entradas son arreglos booleanos en RAM; no se leen bornes físicos.

## Casos evaluados

### Tamaños y capacidad

```text
LogicBlockDefinition v1 = 12 bytes
LogicV2BlockRecord      = 12 bytes
LogicV2InputLink        = 2 bytes
```

Presupuestos:

```text
100 bloques + 512 enlaces  = 2288 bytes <= 2528
400 bloques + 2048 enlaces = 8960 bytes <= 12256
```

### Lógica

Se prueban tres patrones:

```text
todas las entradas FALSE
patrón mixto
las ocho entradas TRUE
```

Se comprueba:

- AND de cuatro entradas;
- AND de ocho entradas;
- OR;
- NAND;
- NOR;
- XOR por paridad;
- NOT;
- negación independiente de B01;
- `X` neutro para AND y OR;
- constantes HI y LO.

### Validación negativa

Se rechaza:

- AND con una entrada;
- AND con nueve entradas bajo la política inicial;
- fuente que apunta al bloque actual o futuro;
- rango de enlaces fuera de la tabla;
- entrada digital fuera del perfil;
- `NOT` con entrada abierta;
- tipo desconocido;
- flags desconocidos;
- exceso de bloques;
- exceso de enlaces;
- programa vacío;
- tabla de enlaces nula;
- arreglo de entradas nulo;
- buffer de resultados insuficiente.

## Resultado esperado

```text
Resultado: 58 PASS, 0 FAIL
ENTRADAS VARIABLES V2 EN RAM: PASS
```

## Métricas a registrar

```text
Flash utilizada
RAM global utilizada
RAM restante
```

## Criterio de avance

Con PASS físico:

1. cerrar semántica de GF;
2. medir el coste RAM del arreglo de 512 enlaces;
3. integrar el modelo v2 en un motor RAM separado;
4. añadir compatibilidad de carga v1;
5. diseñar codec v2;
6. retomar el mapa FBD estable con puertos variables.
