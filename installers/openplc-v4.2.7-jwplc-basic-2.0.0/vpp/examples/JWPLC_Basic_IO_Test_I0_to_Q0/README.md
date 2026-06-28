# Ejemplo OpenPLC: I0_0 controla Q0_0

Objetivo: validar lectura de entrada industrial y escritura de salida industrial desde OpenPLC.

## Variables sugeridas

```txt
I0_0_IN   BOOL  %IX0.0
Q0_0_OUT  BOOL  %QX0.0
```

## Lógica Ladder sugerida

```txt
I0_0_IN  ----[ ]----------------( )---- Q0_0_OUT
```

## Resultado esperado

Al activar la entrada física `I0_0`, OpenPLC actualiza `%IX0.0` y energiza `Q0_0` mediante `%QX0.0`.

## Mapa completo recomendado para prueba final

Entradas:

```txt
I0_0_IN  BOOL  %IX0.0
I0_1_IN  BOOL  %IX0.1
I0_2_IN  BOOL  %IX0.2
I0_3_IN  BOOL  %IX0.3
I0_4_IN  BOOL  %IX0.4
I0_5_IN  BOOL  %IX0.5
I0_6_IN  BOOL  %IX0.6
I0_7_IN  BOOL  %IX0.7
```

Salidas:

```txt
Q0_0_OUT  BOOL  %QX0.0
Q0_1_OUT  BOOL  %QX0.1
Q0_2_OUT  BOOL  %QX0.2
Q0_3_OUT  BOOL  %QX0.3
Q0_4_OUT  BOOL  %QX0.4
Q0_5_OUT  BOOL  %QX0.5
Q0_6_OUT  BOOL  %QX0.6
Q0_7_OUT  BOOL  %QX0.7
```
