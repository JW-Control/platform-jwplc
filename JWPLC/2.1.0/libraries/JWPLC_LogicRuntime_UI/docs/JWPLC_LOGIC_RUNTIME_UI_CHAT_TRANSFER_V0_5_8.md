# Transferencia de chat — JWPLC Logic Runtime UI v0.5.8

## Instrucción para el nuevo chat

Usa este documento como punto de partida. Antes de modificar código:

1. revisa el branch indicado directamente en GitHub;
2. confirma el estado real de los archivos actuales;
3. no asumas que la v0.5.8 está aprobada;
4. realiza cambios incrementales y explica archivo, función y motivo;
5. cierra primero los dos defectos físicos pendientes;
6. no avances todavía a eliminación de bloques ni a nuevos tipos.

## Repositorio y branch

```text
Repositorio: JW-Control/platform-jwplc
Branch activo: feature/logic-runtime-poc
Ruta del package: JWPLC/2.1.0
Librería de motor: JWPLC_LogicRuntime
Librería gráfica: JWPLC_LogicRuntime_UI
Ejemplo principal: JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
Package local de prueba: jwplc_local:esp32 2.1.0-dev
```

La rama está muy adelantada respecto a `main` y contiene todo el desarrollo experimental del Logic Runtime. No mover estos cambios a otro branch ni abrir PR antes de cerrar los pendientes actuales.

## Objetivo del desarrollo

Crear un runtime lógico tipo FBD/LOGO! para el JWPLC Basic, ejecutable y editable desde la TFT y botonera integradas, manteniendo:

```text
estabilidad
compatibilidad Arduino IDE
edición transaccional en RAM
renderizado sin parpadeo
persistencia FRAM separada del callback TFT
navegación coherente con OK/ESC/LEFT/RIGHT/UP/DOWN
```

No asumir todavía:

```text
OpenPLC integrado
OTA definida
persistencia automática de cada edición
programa vacío soportado
inserción arbitraria en medio del orden topológico
```

## Arquitectura actual

### Motor

`JWPLC_LogicRuntime` contiene el motor y el modelo lógico. La UI no debe mezclar su lógica con almacenamiento, TFT ni botonera.

El prototipo v2 trabaja con:

```text
LogicV2BlockRecord
LogicV2InputLink
LogicV2Program
LogicV2EnginePrototype
LogicVariableInputPrototype
```

Las fuentes de un bloque deben apuntar a bloques anteriores o a fuentes especiales válidas.

### UI

`JWPLC_LogicRuntime_UI` conecta el motor con:

```text
JWPLC_Display
JW_MatrixButtons / JWPLC_Buttons
callbacks USER
pantallas FBD
sesión de edición RAM
```

La interfaz activa declarada es:

```text
JWPLC_LogicRuntime_UI 0.5.8
RuntimeUIFBDMapV11
```

Existe una cadena histórica de clases:

```text
RuntimeUIFBDMapV4
→ V5
→ V6
→ V7
→ V8
→ V9
→ V10
→ V11
```

Esta herencia acumulada es relevante para el bug del nodo `+`: una clase base todavía puede dibujar contenido aunque V11 intente ocultarlo después.

## Reglas técnicas obligatorias

### Acciones diferidas

La UI no debe ejecutar dentro del callback TFT:

```text
FRAM
SD
Ethernet
codec de programa
operaciones largas
carga/guardado persistente
```

Patrón aprobado:

```text
callback gráfico registra solicitud
→ callback termina
→ se libera SPI
→ JWPLC_LogicRuntime_UI.update() o processV2EditorPending()
→ siguiente refresh muestra resultado
```

### Renderizado

Solo redibujar:

```text
el bloque que cambió
el enlace que cambió
el campo cuyo valor cambió
la selección anterior y nueva
```

Redibujado completo únicamente al:

```text
entrar a una pantalla
cambiar viewport/página/layout
aplicar una edición estructural
forzar redraw explícito
```

### Botonera

`JWPLC_Buttons.pressed()` es consumible. No debe consultarse el mismo botón en dos capas durante el mismo refresh.

El repeat validado utiliza `applyAxis()`. El microfix aprobado por el usuario para tiempos es:

```cpp
setRepeatInitialDelay(170);
setRepeatProfile(
    10, 25, 50,
    step1, step2, step3, step4,
    120, 90, 70, 50);
```

## Estado validado hasta v0.5.5

### Motor y almacenamiento experimental

Se validaron previamente:

```text
programa A/B en RAM
ventana FRAM
arranque desde FRAM
CRC y fallback
startup policy
retentivos
TON
SR
entradas variables
DigitalOutput lógico
```

### Sesión estructural RAM v0.5.5

Prueba física aprobada:

```text
RESULTADO: 34 OK, 0 FAIL
PRUEBA APROBADA
```

Quedó validado:

```text
appendBlock()
consumerCount()
hasConsumers()
removeBlock()
compactación de bloques y enlaces
corrección de firstInput
renumeración de fuentes
rollback atómico
apply al motor
scan posterior correcto
```

No escribe FRAM ni conmuta salidas físicas.

## Estado visual validado

### Refresco regional

Después de la revisión v0.5.4:

- el parpadeo general se redujo drásticamente;
- el mapa y detalle actualizan regiones concretas;
- `Ta` puede actualizarse sin reconstruir toda la pantalla;
- `EDITAR T` usa repeat mantenido.

### Nuevo bloque

La selección de tipo `NUEVO BLOQUE` ya no parpadea completamente con `UP/DOWN`.

Tipos iniciales implementados:

```text
ENTRADA DI
NOT
AND 2
TON
SALIDA DO
```

La creación sigue siendo append-only y transaccional en RAM.

### Mini mapa FBD

El mini mapa contextual fue aprobado visualmente por el usuario.

Muestra:

```text
bloques reales simplificados
enlaces
bloque seleccionado en amarillo
bloques activos en verde
posición Fxx/Cxx
```

No debe representar como bloques:

```text
HI
LO
ABIERTO
I0.x
Q0.x
```

## Diseño acordado de CONFIGURAR

La pantalla principal debe mantener una única fila:

```text
FUENTE | PARAMETROS | CREAR
```

Casos:

```text
NOT / AND 2
FUENTE | SIN PARAMETROS | CREAR

TON / SALIDA DO
FUENTE | PARAMETROS | CREAR

ENTRADA DI
SIN FUENTE | PARAMETROS | CREAR
```

`SIN PARAMETROS` y `SIN FUENTE`:

```text
gris
no seleccionable
LEFT/RIGHT los omite
```

### Una fuente

Para NOT, TON y DO:

```text
FUENTE
→ OK
→ una fila con la fuente actual
→ UP/DOWN cambia fuente
→ mini mapa fijo debajo
```

### Varias fuentes

Para AND 2 y bloques futuros:

```text
FUENTE
→ lista IN1 / IN2 / ...
→ UP/DOWN elige entrada
→ OK abre una sola fila de edición
→ UP/DOWN cambia fuente
→ mini mapa fijo debajo
```

### Parámetros

Flujo general:

```text
PARAMETROS
→ lista de parámetros
→ UP/DOWN selecciona
→ OK abre editor del parámetro
```

TON actual:

```text
T
→ VALOR | UNIDAD
→ LEFT/RIGHT cambia campo
→ UP/DOWN modifica
→ OK acepta
→ ESC cancela el parámetro actual
```

DI/DO:

```text
RECURSO I0.x / Q0.x
```

## Corrección local de compilación

Un checkout remoto de v0.5.8 contiene:

```cpp
static constexpr int16_t LIST_H = 24;
```

Ese nombre colisiona con una macro y produce:

```text
expected unqualified-id before '=' token
```

Corrección local validada:

```text
LIST_H → LIST_ROW_H
```

Debe aplicarse tanto en:

```text
RuntimeUIFBDMapV11.h
RuntimeUIFBDMapV11.cpp
```

Antes de continuar, incorporar este rename al branch y verificar compilación limpia desde un checkout nuevo.

## Defectos abiertos prioritarios

### P0 — el nodo `+` se superpone al último bloque

Resultado físico actual:

```text
B10 Q0.1 queda parcialmente cubierto por un + compacto
```

Esto sigue ocurriendo aunque V11 intente establecer `_addPreviewDrawn = true`.

Resultado esperado:

```text
MAPA NORMAL
- no existe ningún preview compacto de +

RIGHT desde la última columna
- desplazar a columna virtual exclusiva
- mostrar allí el bloque + completo
```

No intentar resolverlo solo limpiando la región después de dibujar. El preview debe dejar de generarse desde el flujo activo.

Archivos a revisar:

```text
RuntimeUIFBDMapV8.cpp/.h
RuntimeUIFBDMapV9.h
RuntimeUIFBDMapV10.cpp/.h
RuntimeUIFBDMapV11.cpp/.h
```

Símbolos clave:

```text
drawAddPreview()
drawAddNode(... compact ...)
addPreviewVisible()
_addPreviewDrawn
refreshNormalMap()
refresh()
enter()
forceRedraw()
```

Recomendación: rastrear todas las llamadas reales y eliminar el preview compacto en el origen. Considerar refactorizar el nodo `+` fuera de la cadena heredada si la superposición viene de múltiples overrides.

### P0 — ESC envía a IDLE en vez de volver al padre

Resultado físico actual:

```text
ESC desde pantallas anidadas puede abandonar USER y mostrar IDLE
```

Resultado esperado:

```text
PARAMETER EDIT → PARAMETER LIST
PARAMETER LIST → CONFIGURAR
SOURCE EDIT → SOURCE LIST o CONFIGURAR
SOURCE LIST → CONFIGURAR
CONFIGURAR → NUEVO BLOQUE
NUEVO BLOQUE → nodo +
NODO + → MAPA FBD
MAPA FBD → vista USER que lo abrió
```

El mapa no debe asumir que su padre es `IDLE`.

Archivos a revisar:

```text
JWPLC_LogicRuntime_UI.cpp
JWPLC_LogicRuntime_UI.h
RuntimeUIView.h
callbacks USER de JWPLC_Display
RuntimeUIFBDMapV11.cpp
```

Puntos clave:

```text
orden de pressed(BTN_ESC)
router global
consumeInputReleaseGate()
gateInputUntilRelease()
transición USER/IDLE
vista de retorno
```

Norma nueva:

```text
docs/USER_UI_NAVIGATION_STACK_RULES.md
```

Recomendación: implementar una pila pequeña de pantallas o registrar explícitamente la vista padre. La pantalla activa debe consumir `ESC` antes del router global.

## Documentos actuales importantes

```text
JWPLC_LogicRuntime_UI/docs/USER_UI_ACTION_RULES.md
JWPLC_LogicRuntime_UI/docs/USER_UI_RENDERING_RULES.md
JWPLC_LogicRuntime_UI/docs/USER_UI_STYLE_GUIDE.md
JWPLC_LogicRuntime_UI/docs/USER_UI_NAVIGATION_STACK_RULES.md
JWPLC_LogicRuntime_UI/docs/RUNTIME_UI_FBD_CONFIG_GROUPS_V0_5_8_TEST.md
JWPLC_LogicRuntime_UI/docs/RUNTIME_UI_FBD_CONFIG_GROUPS_V0_5_8_PHYSICAL_RESULT.md
JWPLC_LogicRuntime_UI/docs/RUNTIME_UI_STRUCTURAL_EDIT_SESSION_V0_5_5_TEST.md
```

## Orden recomendado para el siguiente chat

### Paso 1 — sincronización y compilación limpia

```text
Fetch/Pull branch
aplicar LIST_H → LIST_ROW_H en repo
compilar ejemplo FBD desde checkout limpio
```

### Paso 2 — cerrar el nodo `+`

```text
instrumentar o revisar cadena V8→V11
identificar la llamada exacta que dibuja preview compacto
eliminar el preview desde el origen
mantener solo la columna virtual seleccionada
validar físicamente sobre B10
```

### Paso 3 — cerrar navegación ESC

```text
definir padre/pila de vistas
priorizar consumo de ESC en pantalla activa
volver un solo nivel
impedir transición global a IDLE desde subpantallas
validar todos los niveles del asistente
```

### Paso 4 — regresión visual

```text
confirmar que no vuelve el parpadeo
confirmar mini mapa con tamaño fijo
confirmar disabled labels no seleccionables
confirmar repeat 10/25/50
confirmar creación transaccional
```

### Paso 5 — decisión de versión

Si ambos defectos se corrigen sin cambiar el diseño:

```text
publicar como v0.5.9
```

Registrar resultado físico antes de continuar.

### Paso 6 — solo después

```text
DETALLE → ACCIONES
ELIMINAR BLOQUE
bloqueo por consumidores
mensaje de usos
configuración de recursos
más tipos de bloque
```

## Prueba mínima de aceptación

```text
[ ] Compila sin microfix local.
[ ] B10 no está cubierto por +.
[ ] No existe + compacto en mapa normal.
[ ] RIGHT abre + en columna virtual propia.
[ ] ESC vuelve un nivel desde cada subpantalla.
[ ] ESC desde MAPA vuelve a su vista padre.
[ ] NUEVO BLOQUE no parpadea con UP/DOWN.
[ ] Mini mapa mantiene tamaño constante.
[ ] SIN PARAMETROS/SIN FUENTE no reciben foco.
[ ] AND 2 permite elegir IN1/IN2 y luego fuente.
[ ] TON permite lista de parámetros y VALOR/UNIDAD.
[ ] Repeat mantenido conserva perfil 10/25/50.
[ ] Crear aplica en RAM fuera del callback TFT.
[ ] No se escribe FRAM.
[ ] No se conmutan salidas físicas.
```

## Prompt sugerido para iniciar el nuevo chat

```text
Revisa @GitHub el branch feature/logic-runtime-poc del repositorio JW-Control/platform-jwplc y usa como transferencia:

JWPLC/2.1.0/libraries/JWPLC_LogicRuntime_UI/docs/JWPLC_LOGIC_RUNTIME_UI_CHAT_TRANSFER_V0_5_8.md

Estamos desarrollando el editor FBD del JWPLC Basic. La v0.5.8 no está aprobada. Antes de añadir funciones nuevas, corrige incrementalmente dos defectos físicos:

1. El nodo compacto (+) sigue superponiéndose sobre B10. En el mapa normal no debe dibujarse ningún preview; solo debe aparecer el bloque + completo cuando RIGHT abre una columna virtual exclusiva.
2. ESC desde subpantallas envía a IDLE. Debe volver exactamente un nivel y el MAPA FBD debe regresar a la vista USER que lo abrió.

Primero incorpora también el microfix LIST_H → LIST_ROW_H en RuntimeUIFBDMapV11.h/.cpp para que un checkout limpio compile.

Revisa la cadena real V8→V11, el consumo de JWPLC_Buttons.pressed(BTN_ESC), el router global y los callbacks USER. No hagas reemplazos masivos ni avances a eliminar bloques. Indica cada archivo, función y motivo. Mantén renderizado regional, acciones diferidas fuera del callback TFT, edición transaccional en RAM, sin FRAM y sin salidas físicas.
```
