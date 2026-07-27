# Prueba física — asistente FBD contextual v0.5.7

## Objetivo

Validar las correcciones visuales y de navegación solicitadas después de la primera prueba de creación gráfica v0.5.6:

1. impedir que el preview compacto `+` se superponga sobre el último bloque real;
2. eliminar el redibujado completo al recorrer tipos con `UP/DOWN`;
3. reservar `ESC` como única acción de retroceso en `NUEVO BLOQUE`;
4. eliminar el traslape entre `CONFIGURAR` y el nombre del tipo;
5. mostrar un mini mapa FBD contextual al seleccionar una fuente de bloque;
6. mostrar fila y columna lógica como `Fxx Cxx`;
7. no representar `HI`, `LO`, `ABIERTO`, `I0.x` ni `Q0.x` como bloques ficticios;
8. conservar la aplicación transaccional RAM y el repeat acelerado del tiempo.

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
UI FBD v0.5.7: PREVIEW SEGURO + MINI MAPA FBD
```

## Prueba 1 — preview `+` no invasivo

1. Navegar hasta `B10 Q0.1`, en la última columna del programa base.
2. Permanecer sobre el bloque sin pulsar `RIGHT`.

Resultado esperado:

- no aparece ningún rectángulo `+` encima de `B10`;
- `B10` conserva borde, texto y salida completamente visibles;
- el mapa no parpadea ni se reconstruye continuamente;
- `RIGHT` sigue disponible para entrar al nodo virtual aunque el preview se oculte.

El preview pequeño solo puede mostrarse cuando existe una columna completa libre dentro de la ventana. Con cinco o más columnas se omite.

## Prueba 2 — nodo `+` seleccionado

1. Desde un bloque de la última columna, pulsar `RIGHT`.

Resultado esperado:

- la ventana se desplaza a una columna virtual dedicada;
- aparece un bloque `+` grande con borde amarillo;
- ningún bloque real queda cubierto;
- `OK` abre `NUEVO BLOQUE`;
- `LEFT` o `ESC` desde el nodo virtual regresan al mapa.

## Prueba 3 — selección de tipo sin parpadeo

1. Entrar a `NUEVO BLOQUE`.
2. Recorrer varias veces:

```text
ENTRADA DI
NOT
AND 2
TON
SALIDA DO
```

Resultado esperado:

- solo se repintan el botón anterior y el nuevo;
- el panel derecho, encabezado, marco y fondo permanecen inmóviles;
- no se aprecia un destello general de la pantalla;
- `UP/DOWN` conserva navegación circular.

## Prueba 4 — retroceso exclusivo con ESC

En `NUEVO BLOQUE`:

1. Pulsar `LEFT`.
2. Pulsar `ESC`.

Resultado esperado:

```text
LEFT → no cambia de pantalla
ESC  → vuelve al nodo +
```

`LEFT/RIGHT` vuelven a utilizarse para cambiar de campo únicamente dentro de `CONFIGURAR`.

## Prueba 5 — encabezado de CONFIGURAR

1. Seleccionar `NOT`.
2. Pulsar `OK`.

Resultado esperado:

- el encabezado muestra solamente `CONFIGURAR`;
- no aparece `NOT` superpuesto sobre las últimas letras del título;
- dentro del panel aparece `TIPO: NOT`;
- el badge `RUN` continúa visible y separado.

Repetir con `TON`, `AND 2`, `ENTRADA DI` y `SALIDA DO`.

## Prueba 6 — mini mapa para una fuente real

1. Configurar `NOT`.
2. Dejar `FUENTE <B10 Q>` o seleccionar cualquier `Bxx`.

Resultado esperado:

- en la parte inferior aparece un mini mapa formado por rectángulos pequeños sin nombres;
- el bloque fuente seleccionado tiene doble borde amarillo;
- los bloques activos aparecen verdes y los inactivos con borde neutro;
- se muestran enlaces simples entre bloques visibles;
- la línea de contexto indica algo equivalente a:

```text
B10 Q   Fxx Cxx
```

La fila corresponde a `_lanes + 1` y la columna a `_levels + 1` del layout FBD actual.

## Prueba 7 — cambio incremental de fuente

1. Mantener seleccionado el campo `FUENTE`.
2. Recorrer `Bxx` con `UP/DOWN`.

Resultado esperado:

- solo cambia el botón `FUENTE` y el panel contextual inferior;
- no se reconstruye el resto de la pantalla;
- el bloque amarillo y `Fxx Cxx` siguen a la nueva fuente;
- el sentido de `UP/DOWN` no está invertido.

## Prueba 8 — constantes sin bloque ficticio

Recorrer las fuentes especiales:

```text
HI
LO
ABIERTO   (AND 2)
```

Resultado esperado:

- no aparece un bloque artificial para la constante;
- el panel inferior muestra texto del tipo:

```text
HI - SIN POSICION FBD
LO - SIN POSICION FBD
ABIERTO - SIN POSICION FBD
```

## Prueba 9 — recursos sin mini bloque

Probar:

```text
ENTRADA DI → RECURSO I0.x
SALIDA DO  → RECURSO Q0.x
```

Resultado esperado:

- `I0.x` y `Q0.x` no se dibujan como nodos del mini mapa;
- el panel indica `SIN POSICION FBD`;
- la selección de recursos sigue siendo discreta;
- una salida ya utilizada no aparece como opción válida para una nueva DO.

## Prueba 10 — AND 2

1. Seleccionar `AND 2`.
2. Alternar con `LEFT/RIGHT` entre `IN1` e `IN2`.
3. Elegir dos bloques diferentes.

Resultado esperado:

- solo cambian los dos botones afectados por el foco;
- el mini mapa resalta la fuente del campo seleccionado;
- `IN2` continúa iniciando en `HI`;
- al seleccionar `IN2 <HI>`, no aparece bloque ficticio.

## Prueba 11 — TON y repeat

1. Seleccionar `TON`.
2. Cambiar `FUENTE` y verificar el mini mapa.
3. Pasar a `VALOR`.
4. Mantener `UP` y `DOWN`.
5. Cambiar `UNIDAD` entre `ms/s/min/h`.

Resultado esperado:

- el mini mapa se conserva como contexto de la fuente al editar `VALOR` o `UNIDAD`;
- la pulsación corta cambia una unidad;
- el pulso mantenido usa el perfil de aceleración validado:

```text
Retardo inicial: 170 ms
Umbrales:        10 / 25 / 50
Intervalos:      120 / 90 / 70 / 50 ms
```

- al cambiar unidad se actualizan únicamente `VALOR` y `UNIDAD`;
- al salir del asistente se restaura el perfil normal de navegación.

## Prueba 12 — creación y retorno al mapa

Crear al menos:

```text
NOT desde B10
TON desde el NOT nuevo
DO desde el TON nuevo
```

Resultado esperado:

- aparece `APLICANDO CAMBIOS...`;
- la aplicación ocurre fuera del callback TFT;
- el nuevo bloque queda seleccionado;
- el layout se recalcula;
- si ya no existe espacio seguro, el preview compacto `+` permanece oculto;
- `RIGHT` desde la nueva última columna sigue abriendo el nodo virtual;
- no se escribe FRAM;
- no se conmutan salidas físicas.

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
Serial muestra UI FBD v0.5.7.
El + nunca cubre B10 ni otro bloque real.
NUEVO BLOQUE no parpadea al usar UP/DOWN.
LEFT no retrocede desde NUEVO BLOQUE.
ESC sí retrocede.
CONFIGURAR no traslapa el tipo con el título.
Las fuentes Bxx muestran mini mapa y Fxx/Cxx.
HI/LO/ABIERTO no generan bloques ficticios.
I0.x/Q0.x no generan bloques ficticios.
Los cambios de campo son regionales.
La creación sigue siendo transaccional en RAM.
No se escribe FRAM.
No se conmutan salidas físicas.
```
