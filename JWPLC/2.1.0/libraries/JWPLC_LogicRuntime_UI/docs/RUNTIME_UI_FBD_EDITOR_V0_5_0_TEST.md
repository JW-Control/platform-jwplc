# Validación física — editor FBD v0.5.0

## Objetivo

Validar la primera edición gráfica real del programa v2 desde la TFT del JWPLC Basic.

Esta versión permite cambiar:

- la fuente de una entrada;
- la lógica normal o negada de esa entrada.

Los cambios se realizan en RAM. No se escribe FRAM.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

## Serial esperado

```text
Motor v2: RUNNING
UI FBD v0.5.0: LISTA
```

## Navegación

### Mapa

```text
LEFT / RIGHT   navegar entre fuentes y consumidores
UP / DOWN      navegar verticalmente
OK             abrir detalle del bloque
```

### Detalle

```text
UP / DOWN      seleccionar entrada
LEFT           saltar al bloque fuente
RIGHT          editar la entrada seleccionada
OK             volver al mapa
```

### Editor de entrada

```text
UP             seleccionar campo FUENTE
DOWN           seleccionar campo LÓGICA
LEFT / RIGHT   cambiar el valor del campo seleccionado
OK             validar y aplicar
ESC            cancelar y volver al detalle
```

Mientras el editor está abierto, `ESC` pertenece al editor y no debe regresar a IDLE.

## Prueba 1 — compuerta de liberación

1. Entrar a USER.
2. Abrir el detalle de `B04 AND`.
3. Mantener `RIGHT` durante la transición al editor.
4. Soltar el botón.

Resultado esperado:

```text
El editor se abre una sola vez.
La fuente no avanza automáticamente.
No quedan repeticiones aplicadas después de la transición.
```

Repetir al:

- cancelar con `ESC`;
- confirmar con `OK`;
- volver del detalle al mapa;
- saltar desde el detalle hacia un bloque fuente.

## Prueba 2 — edición de fuente en B04

1. Seleccionar `B04`.
2. Pulsar `OK`.
3. Seleccionar una de sus cuatro entradas con `UP/DOWN`.
4. Pulsar `RIGHT`.
5. Con el foco en `FUENTE`, recorrer candidatos con `LEFT/RIGHT`.

Para `B04` deben aparecer:

```text
X
HI
LO
B00
B01
B02
B03
```

No deben aparecer `B04` ni bloques posteriores.

La tarjeta izquierda, el cable y el bloque destino deben actualizarse gráficamente.

## Prueba 3 — filtro de entrada abierta

Abrir el editor de:

- `B07 TON`;
- `B08 NOT`;
- `B09 Q`.

Resultado esperado:

```text
X no aparece como candidato.
HI y LO sí aparecen.
Solo aparecen bloques anteriores al bloque editado.
```

La entrada abierta `X` solo es válida en:

```text
AND
OR
NAND
NOR
XOR
```

## Prueba 4 — negación

1. Abrir una entrada de `B04`.
2. Pulsar `DOWN` para seleccionar `LÓGICA`.
3. Usar `LEFT/RIGHT`.

Resultado esperado:

```text
NORMAL ↔ NEGADA
```

Al seleccionar `NEGADA`:

- aparece una burbuja gris en el pin;
- el color del cable representa el valor efectivo después de negar;
- la fuente conserva su valor bruto en su propia tarjeta.

## Prueba 5 — aplicar

1. Elegir una fuente distinta.
2. Elegir lógica normal o negada.
3. Pulsar `OK`.

Resultado esperado:

```text
APLICANDO CAMBIOS...
```

Después:

- se regresa al detalle;
- la conexión mostrada refleja el programa nuevo;
- el motor continúa en RUNNING;
- el mapa reconstruye niveles y cableado si la nueva fuente cambia la topología;
- el cambio afecta los valores lógicos durante el ciclo automático.

La recarga del motor ocurre desde el `loop`, mediante:

```cpp
JWPLC_LogicRuntime_UI.update();
JWPLC_LogicRuntime_UI.processV2EditorPending();
```

## Prueba 6 — cancelar

1. Abrir el editor.
2. Cambiar fuente y negación.
3. Pulsar `ESC`.

Resultado esperado:

```text
Se vuelve al detalle.
El programa activo no cambia.
USER permanece abierto.
Al soltar ESC no se salta a IDLE.
No quedan pulsos pendientes.
```

Después de volver al detalle, un nuevo `ESC` deliberado puede regresar a IDLE según la política normal del display.

## Prueba 7 — caso inválido

La navegación no debería ofrecer fuentes posteriores ni `X` en bloques que la rechazan. Como defensa adicional, cualquier borrador rechazado debe mostrar:

```text
CONFIGURACION NO VALIDA
```

El motor activo debe permanecer intacto.

## Registro de memoria

Anotar al compilar:

```text
Flash usada:
RAM global usada:
RAM disponible:
```

## Criterio de aprobación

```text
Compila en Arduino IDE.
Carga correctamente en JWPLC Basic.
No hay reinicios ni Fault del motor.
Mapa de cinco slots permanece estable.
No se arrastran pulsos entre pantallas.
ESC cancela el editor sin salir de USER.
La fuente aplicada cambia realmente el programa RAM.
La negación aplicada cambia realmente el valor efectivo.
No se escribe FRAM.
```
