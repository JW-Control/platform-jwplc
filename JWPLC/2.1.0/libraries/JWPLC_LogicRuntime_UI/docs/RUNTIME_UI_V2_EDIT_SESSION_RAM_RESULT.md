# Resultado físico — sesión de edición RAM v2

## Estado

```text
COMPILACIÓN ARDUINO IDE: aprobada
SUBIDA COM4: aprobada
EJECUCIÓN EN JWPLC BASIC: aprobada
PASS: 18
FAIL: 0
```

## Uso de memoria reportado

```text
Flash: 455973 bytes / 3145728 bytes (14 %)
RAM global: 36268 bytes / 327680 bytes (11 %)
RAM disponible para variables locales: 291412 bytes
```

## Resultado

Se validó físicamente que `RuntimeUIV2EditSession`:

- copia el programa activo a un borrador RAM;
- conserva el borrador inicialmente limpio;
- modifica la negación de una entrada;
- marca el borrador como modificado;
- valida el programa completo;
- aplica el borrador y reinicia el motor;
- cambia el resultado lógico después de aplicar;
- permite seleccionar una constante `HI` como fuente;
- mantiene el motor operativo después de una segunda aplicación.

Salida final:

```text
PASS=18 FAIL=0
```

## Decisión

El backend RAM queda aprobado para integrarse en el editor gráfico de la TFT.

No incluido todavía:

- FRAM;
- persistencia tras reinicio;
- cambio del número de entradas;
- edición de recursos I/Q;
- edición gráfica del parámetro TON;
- creación o eliminación de bloques.
