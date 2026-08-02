# Diseño aprobado propuesto — Mapa FBD estable v0.4

## Propósito

Reemplazar la navegación tipo páginas de `RuntimeUIDiagram v0.3.1` por un mapa lógico continuo, estable y navegable.

La representación corresponde a un diagrama de bloques de funciones (FBD) con lectura de izquierda a derecha. No se denomina Ladder porque el modelo actual del runtime está compuesto por bloques con fuentes A/B, recursos y una salida booleana.

## Principio central

```text
El bloque se mueve dentro del viewport solamente cuando cambia el mapa.
El cursor se mueve sobre el mapa.
El mapa no se recentra en cada pulsación.
```

La posición de un bloque debe mantenerse mientras el usuario navega. Esta persistencia espacial es obligatoria para comprender el flujo.

## Referencia funcional

Se toma de LOGO!:

- identificación de bloques `Bxx`;
- flujo de señal izquierda -> derecha;
- conexiones visibles;
- detalle separado del bloque;
- edición contextual mediante cursor;
- posibilidad de navegar el programa desde el propio equipo.

Se evita copiar sus limitaciones históricas:

- dependencia principal de números de bloque;
- visualización de un bloque aislado;
- edición excesivamente secuencial;
- poco aprovechamiento del color y del área disponible.

## Jerarquía de interfaz

```text
HOME
└── BLOQUES
    └── MAPA FBD             <- principal
        ├── menú contextual
        ├── detalle
        ├── lista técnica
        └── volver
```

## Distribución de pantalla

```text
┌────────────────────────────────────────┐
│ MAPA FBD       N1  1/2          RUN    │
├────────────────────────────────────────┤
│                                        │
│ [I0.0]──[NOT]────┐                     │
│                  ├──[OR]──[TON]──[Q0.0]│
│ [I0.1]──[AND]────┘                     │
│          ▲                             │
│ [I0.2]───┘                             │
│                                        │
├────────────────────────────────────────┤
│ B04 OR | A:B02 B:B03 | OK: acciones    │
└────────────────────────────────────────┘
```

El área del mapa ocupa casi toda la TFT. Los tres botones grandes de v0.3.1 desaparecen.

## Bloques compactos

Tamaño inicial sugerido:

```text
ancho:  44 a 58 px
alto:   24 a 34 px
```

Contenido mínimo:

```text
B04
OR
```

Casos de recurso:

```text
I0.0
Q0.0
TON
2.0s
S/R
RET
```

La información extensa se muestra en la barra contextual o en DETALLE, no dentro de cada nodo.

## Diferencia entre selección y estado lógico

No deben compartir el mismo lenguaje visual.

```text
Selección:
- borde verde de marca de 2 px;
- pequeñas esquinas o cursor;
- fondo negro/gris.

Señal TRUE:
- cable verde brillante;
- punto de salida verde;
- pequeño indicador de estado dentro del bloque.

Señal FALSE:
- cable gris;
- indicador apagado.
```

Un bloque seleccionado en FALSE no debe parecer activado.

## Posicionamiento

### v0.4 de solo lectura

Se utiliza layout determinista calculado en RAM:

1. entradas físicas en la primera columna;
2. profundidad lógica calculada por dependencias;
3. cada profundidad ocupa una columna;
4. ramas independientes reciben carriles verticales;
5. salidas físicas se colocan en la última columna;
6. cruces se reducen mediante orden por dependencias;
7. las coordenadas no cambian mientras el programa cargado sea el mismo.

No se escribe metadata de layout en FRAM en esta fase.

### Edición futura

Antes de permitir mover bloques manualmente se evaluará almacenar coordenadas compactas:

```text
columna: 1 byte
fila:    1 byte
```

Para 100 bloques el coste máximo sería 200 bytes antes de encabezados. Debe validarse contra el codec y el slot físico antes de adoptarse.

## Viewport

El mapa puede ser mayor que la TFT.

Estado del viewport:

```cpp
struct DiagramViewport
{
    int16_t offsetX;
    int16_t offsetY;
    uint16_t selectedBlock;
};
```

Reglas:

- el cursor se mueve entre nodos sin recentrar el mapa;
- el viewport solo se desplaza cuando el nodo seleccionado sale del margen visible;
- el desplazamiento conserva contexto lateral;
- al volver desde DETALLE se recupera exactamente la misma posición;
- al volver desde LISTA se recupera el mismo bloque y viewport.

## Navegación

La navegación deja de basarse en índice numérico.

```text
RIGHT:
- consumidor conectado más cercano;
- si hay varios, conserva el carril preferido.

LEFT:
- fuente A o B conectada más cercana.

UP / DOWN:
- otra rama o nodo visible por encima/debajo;
- cuando un bloque tiene dos fuentes/destinos, cambia entre ellas.

OK:
- abre menú contextual del bloque seleccionado.

ESC:
- vuelve a HOME o IDLE según el nivel.
```

El índice `Bxx` se conserva como identidad técnica, no como mecanismo principal de recorrido.

## Barra contextual

En reposo muestra:

```text
B04 OR | A:B02 B:B03 | OUT:1
```

Al pulsar OK abre un menú compacto:

```text
DETALLE
EDITAR
INSERTAR
CONECTAR
ELIMINAR
LISTA
VOLVER
```

En v0.4 solo estarán habilitados:

```text
DETALLE
LISTA
VOLVER
```

Los demás quedan documentados para fases posteriores.

## Redes

Un programa grande no debe mostrarse como un lienzo infinito sin estructura.

Se introduce el concepto visual de red:

```text
N1, N2, N3...
```

En v0.4 las redes se pueden derivar automáticamente por componentes conectados o por salidas finales. Antes de guardar redes explícitas en el formato persistente se requiere una decisión de codec.

Encabezado:

```text
MAPA FBD | N1 | 1/3 | RUN
```

## Renderizado

Se mantienen las reglas existentes:

- fondo y mapa estructural solo al cargar/cambiar programa;
- cables y bloques se cachean;
- navegación redibuja cursor anterior y nuevo;
- cambios lógicos actualizan cables e indicadores afectados;
- paneo redibuja el viewport completo, no toda la pantalla exterior;
- no se usa framebuffer completo;
- no se realizan operaciones FRAM dentro del callback TFT.

## Separación de archivos

```text
screens/RuntimeUIMap.h
screens/RuntimeUIMap.cpp
map/RuntimeUIMapLayout.h
map/RuntimeUIMapLayout.cpp
map/RuntimeUIMapRenderer.h
map/RuntimeUIMapRenderer.cpp
map/RuntimeUIMapNavigator.h
map/RuntimeUIMapNavigator.cpp
```

Responsabilidades:

```text
Layout:
- calcula columnas, carriles, redes y coordenadas.

Renderer:
- dibuja nodos, conexiones, estado y viewport.

Navigator:
- resuelve LEFT/RIGHT/UP/DOWN por geometría y conectividad.

Screen:
- coordina UI, caché, menú y transiciones.
```

## Plan de implementación

```text
v0.4.0  layout estable en RAM + mapa completo de una red pequeña
v0.4.1  viewport y paneo automático
v0.4.2  navegación por conectividad/geometría
v0.4.3  valores en vivo con dirty rendering
v0.4.4  detalle/lista conservando posición
v0.5    menú contextual y edición de parámetros
v0.6    inserción, conexión y eliminación
v0.7    persistencia transaccional del programa editado
```

## Criterios de aceptación de v0.4

```text
[ ] Se reconoce el programa como mapa sin recorrer todos los Bxx.
[ ] Los bloques conservan posiciones fijas.
[ ] LEFT/RIGHT sigue conexiones reales.
[ ] UP/DOWN cambia de rama de forma predecible.
[ ] Selección y TRUE son visualmente distintos.
[ ] La mayor parte de la TFT pertenece al mapa.
[ ] El viewport no se recentra en cada movimiento.
[ ] DETALLE vuelve al mismo bloque y posición.
[ ] LISTA vuelve al mismo bloque y posición.
[ ] Los cambios de entradas se siguen visualmente por los cables.
[ ] No hay parpadeo ni residuos.
[ ] Q0 y FRAM permanecen seguros en la demo.
```

## Estado de decisión

```text
v0.3.1: técnicamente validado, UX no aprobada como mapa.
v0.4: dirección seleccionada para cerrar la experiencia gráfica.
```