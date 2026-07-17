# Validación física — editor FBD v0.5.0

## Objetivo

Validar la primera edición gráfica real del programa v2 desde la TFT:

- seleccionar una entrada del bloque;
- cambiar su fuente;
- activar o quitar negación;
- validar el borrador RAM;
- recargar el motor;
- regresar al detalle con el diagrama actualizado.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

Serial esperado:

```text
Motor v2: RUNNING
UI FBD v0.5.0: LISTA
```

## Navegación

### Mapa

```text
LEFT/RIGHT/UP/DOWN → navegar bloques
OK                  → abrir DETALLE
```

### Detalle

```text
UP/DOWN → seleccionar entrada
LEFT    → saltar al bloque fuente
RIGHT   → editar la entrada seleccionada
OK      → volver al mapa
```

### Editor de entrada

```text
UP/DOWN → recorrer X, HI, LO y bloques anteriores
LEFT    → alternar NORMAL / NEGADA
RIGHT   → cancelar y volver al detalle
OK      → validar y aplicar
```

## Prueba A — quitar negación de B04 IN3

Estado inicial:

```text
B04 AND
IN3 = B02
Lógica = NEGADA
```

Procedimiento:

1. Seleccionar `B04`.
2. Pulsar `OK`.
3. Seleccionar `IN3/4` con `UP/DOWN`.
4. Pulsar `RIGHT`.
5. Verificar que aparece `B02 / I0.2` y `LOGICA: NEGADA`.
6. Pulsar `LEFT` una vez.
7. Verificar `LOGICA: NORMAL`.
8. Pulsar `OK`.
9. Confirmar retorno a DETALLE.
10. Confirmar que la burbuja de negación desapareció.

## Prueba B — cambiar B05 IN2 de LO a HI

Estado inicial:

```text
B05 OR
IN2 = LO
```

Procedimiento:

1. Seleccionar `B05`.
2. Abrir DETALLE.
3. Seleccionar `IN2/2`.
4. Pulsar `RIGHT`.
5. Usar `UP/DOWN` hasta seleccionar `HI / CONST 1`.
6. Mantener lógica NORMAL.
7. Pulsar `OK`.
8. Confirmar retorno a DETALLE.
9. Confirmar que la tarjeta de entrada muestra `HI / CONST 1`.
10. Confirmar que el estado lógico de B05 cambia acorde a la nueva fuente.

## Prueba C — cancelar

1. Abrir cualquier entrada en edición.
2. Cambiar temporalmente fuente y negación.
3. Pulsar `RIGHT`.
4. Confirmar retorno a DETALLE.
5. Confirmar que el programa activo no cambió.

## Prueba D — restricción acíclica

El selector solo debe ofrecer:

```text
X
HI
LO
B00 ... bloque inmediatamente anterior al bloque editado
```

No deben aparecer el propio bloque ni bloques posteriores.

## Prueba E — transición limpia

- Pulsar varias veces una dirección dentro del editor.
- Aplicar o cancelar.
- Confirmar que los pulsos pendientes no navegan automáticamente en DETALLE o MAPA.

## Criterio de aprobación

```text
Compila en Arduino IDE.
No reinicia el ESP32.
No aparece SCAN ERROR.
Aplicar modifica el diagrama y la lógica.
Cancelar no modifica el programa.
No se escribe FRAM.
No se conmutan salidas físicas Q0.
No quedan pulsos pendientes entre pantallas.
```
