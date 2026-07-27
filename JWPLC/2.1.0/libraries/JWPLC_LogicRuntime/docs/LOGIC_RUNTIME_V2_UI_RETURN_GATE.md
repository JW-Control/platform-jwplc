# Puerta de retorno a JWPLC_LogicRuntime_UI

## Decisión

El desarrollo puede regresar a `JWPLC_LogicRuntime_UI` antes de completar codec v2, FRAM A/B y escritura física de salidas, pero no antes de cerrar el bloque `TON` y una regresión integrada del subconjunto histórico.

## Estado cerrado

```text
Entradas variables 2..8:       VALIDADO
Negación individual por pin:   VALIDADA
Motor RAM v2 100/512:          VALIDADO
Adaptador combinacional v1-v2: VALIDADO
DigitalOutput lógico:          VALIDADO
SET/RESET no retentivo:        VALIDADO
```

## Pendientes mínimos antes de volver al mapa FBD

### 1. TON v2

Debe incluir:

- una entrada de disparo;
- parámetro de tiempo en milisegundos;
- tiempo explícito entregado al scan, sin leer `millis()` dentro del núcleo;
- cálculo seguro ante rollover de `uint32_t`;
- cancelación inmediata cuando la entrada vuelve a `FALSE`;
- salida `TRUE` al alcanzar el tiempo configurado;
- limpieza de estado con `stop()` y `start()`;
- conversión v1 -> v2;
- comparación temporal contra el comportamiento v1.

### 2. Regresión integrada v1 -> v2

Un único programa debe combinar:

```text
DigitalInput
DigitalOutput
NOT
AND
OR
SET/RESET
TON
```

La prueba debe comparar bloque por bloque ambos motores durante una secuencia de scans y tiempos conocidos.

### 3. API de inspección para UI

La UI debe poder consultar en modo lectura:

- descriptor de bloque;
- enlaces variables;
- valor lógico actual;
- recurso físico;
- parámetro;
- estado temporal de TON;
- tiempo transcurrido o restante cuando corresponda.

## Trabajo que puede esperar durante el prototipo visual

```text
codec persistente v2
lectura/escritura FRAM v2
transacción A/B v2
escritura física de Q0 desde el motor v2
retentivos persistentes v2
editor gráfico con modificación del programa
commit desde UI
```

Estos puntos no bloquean el mapa FBD estable de solo lectura, pero sí bloquean declarar operativo el programador gráfico embarcado.

## Orden acordado

```text
1. TON v2
2. regresión integrada histórica
3. puente de lectura motor v2 -> UI
4. mapa FBD estable con puertos variables
5. navegación por cables
6. señales y temporización en vivo
7. codec/FRAM v2
8. edición gráfica transaccional
```

## Criterio de retorno

Con PASS físico de TON y de la regresión integrada, se pausa la expansión del backend y se retoma `JWPLC_LogicRuntime_UI` para construir el mapa FBD estable. La primera UI seguirá siendo de lectura y ejecución en RAM; no guardará cambios todavía.
