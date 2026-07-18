# Prueba física — JWPLC Logic Runtime UI v0.5.9 candidata

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

La versión no debe marcarse como aprobada ni publicarse hasta completar esta prueba.

## Alcance

La candidata v0.5.9 corrige y ajusta exclusivamente:

1. eliminación del preview compacto del nodo `+` en el mapa normal;
2. retorno jerárquico con `ESC` dentro del editor FBD;
3. acceso directo al editor cuando existe un solo parámetro;
4. soporte de listas futuras de hasta cuatro parámetros visibles;
5. indicador de posición `01/08` para listas desplazables.

No incluye:

- eliminación de bloques;
- nuevos tipos de bloque;
- escritura FRAM;
- conmutación de salidas físicas;
- cambios en el motor lógico;
- cambios en `JWPLC_Display.cpp`.

## Archivos principales

```text
src/screens/RuntimeUIFBDMapV11.h
src/screens/RuntimeUIFBDMapV12.h
src/screens/RuntimeUIFBDMapV12.cpp
src/JWPLC_LogicRuntime_UI.h
examples/JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM/
```

## Decisiones de interfaz

### Parámetro único

Cuando el bloque solo contiene un parámetro:

```text
CONFIGURAR
→ PARAMETROS
→ OK
→ editor VALOR / UNIDAD o RECURSO
```

Se elimina la pantalla intermedia que mostraba una lista de un solo elemento.

Al aceptar o cancelar:

```text
editor
→ CONFIGURAR
```

### Varios parámetros

Cuando un bloque futuro contenga dos o más parámetros:

```text
PARAMETROS
→ lista desplazable
→ OK sobre el parámetro
→ editor
```

Reglas:

- máximo cuatro filas visibles;
- `UP/DOWN` mueve selección;
- la ventana se desplaza al salir de las cuatro filas visibles;
- encabezado con posición `01/08`;
- sin scrollbar lateral para conservar el ancho útil de las etiquetas;
- redibujado completo solo cuando cambia el viewport de la lista.

### ESC

`JWPLC_Display` consulta el retorno a IDLE antes del callback USER. Para evitar que consuma `ESC` desde una subpantalla, V12 sincroniza el modo global con la profundidad actual:

```text
raíz MAPA FBD       → IDLE_RETURN_ESC_ONLY
pantalla anidada    → IDLE_RETURN_DISABLED
```

La pantalla activa consume `ESC` y, al volver a la raíz, V12 restaura automáticamente `IDLE_RETURN_ESC_ONLY`.

No se modifica `JWPLC_Display.cpp` ni se reemplaza el hook global `jwplcCanReturnToIdle()`.

## Compilación

- [ ] Sin errores desde checkout limpio.
- [ ] `Used platform` apunta al package local esperado.
- [ ] `RuntimeUIFBDMapV12.cpp` compila como parte de la librería.
- [ ] No existe conflicto por `LIST_ROW_H`.
- [ ] `JWPLC_LogicRuntime_UI.h` activa `RuntimeUIFBDMapV12`.

## Prueba del nodo `+`

- [ ] Entrar a USER y observar el mapa normal.
- [ ] Confirmar que `B10 Q0.1` queda completamente visible.
- [ ] Confirmar que no aparece ningún `+` compacto.
- [ ] Seleccionar un bloque de la última columna.
- [ ] Pulsar `RIGHT`.
- [ ] Confirmar que aparece el bloque `+` completo en una columna virtual propia.
- [ ] Pulsar `LEFT` y confirmar regreso al bloque anterior sin residuos.
- [ ] Repetir usando `ESC` desde el nodo `+`.

## Prueba jerárquica de ESC

Validar una pulsación por nivel:

- [ ] `PARAMETER EDIT → CONFIGURAR` cuando existe un único parámetro.
- [ ] `PARAMETER EDIT → PARAMETER LIST` cuando existan varios parámetros.
- [ ] `PARAMETER LIST → CONFIGURAR`.
- [ ] `SOURCE EDIT → SOURCE LIST` para varias entradas.
- [ ] `SOURCE EDIT → CONFIGURAR` para una entrada.
- [ ] `SOURCE LIST → CONFIGURAR`.
- [ ] `CONFIGURAR → NUEVO BLOQUE`.
- [ ] `NUEVO BLOQUE → nodo +`.
- [ ] `nodo + → MAPA FBD`.
- [ ] Desde la raíz del mapa, `ESC` conserva el retorno normal a IDLE del ejemplo directo.

En ningún nivel anidado debe aparecer IDLE.

## Prueba de parámetros actuales

### TON

- [ ] Abrir `PARAMETROS` desde CONFIGURAR.
- [ ] Confirmar que abre directamente `PARAMETRO: T`.
- [ ] `LEFT/RIGHT` alterna entre `VALOR` y `UNIDAD`.
- [ ] `UP/DOWN` modifica el campo seleccionado.
- [ ] El repeat mantenido continúa operativo en `VALOR`.
- [ ] `OK` acepta y vuelve a CONFIGURAR.
- [ ] `ESC` restaura el respaldo y vuelve a CONFIGURAR.

### ENTRADA DI / SALIDA DO

- [ ] Abrir `PARAMETROS`.
- [ ] Confirmar acceso directo al editor `RECURSO`.
- [ ] `UP/DOWN` cambia el recurso.
- [ ] `OK` acepta y vuelve a CONFIGURAR.
- [ ] `ESC` cancela y vuelve a CONFIGURAR.

## Regresión

- [ ] Sin parpadeo general al navegar.
- [ ] Mini mapa conserva dimensiones.
- [ ] `SIN PARAMETROS` y `SIN FUENTE` siguen sin recibir foco.
- [ ] AND 2 conserva selección `IN1 / IN2`.
- [ ] Creación sigue siendo append-only y transaccional en RAM.
- [ ] No se escribe FRAM.
- [ ] No se conmutan salidas físicas.

## Resultado

```text
Compilación:
Prueba nodo +:
Prueba ESC:
Prueba parámetros:
Regresión:
Observaciones:
Decisión: APROBADA / REQUIERE CORRECCIÓN
```
