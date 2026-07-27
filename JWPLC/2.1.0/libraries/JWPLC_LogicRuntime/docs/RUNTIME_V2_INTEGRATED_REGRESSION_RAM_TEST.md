# Regresión integrada v1 -> v2 en RAM

## Objetivo

Cerrar la última prueba obligatoria de backend antes de regresar a `JWPLC_LogicRuntime_UI`.

Un único programa combina todos los tipos históricos soportados:

```text
DigitalInput
DigitalOutput
NOT
AND
OR
SET/RESET
TON
```

El programa v1 se convierte mediante `LogicV1ToV2Adapter` y se ejecuta en `LogicV2EnginePrototype`. En cada scan se compara contra un evaluador de referencia que reproduce la semántica histórica v1.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V1_V2_Integrated_Regression_RAM
```

No requiere comandos por Serial.

## Programa

```text
B00  I0.0 simulada
B01  I0.1 simulada
B02  I0.2 simulada
B03  NOT B02
B04  B00 AND B03
B05  B04 OR B01
B06  SET/RESET: S=B04, R=B01
B07  TON B06, 1000 ms
B08  B07 AND B02
B09  B08 OR B03
B10  Q0.0 lógica <- B06
B11  Q0.1 lógica <- B07
B12  Q0.2 lógica <- B09
B13  Q0.3 lógica <- B05
```

La conversión debe producir:

```text
14 bloques
16 enlaces
```

## Secuencia principal

La prueba valida:

1. estado inicial;
2. SET que activa la memoria e inicia TON;
3. liberación de SET conservando SET/RESET;
4. TON a 999 ms;
5. TON a 1000 ms;
6. mantenimiento de salida;
7. RESET de SET/RESET y cancelación de TON;
8. liberación de RESET;
9. prioridad de RESET con SET y RESET simultáneos;
10. nuevo SET después de liberar RESET;
11. segunda cuenta a 999 ms;
12. segunda activación a 1000 ms;
13. mantenimiento con otra rama lógica activa;
14. RESET final.

Cada paso comprueba:

```text
referencia v1
scan v2
los 14 valores de bloque
las cuatro salidas lógicas
máscara de salida esperada
SET/RESET, TON y timing
elapsed y remaining
```

## Rollover

Después de `stop()` y `start()` se repite una activación cruzando `UINT32_MAX`:

```text
inicio       0xFFFFFF00
999 ms       0x000002E7
1000 ms      0x000002E8
```

## Regresión del scan sin tiempo

Al final se carga un programa combinacional sin TON y se verifica que el overload histórico:

```cpp
engine.scan(inputs, inputCount);
```

continúe funcionando y no devuelva `TIME_REQUIRED`.

## Aislamiento

```text
No inicializa entradas físicas
No conmuta Q0 físicas
No abre ni escribe FRAM
No modifica el codec v1
No modifica el almacenamiento A/B
```

Las entradas y salidas son valores lógicos simulados en RAM.

## Resultado esperado

```text
Resultado: 148 PASS, 0 FAIL
REGRESION INTEGRADA V1 A V2: PASS
```

## Criterio de cierre

Con PASS físico:

```text
BACKEND RAM V2 MÍNIMO PARA UI: CERRADO
```

A continuación:

1. registrar métricas físicas;
2. añadir un puente de inspección de solo lectura entre motor v2 y UI;
3. regresar a `JWPLC_LogicRuntime_UI`;
4. sustituir el visor centrado por el mapa FBD estable;
5. dibujar puertos variables y negaciones;
6. navegar por conexiones;
7. mostrar TON, elapsed y remaining en vivo.
