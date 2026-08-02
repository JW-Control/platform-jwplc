# Resultados físicos — SET/RESET v1 a v2 en RAM

## Estado

```text
SETRESET V1 A V2 EN RAM: VALIDADO EN HARDWARE
RESULTADO: 62 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0 FÍSICAS: NO CONMUTADAS
```

## Compilación validada

```text
Flash:        423413 bytes / 3145728 bytes (13 %)
RAM global:    30276 bytes / 327680 bytes (9 %)
RAM restante: 297404 bytes
Puerto: COM4
```

## Validaciones cerradas

Se confirmó físicamente:

- descriptor v2 de 12 bytes;
- enlaces v2 de 2 bytes;
- conversión v1 -> v2 de `SET/RESET`;
- dos entradas con roles `S` y `R`;
- prioridad de RESET;
- conservación del estado entre scans;
- igualdad bloque por bloque frente a una referencia v1;
- propagación hacia `DigitalOutput` lógico;
- limpieza con `stop()`;
- reinicio en `FALSE` después de `start()`;
- rechazo de una sola entrada;
- rechazo de entradas `S` o `R` abiertas;
- rechazo de fuentes ausentes, propias o futuras;
- rechazo temporal de la variante retentiva hasta implementar su compatibilidad completa.

## Consumo RAM

La prueba anterior de `DigitalOutput` utilizó 30268 bytes de RAM global. La prueba de `SET/RESET` utiliza 30276 bytes, una diferencia de 8 bytes atribuible al estado de referencia del sketch y su alineación. El motor v2 no añadió un arreglo permanente específico para `SET/RESET`; reutiliza el valor de bloque entre scans.

## Decisión

```text
COMPATIBILIDAD SET/RESET NO RETENTIVO V1 -> V2: APROBADA
PRIORIDAD DE RESET: APROBADA
MEMORIA ENTRE SCANS: APROBADA
RETENTIVO PERSISTENTE: TODAVÍA PENDIENTE
TON: SIGUIENTE BLOQUE OBLIGATORIO
REGRESIÓN INTEGRADA V1 -> V2: PENDIENTE
```
