# Resultados físicos — adaptador combinacional v1 a v2

## Estado

```text
ADAPTADOR COMBINACIONAL V1 A V2: VALIDADO EN HARDWARE
RESULTADO: 49 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0: NO CONMUTADAS
```

## Compilación validada

```text
Flash:        423317 bytes / 3145728 bytes (13 %)
RAM global:    30268 bytes / 327680 bytes (9 %)
RAM restante: 297412 bytes
Puerto: COM4
```

## Compatibilidad cerrada en esta fase

El adaptador conservó exactamente el comportamiento del subconjunto v1:

```text
DigitalInput
NOT
AND de dos entradas
OR de dos entradas
```

Se confirmó:

- descriptor v1 de 12 bytes;
- descriptor v2 de 12 bytes;
- enlace v2 de 2 bytes;
- conversión de 14 bloques y 10 enlaces;
- conservación del orden de bloques;
- conservación de recursos de entradas;
- conversión de `sourceA/sourceB` a enlaces normales;
- ausencia de negaciones introducidas por el adaptador;
- comparación bloque por bloque entre referencia v1 y motor v2;
- igualdad bajo tres patrones de entradas;
- rechazo de programas nulos o vacíos;
- rechazo de buffers insuficientes;
- rechazo de flags todavía no soportados;
- rechazo de fuentes ausentes, propias o futuras.

## Decisión

```text
COMPATIBILIDAD COMBINACIONAL V1 -> V2: APROBADA
DIGITALOUTPUT: SIGUIENTE FASE
SET/RESET Y TON: PENDIENTES
CODEC V2: TODAVÍA NO
```

La RAM global permanece en 30268 bytes, igual que la prueba del motor v2. El adaptador no añade almacenamiento global permanente significativo.

## Siguiente fase

Añadir `DigitalOutput` al modelo v2 como bloque de una entrada y recurso de salida, manteniendo la prueba física sin conmutar relés:

- validación de recurso `Q0.x`;
- rechazo de salida duplicada;
- pass-through de la señal fuente;
- consulta de salida lógica desde el motor;
- adaptación v1 -> v2;
- comparación de resultados sin conectar todavía `JWPLCLogicIO`.