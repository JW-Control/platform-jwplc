# Runtime UI — Diagrama gráfico v0.3.1

## Objetivo

Convertir `DIAGRAMA` en la vista principal de `BLOQUES`, aprovechando la TFT a color para representar el flujo lógico de manera más intuitiva que una tabla de referencias.

La tabla existente se conserva como inspector técnico secundario.

## Jerarquía de navegación

```text
HOME
└── BLOQUES
    └── DIAGRAMA          <- vista principal
        ├── DETALLE       <- parámetros del bloque seleccionado
        ├── LISTA         <- inspector técnico
        └── VOLVER        <- HOME
```

Desde `LISTA`, el botón `VOLVER` regresa a `DIAGRAMA`.

## Representación gráfica v0.3.1

La primera versión no intenta mostrar 100 bloques diminutos simultáneamente. Usa una ventana de contexto centrada en un bloque:

```text
fuente A/B -> bloque seleccionado -> consumidores inmediatos
```

Distribución:

```text
┌────────────┐      ┌────────────┐      ┌────────────┐
│ fuente A   │ ─A─> │            │ ───> │ destino 1  │
└────────────┘      │ bloque Bxx │      └────────────┘
                    │ tipo/dato  │
┌────────────┐      │ salida 0/1 │      ┌────────────┐
│ fuente B   │ ─B─> │            │ ───> │ destino 2  │
└────────────┘      └────────────┘      └────────────┘
```

Para `SET/RESET`, los puertos se etiquetan como:

```text
S = SET
R = RESET
```

Para entradas y salidas físicas:

```text
I0.x -> bloque ENTRADA
bloque SALIDA -> Q0.x
```

Cuando un bloque no tiene consumidores:

```text
bloque -> FIN / SIN DESTINO
```

Se muestran hasta dos consumidores inmediatos. La edición futura conservará la identidad completa del grafo aunque la ventana visual solo muestre el contexto más útil alrededor del bloque actual.

## Colores

```text
verde brillante   señal TRUE / conexión activa
verde oscuro      bloque seleccionado o nodo activo
blanco            texto principal
verde de marca    acentos y selección
amarillo          STOPPED / advertencia
rojo              FAULT
negro/gris        fondo, nodos inactivos y conexiones FALSE
```

## Controles

### DIAGRAMA

```text
UP / DOWN       bloque anterior / siguiente
LEFT / RIGHT    seleccionar DETALLE / LISTA / VOLVER
OK              ejecutar acción seleccionada
ESC             volver directamente a IDLE
```

La selección de bloques es circular:

```text
B00 <- UP -> último bloque
último bloque <- DOWN -> B00
```

### DETALLE

```text
UP / LEFT       bloque anterior
DOWN / RIGHT    bloque siguiente
OK              regresar a DIAGRAMA
ESC             volver directamente a IDLE
```

El identificador se muestra sin ambigüedad:

```text
B06 | 7 de 7
```

## Renderizado

Se aplican las reglas aprobadas de `USER_UI_RENDERING_RULES.md`:

- fondo, paneles, títulos y comandos se dibujan solo al entrar;
- cambiar de bloque limpia únicamente el viewport gráfico;
- cambiar una señal repinta las mismas conexiones y nodos, sin reconstruir la pantalla completa;
- el runtime se consulta cada 100 ms para los valores visibles;
- si nada cambia, no se escribe en la TFT;
- una conexión activa usa dos píxeles de grosor;
- al volver a FALSE se limpia explícitamente el segundo píxel para evitar residuos verdes.

## Arquitectura

La vista vive en archivos propios:

```text
src/screens/RuntimeUIDiagram.h
src/screens/RuntimeUIDiagram.cpp
```

No se añadió dibujo al núcleo `JWPLC_LogicRuntime`.

La interfaz usa únicamente APIs de lectura:

```cpp
runtime.program();
runtime.blockCount();
runtime.blockDefinition(index);
runtime.blockValue(index);
```

No modifica el programa ni accede a FRAM, SD o Ethernet desde el callback gráfico.

## Alcance de seguridad

La prueba usa el ejemplo:

```text
JWPLC_LogicRuntime_UI_Blocks
```

Este ejemplo:

- carga siete bloques en RAM;
- no contiene bloques `DigitalOutput`;
- no conmuta Q0;
- no formatea la FRAM;
- no escribe la FRAM;
- ejecuta el runtime únicamente para mostrar valores en vivo.

## Programa de prueba

```text
B00  ENTRADA I0.0
B01  ENTRADA I0.1
B02  NOT B00
B03  B00 AND B01
B04  B02 OR B03
B05  SET=B03 RESET=B01 RETENTIVO
B06  TON B04 2000 ms
```

## Salida serial esperada

```text
Storage: OK
Runtime: OK
Programa RAM: OK
RUN: OK
Runtime UI: OK
```

## Checklist físico

```text
[ ] HOME -> BLOQUES abre DIAGRAMA, no la tabla.
[ ] La transición es rápida y no parpadea.
[ ] B00 muestra I0.0 como origen físico.
[ ] B01 muestra I0.1 como origen físico.
[ ] B02 muestra B00 -> NOT -> consumidores.
[ ] B03 muestra dos fuentes A/B.
[ ] B04 muestra B02 y B03 como fuentes.
[ ] B05 etiqueta sus puertos S/R y muestra MEMORIA RET.
[ ] B06 muestra B04 -> TON 2000 ms -> FIN.
[ ] UP/DOWN recorre circularmente B00..B06.
[ ] LEFT y RIGHT recorren correctamente las tres acciones.
[ ] DETALLE abre el bloque seleccionado.
[ ] DETALLE muestra Bxx y posición humana sin contradicción.
[ ] OK desde DETALLE regresa a DIAGRAMA.
[ ] LISTA abre la tabla técnica existente.
[ ] VOLVER desde LISTA regresa a DIAGRAMA.
[ ] VOLVER desde DIAGRAMA regresa a HOME.
[ ] Activar entradas cambia nodos y conexiones a verde.
[ ] Volver a FALSE no deja residuos verdes.
[ ] Q0 permanece apagada.
[ ] La FRAM original no se modifica.
```

## Dirección de desarrollo aprobada

`DIAGRAMA` será la superficie principal de programación. La evolución prevista es:

```text
v0.3.1  diagrama navegable de solo lectura
v0.4    menú contextual del bloque
v0.5    edición de parámetros y fuentes
v0.6    inserción y eliminación
v0.7    guardado transaccional del programa editado
```

La tabla y el detalle permanecen disponibles para diagnóstico, pero no serán el flujo principal de programación.