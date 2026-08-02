# Resultados físicos — DigitalOutput v1 a v2 en RAM

## Estado

```text
DIGITALOUTPUT V1 A V2 EN RAM: VALIDADO EN HARDWARE
RESULTADO: 53 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0 FÍSICAS: NO CONMUTADAS
```

## Compilación validada

```text
Flash:        423721 bytes / 3145728 bytes (13 %)
RAM global:    30268 bytes / 327680 bytes (9 %)
RAM restante: 297412 bytes
Puerto: COM4
```

## Validaciones cerradas

Se confirmó físicamente:

- descriptor v2 de 12 bytes;
- enlace v2 de 2 bytes;
- conversión v1 -> v2 de `DigitalOutput`;
- una entrada por salida;
- conservación del recurso `Q0.x`;
- pass-through de la señal fuente;
- consulta lógica mediante `digitalOutputValue()`;
- igualdad bloque por bloque frente a la referencia v1;
- recursos `Q0.0`, `Q0.1` y `Q0.2`;
- rechazo de recurso fuera del perfil;
- rechazo de salidas duplicadas;
- rechazo de entrada abierta;
- negación individual permitida en el pin de salida;
- rechazo de fuente ausente, propia o futura;
- `stop()` limpia los valores lógicos de salida.

## Decisión

```text
COMPATIBILIDAD DIGITALOUTPUT V1 -> V2: APROBADA
ESCRITURA FÍSICA DE Q0: TODAVÍA NO CONECTADA
RAM GLOBAL ADICIONAL: SIN CAMBIO MEDIBLE
SET/RESET: SIGUIENTE FASE
TON: PENDIENTE
```

La RAM global permanece en 30268 bytes, igual que las pruebas anteriores del motor v2. `DigitalOutput` reutiliza el arreglo de valores de bloque y no añade almacenamiento permanente.

## Siguiente fase

Añadir `SET/RESET` no retentivo al motor RAM v2:

- dos entradas con roles `S` y `R`;
- prioridad de RESET;
- conservación del estado entre scans;
- limpieza con `stop()` y `start()`;
- adaptación v1 -> v2;
- salida lógica asociada sin conmutar relés;
- retención persistente todavía fuera de alcance.
