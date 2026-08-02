# Prueba de consolidación — motor v2 y renderer FBD activo

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

Esta prueba se ejecuta antes de añadir eliminación o nuevos tipos de bloque.

## Separación de responsabilidades

El problema de frames antiguos no corresponde al motor lógico v2.

```text
Motor v2:
- evalúa bloques y enlaces;
- mantiene parameter/resource;
- no dibuja TFT;
- no procesa navegación.

JWPLC_LogicRuntime_UI:
- procesa botones;
- decide cambios de vista;
- dibuja MAPA, DETALLE y EDITAR T.
```

La regresión provenía de la cadena visual histórica V5/V7/V13/V14: una revisión antigua dibujaba primero y otra la sustituía después. La fachada activa ahora usa `RuntimeUIFBDMapActiveRenderer`, que intercepta las transiciones antes de esos renderers.

## Archivos principales

```text
JWPLC_LogicRuntime/src/JWPLC_LogicRuntime_V2.h
JWPLC_LogicRuntime/src/experimental/LogicV2EnginePrototype.h
JWPLC_LogicRuntime/src/experimental/LogicV2EngineInspection.cpp
JWPLC_LogicRuntime/docs/LOGIC_RUNTIME_V2_CONTRACT.md

JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMap.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMap.cpp
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapActiveRenderer.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapActiveRenderer.cpp
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV5.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV11.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV13.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV14.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV14.cpp
```

## Contrato esperado del motor

```text
Runtime v1: #include <JWPLC_LogicRuntime.h>
Motor v2:   #include <JWPLC_LogicRuntime_V2.h>

LogicV2BlockRecord = 12 bytes
LogicV2InputLink   = 2 bytes
Contrato v2        = 1.0
Esquema de record  = 1
```

No se modifica el evaluador TON, el formato del bloque ni la sesión RAM en este incremento.

## Política TFT vigente

| Pantalla | Periodo de callback |
|---|---:|
| MAPA FBD | 100 ms |
| DETALLE | 100 ms |
| Nodo `+` y asistentes | 40 ms |
| Editor de fuente/entrada | 40 ms |
| EDITAR T | 40 ms |

La UI todavía procesa botones y render dentro de `refresh()`. Por seguridad:

```text
RuntimeUIFBDMap::needsTftRefresh() = true
```

La compuerta previa al SPI está suspendida para FBD. Las cachés regionales siguen evitando escrituras de píxeles cuando el callback no detecta cambios visibles.

La optimización previa al lock solo podrá reactivarse después de separar:

```text
updateInputAndState()  // sin TFT/SPI
needsRender()          // regiones sucias
render()               // con TFT/SPI
```

## Compilación

- [ ] Compila desde checkout limpio.
- [ ] Se compila `RuntimeUIFBDMapActiveRenderer.cpp`.
- [ ] No aparece acceso privado/protegido en V5/V7/V11/V13.
- [ ] No aparece símbolo duplicado de métodos del editor TON.
- [ ] `RuntimeUIFBDMap` hereda de `RuntimeUIFBDMapActiveRenderer`.
- [ ] El sketch valida y arranca el programa RAM.
- [ ] `sizeof(LogicV2BlockRecord) == 12` continúa vigente.

## Regresión crítica de botonera

Con `Trg` permanentemente inactivo:

- [ ] LEFT/RIGHT cambia entre `Trg` y `PARAM T`.
- [ ] OK sobre `PARAM T` abre `EDITAR T`.
- [ ] ESC vuelve a DETALLE y luego a MAPA.
- [ ] UP/DOWN responde donde corresponda.
- [ ] Ninguna acción requiere un cambio lógico del TON.

Repetir durante temporización y después de completar el TON.

## DETALLE → MAPA FBD

La revisión activa intercepta ESC antes del retorno histórico V7.

- [ ] El cambio visible ocurre una sola vez.
- [ ] No se observa un segundo borrado/redibujado.
- [ ] El interior del mapa aparece completo.
- [ ] El título `MAPA FBD` aparece una vez.
- [ ] La información central del bloque no desaparece.
- [ ] RUN/READY/FAULT permanece correcto.
- [ ] La selección conserva el bloque de origen.

## DETALLE → EDITAR T

La revisión activa intercepta OK antes del editor histórico V5.

- [ ] No aparece el layout antiguo de dos campos `VALOR / UNIDAD`.
- [ ] No aparece el texto antiguo `Bxx TON PARAM T` en una sola fila.
- [ ] La primera composición visible ya contiene tres campos:

```text
SEG/HORA/MIN | CENT/SEG/MIN | BASE
```

- [ ] No aparece una pantalla negra completa.
- [ ] No quedan restos de DETALLE.

## Encabezado EDITAR T

Debe permanecer estable:

```text
EDITAR T     Bxx TON          RUN
             PARAM T
```

- [ ] Primera fila: `Bxx TON`.
- [ ] Segunda fila: `PARAM T`.
- [ ] No invade el título.
- [ ] No invade RUN.
- [ ] No cambia a una fila durante la navegación.

## Campos de EDITAR T

- [ ] Al mantener UP/DOWN en el primer componente, solo cambia `<nn>`.
- [ ] El rótulo `SEG`, `MIN` o `HORA` no parpadea.
- [ ] Al mantener UP/DOWN en el segundo componente, solo cambia `<nn>`.
- [ ] El rótulo `CENT`, `SEG` o `MIN` no parpadea.
- [ ] `Ta LECTURA` permanece estático como rótulo.
- [ ] Solo el valor `__:__x` de Ta se actualiza durante el conteo.
- [ ] Cambiar el foco redibuja una vez los dos botones afectados.
- [ ] Cambiar BASE actualiza los rótulos una sola vez porque cambia su significado.

## Pruebas TON ya aprobadas anteriormente

- [x] T estático sin parpadeo en DETALLE.
- [x] Ta estático sin parpadeo en DETALLE.
- [x] Durante temporización, solo Ta se actualiza en DETALLE.
- [x] Al completar, Ta queda estable.
- [x] El formato histórico de Ta no reaparece en DETALLE.

Estas pruebas deben repetirse brevemente después de compilar la revisión activa.

## Regresión general

- [ ] El nodo `+` no vuelve a parpadear a pantalla completa.
- [ ] El panel TON conserva el marco amarillo completo.
- [ ] `T` y `Ta` usan la misma base.
- [ ] ESC conserva navegación jerárquica.
- [ ] DI, NOT, AND2, TON y DO siguen creándose.
- [ ] No se escribe FRAM.
- [ ] No se conmutan salidas físicas.

## Criterio de cierre

```text
Compilación:                    APROBADA / FALLA
Botonera con Trg inactivo:      APROBADA / FALLA
DETALLE -> MAPA, una pasada:    APROBADA / FALLA
Editor antiguo ausente:         APROBADO / FALLA
Encabezado de dos filas:        APROBADO / FALLA
Valores sin parpadeo de rótulo: APROBADOS / FALLA
Regresión TON:                  APROBADA / FALLA
Decisión:                       CONSOLIDACIÓN APROBADA / REQUIERE CORRECCIÓN
```

Solo después de aprobar esta prueba se habilitará `ELIMINAR BLOQUE`.
