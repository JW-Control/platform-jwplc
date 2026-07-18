# Prueba física — configuración jerárquica FBD v0.5.8

## Objetivo

Validar la reorganización del asistente `CONFIGURAR` después de la prueba física v0.5.7:

1. eliminar por completo el preview compacto `+` del mapa normal;
2. conservar el nodo `+` únicamente en una columna virtual seleccionada con `RIGHT`;
3. mantener una fila fija de grupos `FUENTE / PARAMETROS / CREAR`;
4. mostrar `SIN PARAMETROS` en gris y omitirlo durante la navegación;
5. separar la selección de una entrada de la selección de su fuente;
6. conservar un mini mapa FBD de tamaño fijo durante la selección de una fuente;
7. generalizar una lista de parámetros y un editor separado de valor/unidad;
8. mantener actualizaciones regionales y aplicación transaccional en RAM.

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
UI FBD v0.5.8: CONFIGURACION JERARQUICA
```

## Prueba 1 — mapa sin preview `+`

1. Entrar a `MAPA FBD`.
2. Seleccionar `B10 Q0.1`.
3. Permanecer sin pulsar botones durante varios ciclos.

Resultado esperado:

- no aparece ningún `+` compacto encima, al lado o dentro de `B10`;
- `B10` conserva todo su borde, texto y terminal de salida;
- los cambios lógicos continúan usando refresco regional;
- `RIGHT` sigue abriendo el nodo virtual de creación.

## Prueba 2 — nodo virtual seleccionado

1. Desde `B10`, pulsar `RIGHT`.

Resultado esperado:

- el mapa cambia a la ventana derecha;
- el `+` aparece como bloque completo en una columna virtual dedicada;
- ningún bloque real queda cubierto;
- `LEFT` o `ESC` regresan al mapa normal sin dejar un preview compacto;
- `OK` abre `NUEVO BLOQUE`.

## Prueba 3 — fila principal de CONFIGURAR

Seleccionar cada tipo inicial y entrar a `CONFIGURAR`.

### NOT

```text
FUENTE | SIN PARAMETROS | CREAR
```

Resultado esperado:

- `SIN PARAMETROS` aparece gris;
- `LEFT/RIGHT` alterna directamente entre `FUENTE` y `CREAR`;
- el elemento gris nunca recibe borde de selección.

### AND 2

```text
FUENTE | SIN PARAMETROS | CREAR
```

Mismas reglas que NOT.

### TON

```text
FUENTE | PARAMETROS | CREAR
```

Los tres grupos son seleccionables.

### ENTRADA DI

```text
SIN FUENTE | PARAMETROS | CREAR
```

`SIN FUENTE` aparece gris y se omite durante la navegación.

### SALIDA DO

```text
FUENTE | PARAMETROS | CREAR
```

Los tres grupos son seleccionables.

En todos los casos el mini mapa/contexto inferior conserva exactamente la misma región y tamaño.

## Prueba 4 — bloque con una fuente

1. Elegir `NOT`.
2. En `CONFIGURAR`, seleccionar `FUENTE` y pulsar `OK`.

Resultado esperado:

- se abre directamente una pantalla con una sola fila:

```text
FUENTE <B10 Q>
```

- debajo se mantiene el mini mapa FBD grande;
- `UP/DOWN` recorre `HI`, `LO` y los bloques `Bxx`;
- un bloque real muestra `Fxx Cxx` y doble borde amarillo;
- `HI/LO` muestran `SIN POSICION FBD` sin crear bloques falsos;
- `OK` acepta y vuelve a la pantalla principal;
- `ESC` restaura la fuente previa y vuelve.

Repetir con `TON` y `SALIDA DO`.

## Prueba 5 — bloque con varias fuentes

1. Elegir `AND 2`.
2. En la pantalla principal, pulsar `OK` sobre `FUENTE`.

Resultado esperado:

- aparece primero la lista de entradas:

```text
IN1 <...>
IN2 <...>
```

- `UP/DOWN` selecciona la entrada;
- `OK` abre el editor de esa entrada;
- el editor muestra una única fila `IN1 <...>` o `IN2 <...>`;
- el mini mapa conserva el tamaño fijo mientras se recorre la fuente;
- `ABIERTO`, `HI` y `LO` no generan nodos gráficos;
- `OK` acepta y regresa a la lista de entradas;
- `ESC` cancela únicamente el cambio de la entrada abierta.

## Prueba 6 — SIN PARAMETROS

1. Configurar `NOT` o `AND 2`.
2. Pulsar varias veces `LEFT/RIGHT`.

Resultado esperado:

- `SIN PARAMETROS` permanece gris;
- la selección nunca se detiene sobre ese elemento;
- no hay parpadeo ni reconstrucción de toda la pantalla;
- solo cambian los botones anterior y nuevo.

## Prueba 7 — lista general de parámetros TON

1. Elegir `TON`.
2. Seleccionar `PARAMETROS`.
3. Pulsar `OK`.

Resultado esperado:

- aparece una lista de parámetros;
- actualmente contiene:

```text
T <1 s>
```

- `UP/DOWN` queda preparado para recorrer parámetros adicionales futuros;
- `OK` abre el parámetro seleccionado;
- `ESC` vuelve a la fila principal.

## Prueba 8 — editor de valor y unidad TON

1. Abrir el parámetro `T`.

Resultado esperado:

```text
VALOR <1> | UNIDAD <s>
```

- ambos campos aparecen en una sola fila;
- `LEFT/RIGHT` alterna entre valor y unidad;
- `UP/DOWN` cambia el campo activo;
- mantener `UP/DOWN` sobre valor usa el perfil validado `10/25/50`;
- el mini mapa inferior conserva como contexto la fuente del TON;
- `OK` acepta el parámetro y vuelve a la lista;
- `ESC` restaura valor y unidad previos.

## Prueba 9 — parámetros RECURSO

Probar:

```text
ENTRADA DI → PARAMETROS → RECURSO I0.x
SALIDA DO  → PARAMETROS → RECURSO Q0.x
```

Resultado esperado:

- la lista general muestra `RECURSO <I0.x>` o `RECURSO <Q0.x>`;
- el editor abre una sola fila de recurso;
- `UP/DOWN` cambia el recurso;
- el panel inferior muestra `SIN POSICION FBD`;
- no se dibuja I/Q como bloque ficticio;
- una `Q0.x` ya utilizada no queda seleccionable para una nueva DO.

## Prueba 10 — creación desde el grupo CREAR

1. Configurar fuentes y parámetros.
2. Volver a la fila principal.
3. Seleccionar `CREAR`.
4. Pulsar `OK`.

Resultado esperado:

- aparece `APLICANDO CAMBIOS...`;
- la sesión completa valida antes de reemplazar el programa;
- la aplicación ocurre desde `processV2EditorPending()`;
- el nuevo bloque queda seleccionado;
- no reaparece el preview compacto `+`;
- `RIGHT` desde la nueva última columna vuelve a abrir la columna virtual;
- no se escribe FRAM;
- no se conmutan salidas físicas.

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
Serial muestra UI FBD v0.5.8.
El + compacto no aparece nunca en el mapa normal.
El + seleccionado ocupa una columna virtual propia.
FUENTE/PARAMETROS/CREAR permanecen en una fila fija.
SIN PARAMETROS queda gris y no recibe selección.
SIN FUENTE queda gris y no recibe selección.
AND 2 muestra primero la lista IN1/IN2.
Cada entrada abre una sola fila de fuente con mini mapa fijo.
TON muestra una lista general de parámetros.
T usa editor separado de VALOR/UNIDAD.
DI/DO editan RECURSO como parámetro.
Los cambios normales son regionales.
OK acepta y ESC cancela en subeditores.
La creación sigue siendo transaccional en RAM.
No se escribe FRAM.
No se conmutan salidas físicas.
```
