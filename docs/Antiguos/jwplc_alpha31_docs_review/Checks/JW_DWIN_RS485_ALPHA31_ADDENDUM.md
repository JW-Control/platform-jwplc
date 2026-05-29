# JW_DWIN_RS485 - Addendum alpha31

Archivo destino sugerido:

```txt
README.md
```

Repositorio destino sugerido:

```txt
JW-Control/JW_DWIN_RS485
```

## Nota recomendada para evitar confusión

Agregar después de la descripción inicial.

```md
> Nota JWPLC: esta librería no forma parte del runtime base de `JWPLC Basic v2.0.0`. No reemplaza a `JWPLC_RS485` ni a `JWPLC_ModbusRTU`; es una librería adicional para pantallas DWIN por RS-485/Modbus RTU.
```

## Diferencia con librerías internas JWPLC

Agregar si se desea aclarar el alcance:

```md
En el package JWPLC:

- `JWPLC_RS485` maneja el puerto físico RS-485 del JWPLC Basic.
- `JWPLC_ModbusRTU` implementa una base Modbus RTU propia sobre ese puerto.
- `JW_DWIN_RS485` está orientada a pantallas DWIN y usa `ModbusMaster`.

Por ello, `JW_DWIN_RS485` debe tratarse como librería complementaria, no como parte obligatoria del autoload normal del JWPLC Basic.
```
