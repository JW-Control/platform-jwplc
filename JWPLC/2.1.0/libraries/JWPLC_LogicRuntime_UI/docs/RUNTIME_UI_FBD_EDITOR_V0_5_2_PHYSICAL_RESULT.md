# Resultado físico — editor FBD v0.5.2

## Estado

```text
NAVEGACIÓN ENTRADAS / PARÁMETROS: aprobada
SELECCIÓN DE Trg Y T: aprobada
EDITOR DE T: aprobado con correcciones
VISUALIZACIÓN T / Ta: aprobada con corrección
REPRESENTACIÓN GRÁFICA DE Q: aprobada
```

## Validación realizada

La prueba física en JWPLC Basic confirmó:

- `LEFT/RIGHT` mueve correctamente el foco entre la entrada `Trg` y el parámetro `T`;
- `UP/DOWN` selecciona el elemento dentro del grupo activo;
- `OK` sobre `Trg` abre `EDITAR IN`;
- `OK` sobre `T` abre `EDITAR T`;
- `VALOR` y `UNIDAD` pueden seleccionarse con `LEFT/RIGHT`;
- las unidades `ms`, `s`, `min` y `h` se muestran correctamente;
- `T ACTUAL` y `Ta LECTURA` se presentan de forma clara;
- la salida `Q` del TON no se duplica como texto y permanece representada por el bloque, terminal y cable activos.

## Correcciones detectadas

### 1. Pulso mantenido en VALOR

`UP/DOWN` solo consumía `pressed()`, por lo que un botón mantenido no aprovechaba los eventos `EV_REPEAT` ya generados por `JW_MatrixButtons`.

Decisión para v0.5.3:

```text
VALOR seleccionado
UP/DOWN corto      → +/- 1
UP/DOWN mantenido  → repeat acelerado mediante applyAxis()
```

Se conserva el perfil global de la botonera del JWPLC Basic y no se implementa un temporizador paralelo dentro de la UI.

### 2. Ta dentro de EDITAR T

`Ta LECTURA` se dibujaba al abrir la pantalla y al modificar un campo, pero no tenía refresco periódico propio.

Decisión para v0.5.3:

- actualizar únicamente el campo numérico de `Ta` cada 100 ms;
- no reconstruir el panel completo;
- mantener `T ACTUAL` como valor aplicado al motor;
- mantener `VALOR` y `UNIDAD` como borrador todavía no aplicado.

## Alcance conservado

```text
Edición transaccional en RAM.
Sin escritura FRAM.
Sin conmutación de salidas físicas.
Sin cambios al motor TON.
Sin cambios a la API pública de JW_MatrixButtons.
```
