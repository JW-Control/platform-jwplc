# Resultados físicos — TON v1 a v2 en RAM

## Estado

```text
TON V1 A V2 EN RAM: VALIDADO EN HARDWARE
RESULTADO: 144 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S FÍSICA: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0 FÍSICAS: NO CONMUTADAS
```

## Compilación validada

```text
Flash:        426017 bytes / 3145728 bytes (13 %)
RAM global:    30780 bytes / 327680 bytes (9 %)
RAM restante: 296900 bytes
Puerto: COM4
```

## Tamaño del motor

```text
sizeof LogicV2EnginePrototype: 2852 bytes
almacen mínimo calculado:      2824 bytes
overhead:                        28 bytes
```

La fase previa de `SET/RESET` utilizaba 30276 bytes de RAM global. La prueba TON utiliza 30780 bytes, un aumento físico de 504 bytes. El incremento coincide prácticamente con los 500 bytes añadidos al motor:

```text
bool timing[100]          100 bytes
uint32_t startedAt[100]   400 bytes
```

Los 4 bytes adicionales corresponden a alineación o estado auxiliar del sketch.

## Validaciones cerradas

Se confirmó físicamente:

- descriptor v2 de 12 bytes;
- enlaces v2 de 2 bytes;
- conversión `LogicBlockType::Ton` a `LogicV2BlockType::Ton`;
- conservación de `sourceA` y `parameter`;
- tiempo explícito mediante `scan(inputs, count, nowMs)`;
- rechazo del scan sin tiempo mediante `TIME_REQUIRED`;
- inicio de cuenta con flanco lógico de entrada;
- salida `FALSE` mientras no se alcanza el retardo;
- salida `TRUE` exactamente al alcanzar 1000 ms;
- mantenimiento de la salida mientras la entrada permanece activa;
- cancelación y limpieza inmediata al caer la entrada;
- segundo inicio después de una cancelación;
- limpieza mediante `stop()` y reinicio mediante `start()`;
- cálculo correcto durante rollover de `uint32_t`;
- consulta de `timing`, tiempo transcurrido y tiempo restante;
- TON de 0 ms activo en el mismo scan;
- negación individual del pin de disparo;
- rechazo de cero o dos entradas;
- rechazo de entrada abierta;
- rechazo temporal del TON retentivo;
- rechazo de fuentes ausentes, propias o futuras;
- igualdad bloque por bloque frente a la semántica histórica v1.

## Decisión

```text
TON V2 EN RAM: APROBADO
TIEMPO EXPLÍCITO POR SCAN: APROBADO
ROLLOVER UINT32_T: APROBADO
API DE INSPECCIÓN TEMPORAL: APROBADA
TON RETENTIVO: TODAVÍA PENDIENTE
REGRESIÓN INTEGRADA V1 -> V2: SIGUIENTE Y ÚLTIMA PRUEBA DE BACKEND
```

Con la regresión integrada aprobada se pausa la expansión del backend y se retoma `JWPLC_LogicRuntime_UI` para el mapa FBD estable de solo lectura.