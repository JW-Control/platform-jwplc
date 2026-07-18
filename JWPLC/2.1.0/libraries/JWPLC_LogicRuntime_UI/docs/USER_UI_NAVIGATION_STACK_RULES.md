# Reglas de navegación jerárquica para pantallas USER

## Objetivo

Evitar que un botón de retroceso abandone inesperadamente la interfaz USER y envíe al usuario a `IDLE` cuando todavía existe una pantalla padre válida.

Estas reglas complementan:

```text
USER_UI_ACTION_RULES.md
USER_UI_RENDERING_RULES.md
USER_UI_STYLE_GUIDE.md
```

## Regla principal

```text
ESC = regresar exactamente un nivel dentro de la navegación USER
```

`ESC` no significa globalmente `salir a IDLE`.

Una vista o subpantalla activa debe consumir primero `BTN_ESC` y decidir su padre inmediato. Solo la raíz real de USER puede solicitar una salida de USER, y esa transición debe estar documentada explícitamente.

## Prioridad de consumo

`JWPLC_Buttons.pressed()` es consumible. El orden aprobado es:

```text
1. diálogo o editor modal activo
2. subpantalla activa
3. vista principal activa
4. router global de JWPLC_LogicRuntime_UI
5. transición USER/IDLE, únicamente si corresponde
```

Un manejador global no debe consultar ni consumir `BTN_ESC` antes que la pantalla activa.

## Navegación FBD acordada

```text
PARAMETER EDIT
ESC → PARAMETER LIST

PARAMETER LIST
ESC → CONFIGURAR

SOURCE EDIT
ESC → SOURCE LIST, cuando existe más de una entrada
ESC → CONFIGURAR, cuando solo existe una entrada

SOURCE LIST
ESC → CONFIGURAR

CONFIGURAR
ESC → NUEVO BLOQUE

NUEVO BLOQUE
ESC → nodo + seleccionado

NODO + SELECCIONADO
ESC → MAPA FBD

MAPA FBD
ESC → vista USER desde la que se abrió el mapa
```

El último paso no debe asumir `IDLE`. El mapa debe conocer o recibir su vista padre.

## Estrategias permitidas

### Pila explícita de vistas

```cpp
enum class RuntimeUIScreen : uint8_t
{
    Home,
    Program,
    FBDMap,
    NewBlock,
    Configure,
    SourceList,
    SourceEdit,
    ParameterList,
    ParameterEdit
};
```

El router conserva una pila pequeña:

```text
push al entrar
pop con ESC
```

Esta es la opción recomendada cuando varias vistas pueden abrir la misma pantalla.

### Padre explícito por pantalla

Una pantalla puede conservar:

```cpp
RuntimeUIScreen _returnScreen;
```

Es válido cuando el flujo es lineal y existe un único nivel de retorno. No debe utilizarse si una pantalla puede provenir de varios padres sin registrar cuál fue el origen real.

### Estados internos jerárquicos

Un editor encapsulado puede manejar subpantallas mediante un enum local, siempre que:

- consuma `ESC` antes del router global;
- cada estado tenga un padre definido;
- la transición al estado padre no pierda el borrador salvo que corresponda cancelar;
- el último estado devuelva el control a la vista que abrió el editor.

## Reglas de cancelación

`ESC` puede cancelar el borrador del nivel actual, pero no niveles superiores.

Ejemplos:

```text
SOURCE EDIT
ESC → restaura solo la fuente que se estaba editando

PARAMETER EDIT
ESC → restaura solo valor/unidad del parámetro actual

CONFIGURAR
ESC → conserva o descarta el borrador completo según la política documentada
```

Nunca debe ejecutar una acción pendiente después de abandonar la pantalla que la originó.

## Gating de botones

Después de cambiar de pantalla:

```text
gateInputUntilRelease()
```

Debe impedir que la misma pulsación se consuma dos veces, pero no debe convertir el siguiente `ESC` válido en una salida global.

Cada transición debe verificar:

```text
[ ] el botón actual fue consumido una sola vez
[ ] se espera liberación antes de aceptar otra acción
[ ] el estado padre quedó definido antes del redibujado
[ ] no existe un segundo manejador global leyendo ESC en el mismo ciclo
```

## IDLE

La salida a `IDLE` debe ser una decisión explícita. Opciones aceptables:

```text
acción visible SALIR
ESC desde la raíz real de USER, si esa política está documentada
pulsación larga dedicada, si se implementa y documenta
retorno automático por inactividad administrado por JWPLC_Display
```

No es aceptable:

```text
ESC desde cualquier subpantalla → IDLE
```

## Renderizado al volver

Al regresar con `ESC`:

- reconstruir solo la pantalla padre necesaria;
- invalidar únicamente sus cachés dinámicas;
- no redibujar una vista intermedia que no quedará visible;
- no permitir que el router global cambie a otra vista en el mismo refresh.

## Criterio de revisión

```text
[ ] Cada pantalla declara su padre o usa una pila explícita.
[ ] ESC vuelve exactamente un nivel.
[ ] La pantalla activa consume ESC antes del router global.
[ ] No hay doble consumo de pressed(BTN_ESC).
[ ] Cancelar un editor restaura solo su borrador local.
[ ] La transición conserva la política de acciones diferidas.
[ ] El mapa FBD vuelve a la vista que lo abrió.
[ ] Solo la raíz USER puede solicitar IDLE explícitamente.
[ ] gateInputUntilRelease evita rebotes sin romper el siguiente ESC.
```

## Estado

```text
NORMA APROBADA PARA LA SIGUIENTE REVISIÓN DEL EDITOR FBD
MOTIVO: defecto físico v0.5.8 donde ESC abandona USER prematuramente
```
