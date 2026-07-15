# Resultado físico — regresión integrada v1 -> v2 en RAM

## Estado

```text
REGRESION INTEGRADA V1 A V2: VALIDADA EN HARDWARE
RESULTADO: 148 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S FÍSICA: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0 FÍSICAS: NO CONMUTADAS
```

## Compilación validada

```text
Flash:        425085 bytes / 3145728 bytes (13 %)
RAM global:    30788 bytes / 327680 bytes (9 %)
RAM restante: 296892 bytes
Puerto: COM4
```

## Programa cubierto

La prueba integró en un único programa:

```text
DigitalInput
DigitalOutput
NOT
AND
OR
SET/RESET
TON
```

La conversión produjo:

```text
14 bloques
16 enlaces
```

## Validaciones cerradas

Se confirmó físicamente:

- conversión v1 -> v2 de todos los tipos históricos soportados;
- igualdad de los 14 valores de bloque frente a la referencia v1;
- igualdad de cuatro salidas lógicas;
- propagación combinacional por NOT, AND y OR;
- memoria SET/RESET entre scans;
- prioridad de RESET;
- inicio, cuenta, activación, mantenimiento y cancelación de TON;
- tiempos de 999 ms y 1000 ms;
- segundo ciclo completo de SET/RESET + TON;
- limpieza de estados mediante `stop()`;
- reinicio mediante `start()`;
- cálculo correcto de TON al cruzar `UINT32_MAX`;
- rechazo del scan sin tiempo cuando el programa contiene TON;
- conservación del overload sin `nowMs` para programas combinacionales;
- descarga final segura del motor.

## Métricas

La prueba individual TON utilizó 30780 bytes de RAM global. La regresión integrada utilizó 30788 bytes, una diferencia de solo 8 bytes atribuible a los datos de referencia del sketch.

```text
Motor v2 con TON: 2852 bytes
RAM global final: 30788 bytes
RAM disponible:   296892 bytes
```

## Decisión

```text
BACKEND RAM V2 MÍNIMO PARA UI: CERRADO
COMPATIBILIDAD HISTÓRICA V1 -> V2: APROBADA
REGRESIÓN INTEGRADA: APROBADA
SIGUIENTE FASE: JWPLC_LogicRuntime_UI
```

La expansión del backend se pausa. El siguiente trabajo se concentra en:

1. puente de inspección de solo lectura;
2. mapa FBD estable;
3. puertos variables y negación por pin;
4. navegación por conexiones;
5. valores y temporización TON en vivo.

Siguen fuera del alcance de esta fase visual:

```text
codec persistente v2
FRAM A/B v2
escritura física de Q0 desde v2
retentivos persistentes v2
edición y guardado desde pantalla
```
