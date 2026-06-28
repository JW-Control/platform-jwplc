# Ejemplo OpenPLC: Blink Q0_0

Objetivo: validar que OpenPLC pueda escribir una salida industrial del JWPLC Basic.

## Variable sugerida

```txt
Q0_0_OUT  BOOL  %QX0.0
```

## Lógica Ladder sugerida

- Generar parpadeo con temporizador.
- Escribir el resultado sobre `Q0_0_OUT`.

## Resultado esperado

La salida física `Q0_0` conmuta periódicamente.

## Validación Modbus TCP

- Coil 0 debe cambiar de estado.
- Usar display tipo LED en ModbusTool para evitar confusión por empaquetamiento de bits.
