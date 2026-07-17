# Prueba física — NUEVO BLOQUE FBD v0.5.6

## Objetivo

Validar la primera creación gráfica de bloques desde la TFT usando la sesión estructural RAM aprobada en v0.5.5.

La revisión incorpora:

- nodo virtual `+` después de la última columna lógica;
- selección de `NUEVO BLOQUE` desde el mapa;
- asistente de tipo y configuración;
- creación de `DI`, `NOT`, `AND2`, `TON` y `DO`;
- validación transaccional y aplicación diferida fuera del callback TFT;
- reconstrucción automática del layout;
- selección del bloque recién creado;
- cero escrituras FRAM y cero conmutaciones físicas de Q.

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
UI FBD v0.5.6: NODO + + ASISTENTE NUEVO BLOQUE
```

El ejemplo imprime también `bloques=` y `enlaces=` en cada cambio de fase para comprobar que el programa activo crece después de aplicar cada creación.

## Modelo del nodo virtual

El `+`:

- no pertenece al programa;
- no consume un índice de bloque;
- no consume enlaces;
- no participa en el scan;
- aparece como vista previa compacta cuando la última columna está visible;
- se convierte en bloque completo amarillo al seleccionarlo.

Cuando ya existen cinco o más niveles, la vista seleccionada desplaza la ventana lógica para mostrar el `+` como nueva columna a la derecha.

## Navegación

### MAPA FBD

```text
Seleccionar un bloque de la última columna
RIGHT       → seleccionar [+]
LEFT / ESC  → volver al bloque anterior
OK          → abrir NUEVO BLOQUE
```

`RIGHT` se intercepta antes del renderer base porque `pressed()` es un evento consumible.

### NUEVO BLOQUE — tipo

```text
UP / DOWN   → DI / NOT / AND2 / TON / DO
OK          → CONFIGURAR
ESC / LEFT  → volver a [+]
```

### CONFIGURAR

```text
LEFT / RIGHT → cambiar de campo
UP / DOWN    → modificar el campo seleccionado
OK           → crear y aplicar
ESC          → volver a selección de tipo
```

## Tipos iniciales

### DI

```text
RECURSO  I0.0 ... I0.7
```

Los recursos de entrada pueden repetirse porque varios bloques pueden leer la misma entrada física.

### NOT

```text
FUENTE  HI / LO / B00 ... Bnn
```

### AND2

```text
IN1  ABIERTO / HI / LO / B00 ... Bnn
IN2  ABIERTO / HI / LO / B00 ... Bnn
```

`IN2` inicia en `HI`, de modo que el AND nuevo siga inicialmente la lógica de `IN1`.

### TON

```text
FUENTE  HI / LO / B00 ... Bnn
VALOR   entero
UNIDAD  ms / s / min / h
```

El tiempo se almacena internamente en milisegundos.

### DO

```text
FUENTE   HI / LO / B00 ... Bnn
RECURSO  primera Q0.x libre
```

El asistente recorre únicamente salidas libres. En el ejemplo base `Q0.0` y `Q0.1` ya están ocupadas, por lo que la primera sugerencia debe ser `Q0.2`.

## Prueba 1 — vista previa y selección de `+`

1. Entrar al mapa FBD.
2. Navegar hasta un bloque de la última columna.
3. Verificar la vista previa compacta `+` a la derecha.
4. Pulsar `RIGHT`.

Resultado esperado:

- la pulsación se procesa una sola vez;
- el mapa se desplaza si hace falta;
- aparece un bloque completo `+` con doble borde amarillo;
- el encabezado indica `NUEVO BLOQUE`;
- ningún bloque real conserva selección amarilla;
- los estados lógicos continúan actualizándose sin redibujar todo el mapa.

## Prueba 2 — cancelar

1. Con `+` seleccionado, pulsar `LEFT`.
2. Seleccionarlo otra vez y pulsar `OK`.
3. En la lista de tipos pulsar `ESC`.
4. Entrar nuevamente, pasar a CONFIGURAR y pulsar `ESC`.

Resultado esperado:

- `LEFT/ESC` desde `+` restaura el mapa anterior;
- `ESC` desde tipo vuelve a `+`;
- `ESC` desde configuración vuelve a tipo;
- no cambian los conteos de bloques ni enlaces;
- no queda una sesión RAM sucia.

## Prueba 3 — crear NOT

1. Seleccionar `+` desde el último bloque actual.
2. Elegir `NOT`.
3. Conservar o elegir una fuente válida.
4. Pulsar `OK` para crear.

Resultado esperado:

- aparece `APLICANDO CAMBIOS...`;
- la aplicación se ejecuta desde `processV2EditorPending()`;
- el mapa reaparece con el nuevo bloque seleccionado;
- aumenta en uno el conteo de bloques;
- aumenta en uno el conteo de enlaces;
- el layout agrega o reutiliza la columna que corresponda a la fuente.

## Prueba 4 — crear AND2

1. Desde el nuevo bloque NOT, pulsar `RIGHT` para seleccionar `+`.
2. Elegir `AND 2`.
3. Seleccionar una fuente para `IN1`.
4. Conservar `IN2 = HI`.
5. Crear.

Resultado esperado:

- se agregan un bloque y dos enlaces;
- el AND sigue inicialmente el estado lógico de `IN1`;
- UP y DOWN recorren las fuentes en sentidos opuestos;
- `ABIERTO`, `HI` y `LO` aparecen en el orden esperado.

## Prueba 5 — crear TON

1. Elegir `TON`.
2. Configurar una fuente.
3. Configurar, por ejemplo, `3 s`.
4. Crear.

Resultado esperado:

- se agrega un bloque y un enlace;
- el parámetro interno equivale a `3000 ms`;
- el bloque puede abrir posteriormente el editor T ya validado;
- `Ta` comienza a actualizarse cuando la fuente está activa.

## Prueba 6 — crear DO

1. Elegir `SALIDA DO`.
2. Configurar como fuente el TON recién creado.
3. Verificar que el recurso sugerido sea `Q0.2`.
4. Crear.

Resultado esperado:

- se agrega un bloque y un enlace;
- el recurso no duplica `Q0.0` ni `Q0.1`;
- `digitalOutputValue(2)` refleja la fuente lógica;
- la salida física Q0.2 sigue sin conmutarse en este PoC.

## Prueba 7 — crear DI

1. Elegir `ENTRADA DI`.
2. Seleccionar `I0.4`.
3. Crear.

Resultado esperado:

- se agrega un bloque sin enlaces;
- el layout lo coloca en el nivel lógico 0 aunque su índice interno sea el último;
- el bloque queda seleccionado y muestra `I0.4`;
- para usarlo como fuente se debe crear después otro bloque, debido al orden topológico append-only actual.

## Restricción deliberada de v0.5.6

La creación es `appendBlock()`: el nuevo índice siempre queda al final del programa. Por ello:

- un nuevo bloque puede consumir cualquiera de los bloques existentes;
- los bloques creados después pueden consumirlo;
- un bloque existente con índice menor no puede conectarse a él;
- todavía no se inserta ni reordena el grafo topológico.

Esta restricción evita renumeraciones masivas durante la primera validación gráfica. La inserción/reordenamiento se evaluará después de cerrar creación y eliminación básica.

## Criterio de aprobación

```text
Compila y carga desde Arduino IDE.
Aparece el nodo + cuando la última columna está visible.
RIGHT selecciona + con una sola pulsación.
LEFT/ESC cancelan sin modificar el programa.
El asistente recorre DI, NOT, AND2, TON y DO.
UP y DOWN cambian fuentes/recursos en sentidos correctos.
Cada creación válida incrementa bloques y enlaces según corresponda.
Cada aplicación vuelve al mapa con el bloque nuevo seleccionado.
El layout se reconstruye correctamente.
No hay parpadeo general durante el runtime.
No se escribe FRAM.
No se conmutan salidas físicas.
```

## Siguiente incremento

Después de aprobar v0.5.6:

1. agregar foco `ACCIONES` en DETALLE;
2. implementar `ELIMINAR`;
3. bloquear eliminación si existen consumidores;
4. mostrar la cantidad/lista de consumidores;
5. compactar y reconstruir el mapa usando la base v0.5.5 ya aprobada.
