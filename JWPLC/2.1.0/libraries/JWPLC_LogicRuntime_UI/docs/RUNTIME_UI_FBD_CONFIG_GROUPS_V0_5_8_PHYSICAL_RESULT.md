# Resultado físico parcial — configuración jerárquica FBD v0.5.8

## Estado

```text
RESULTADO PARCIAL
NO APROBADO
```

La revisión compila después de corregir localmente un conflicto de macro con `LIST_H`, y varias mejoras visuales quedaron verificadas. Sin embargo, siguen abiertos dos defectos de navegación/renderizado que impiden aprobar v0.5.8.

## Entorno validado

```text
Repositorio: JW-Control/platform-jwplc
Branch: feature/logic-runtime-poc
Package local: jwplc_local:esp32 2.1.0-dev
Ejemplo: JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
UI declarada: 0.5.8
```

## Corrección local necesaria para compilar

El compilador reportó:

```text
RuntimeUIFBDMapV11.h:63:35: error: expected unqualified-id before '=' token
static constexpr int16_t LIST_H = 24;
```

`LIST_H` colisiona con una macro proveniente de headers incluidos. La corrección local validada fue renombrar el identificador en `RuntimeUIFBDMapV11.h` y `RuntimeUIFBDMapV11.cpp`:

```cpp
LIST_H
→ LIST_ROW_H
```

Esta corrección debe incorporarse al branch para que un checkout limpio compile sin intervención manual.

## Funcionalidad verificada

### Navegación incremental de NUEVO BLOQUE

- `UP/DOWN` ya no provoca redibujado completo.
- El parpadeo general observado en v0.5.6/v0.5.7 quedó eliminado.
- Solo cambian los elementos de selección necesarios.

### Mini mapa FBD

- El mini mapa contextual fue aprobado visualmente.
- Permite identificar el bloque seleccionado dentro de la estructura.
- La indicación de fila/columna aporta contexto útil.
- Las constantes y recursos no se representan como bloques ficticios.

### Organización jerárquica

La propuesta se mantiene aprobada conceptualmente:

```text
FUENTE | PARAMETROS | CREAR
```

- `SIN PARAMETROS` debe permanecer gris y no seleccionable.
- Para varias entradas se selecciona primero `IN1/IN2/...` y después su fuente.
- Los parámetros usan lista general y editor específico de valor/unidad.

## Defecto abierto 1 — el nodo `+` continúa superpuesto

### Resultado observado

El símbolo compacto `+` sigue apareciendo sobre el último bloque visible, particularmente sobre `B10 Q0.1`.

### Resultado esperado

```text
MAPA NORMAL
- ningún preview compacto de +
- ningún indicador dibujado dentro o encima de un bloque real

RIGHT DESDE LA ÚLTIMA COLUMNA
- desplazar a una columna virtual exclusiva
- dibujar allí el bloque + completo
```

El nodo `+` debe existir visualmente solo cuando está seleccionado en su propia columna virtual. No se debe intentar ocultarlo mediante flags posteriores al dibujo; la llamada que genera el preview debe eliminarse o quedar inalcanzable desde el flujo activo.

### Archivos a revisar

```text
RuntimeUIFBDMapV8.cpp/.h
RuntimeUIFBDMapV9.h
RuntimeUIFBDMapV10.cpp/.h
RuntimeUIFBDMapV11.cpp/.h
```

Buscar especialmente:

```text
drawAddPreview()
drawAddNode(... compact ...)
_addPreviewDrawn
addPreviewVisible()
refreshNormalMap()
refresh()
enter()
forceRedraw()
```

Debe comprobarse el orden real de llamadas entre las clases heredadas. El defecto sugiere que una implementación base todavía dibuja el preview antes o después de que V11 modifique `_addPreviewDrawn`.

## Defecto abierto 2 — ESC abandona USER y entra a IDLE

### Resultado observado

Desde pantallas anidadas del editor, `ESC` puede enviar directamente a `IDLE` en lugar de regresar a la pantalla padre.

### Resultado esperado

`ESC` debe realizar navegación jerárquica de un solo nivel:

```text
PARAMETER EDIT   → PARAMETER LIST
PARAMETER LIST   → CONFIGURAR
SOURCE EDIT      → SOURCE LIST o CONFIGURAR
SOURCE LIST      → CONFIGURAR
CONFIGURAR       → NUEVO BLOQUE
NUEVO BLOQUE     → nodo + seleccionado
NODO +           → MAPA FBD
MAPA FBD         → vista anterior desde la que se abrió
```

No debe existir una salida global automática a `IDLE` mientras una vista USER activa tenga una pantalla padre válida.

### Posibles puntos de conflicto a inspeccionar

```text
JWPLC_LogicRuntime_UI.cpp
RuntimeUIView.h
callbacks USER de JWPLC_Display
orden de consumo de JWPLC_Buttons.pressed(BTN_ESC)
consumeInputReleaseGate()
gateInputUntilRelease()
```

`pressed()` es consumible. La pantalla activa debe tener prioridad para consumir `ESC` antes de cualquier manejador global de salida a IDLE.

## Regla de navegación acordada

```text
ESC = volver exactamente un nivel
OK  = entrar, editar, aceptar o ejecutar la acción seleccionada
LEFT/RIGHT = cambiar foco horizontal cuando la pantalla lo defina
UP/DOWN = recorrer elementos dentro del grupo activo
```

La transición a IDLE no debe estar asociada implícitamente a cualquier `ESC`. Debe ocurrir únicamente cuando la navegación de la aplicación lo defina expresamente, preferentemente mediante una acción visible de salida o desde la raíz real de USER.

## Criterio de aprobación pendiente

```text
[ ] Checkout limpio compila sin renombrar LIST_H manualmente.
[ ] El + no se dibuja en el mapa normal.
[ ] El + solo aparece en una columna virtual seleccionada.
[ ] Ningún bloque real queda cubierto por el +.
[ ] ESC vuelve un nivel en todas las subpantallas.
[ ] ESC desde MAPA FBD vuelve a la vista padre, no a IDLE.
[ ] La navegación no produce doble consumo de botones.
[ ] El mini mapa conserva el tamaño aprobado.
[ ] No reaparece el parpadeo general.
[ ] La creación continúa siendo transaccional en RAM.
[ ] No se escribe FRAM.
[ ] No se conmutan salidas físicas.
```

## Decisión

No avanzar todavía a `ACCIONES / ELIMINAR BLOQUE`. Primero deben cerrarse el nodo `+`, la pila de navegación y la compilación limpia de v0.5.8. Después se repite la prueba física completa y se registra un resultado aprobado o una nueva revisión v0.5.9.
