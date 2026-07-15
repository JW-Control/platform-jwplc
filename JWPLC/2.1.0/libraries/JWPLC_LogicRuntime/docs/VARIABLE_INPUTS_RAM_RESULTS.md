# Resultados físicos — entradas variables v2 en RAM

## Estado

```text
PROTOTIPO RAM V2: VALIDADO EN HARDWARE
RESULTADO: 58 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0: NO CONMUTADAS
```

## Compilación validada

```text
Flash:       422269 bytes / 3145728 bytes (13 %)
RAM global:   27916 bytes / 327680 bytes (8 %)
RAM restante: 299764 bytes
Puerto: COM4
```

## Resultado serial

```text
Resultado: 58 PASS, 0 FAIL
ENTRADAS VARIABLES V2 EN RAM: PASS
```

## Validaciones cerradas

Se confirmó físicamente:

- `LogicBlockDefinition` v1 conserva 12 bytes;
- `LogicV2BlockRecord` conserva 12 bytes;
- cada enlace v2 ocupa 2 bytes;
- política inicial de 2 a 8 entradas por compuerta;
- AND de cuatro entradas;
- AND de ocho entradas;
- OR, NAND, NOR, XOR y NOT;
- negación independiente por enlace;
- `X` como elemento neutro de AND y OR;
- constantes `HI` y `LO`;
- rechazo de fuentes futuras;
- rechazo de rangos de enlaces inválidos;
- rechazo de recursos fuera de perfil;
- rechazo de tipos y flags desconocidos;
- límites de 100 bloques y 512 enlaces;
- imagen Basic de 2288 bytes dentro del payload de 2528 bytes;
- imagen de 400 bloques y 2048 enlaces dentro del payload de 12256 bytes.

## Decisión

El modelo de entradas variables y negación por pin queda aprobado para avanzar a un motor RAM v2 con copia profunda y capacidades compiladas reales.

Todavía no se modifica:

```text
LogicEngine v1
codec v1
program store A/B
retentivos
mapa de FRAM
interfaz FBD estable
```

## Siguiente fase

Validar `LogicV2EnginePrototype` con:

- capacidad compilada de 100 bloques y 512 enlaces;
- copia profunda de bloques y enlaces;
- carga, `start()`, `scan()` y `stop()` explícitos;
- ejecución sin repetir el validador en cada scan;
- programa máximo válido;
- medición física del incremento de RAM global.