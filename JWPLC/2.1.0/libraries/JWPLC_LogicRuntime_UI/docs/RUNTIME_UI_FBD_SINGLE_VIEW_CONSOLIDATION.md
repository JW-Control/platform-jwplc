# Consolidación FBD en una sola vista

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN ARDUINO Y VALIDACIÓN FÍSICA
```

Rama de trabajo:

```text
refactor/fbd-single-view
```

La rama validada anteriormente permanece disponible como respaldo:

```text
feature/logic-runtime-poc
```

## Objetivo

Eliminar de la ruta activa la herencia acumulativa:

```text
V4 -> V5 -> V7 -> V13 -> V14 -> ActiveRenderer
```

La vista pública ahora es una clase autónoma:

```text
RuntimeUIFBDMap final
```

No incluye, no hereda y no llama ninguna clase `RuntimeUIFBDMapVx`.

## Máquina de estados única

```text
MAPA
DETALLE
EDITAR IN
EDITAR T
NUEVO BLOQUE / TIPO
CONFIGURAR
ELEGIR FUENTE
ELEGIR RECURSO
EDITAR T de bloque nuevo
```

Todas las transiciones se procesan desde la misma clase y comparten:

- una cabecera;
- una sesión transaccional;
- un layout FBD;
- una política de limpieza;
- una política de refresco;
- un gate de liberación de botones.

## Política general de render

### Entrada desde IDLE

Solo la entrada inicial desde IDLE puede ejecutar un borrado completo de TFT.

### Transiciones internas

MAPA, DETALLE, EDITAR IN, EDITAR T y el asistente comparten el panel interior:

```text
x=3..316
y=28..167
```

Las transiciones internas sustituyen únicamente esta región con `COLOR_PANEL`.

La cabecera se divide en regiones independientes:

```text
Título:      x=0..107
Contexto:    x=108..232
Estado:      x=239..313
Fila 1:      y=3
Fila 2:      y=12
```

Cada región se repinta solo cuando cambia su propio contenido. Por ejemplo:

```text
DETALLE -> EDITAR IN
B07 TON permanece
RUN permanece
solo cambia el título y la segunda fila
```

## Política de entrada

`needsTftRefresh()` permanece en `true`.

La UI todavía procesa botones dentro de `refresh()`. Por tanto, nunca se omite el callback completo en función de cambios visuales. Esto evita repetir la regresión donde los botones solo respondían al cambiar `Trg`.

## Motor v2

Esta consolidación no modifica:

```text
LogicV2EnginePrototype
LogicV2BlockRecord = 12 bytes
LogicV2InputLink = 2 bytes
parameter
resource
scan TON
sesión RAM
FRAM
salidas físicas
```

La vista usa únicamente el contrato explícito `JWPLC_LogicRuntime_V2.h`.

## Archivos activos

```text
src/screens/RuntimeUIFBDMap.h
src/screens/RuntimeUIFBDMap.cpp
src/screens/RuntimeUIFBDMapLayout.cpp
src/screens/RuntimeUIFBDMapInput.cpp
src/screens/RuntimeUIFBDMapTon.cpp
src/screens/RuntimeUIFBDMapWizard.cpp
src/screens/RuntimeUIFBDMapRenderHeader.cpp
src/screens/RuntimeUIFBDMapRenderMap.cpp
src/screens/RuntimeUIFBDMapRenderDetail.cpp
```

## Archivos históricos

Las revisiones `RuntimeUIFBDMapV2...V14` permanecen temporalmente bajo `src` como respaldo durante la validación física, pero no participan en la ruta activa.

Después de aprobar la consolidación deben moverse fuera de `src` o eliminarse en un commit separado. Esto evitará que Arduino las compile y reducirá tiempo de construcción.

## Validación estática realizada

- [x] Sintaxis C++11 de las unidades consolidadas.
- [x] `-Wall -Wextra -Wpedantic -Werror` sin advertencias.
- [x] Enlace parcial de todas las unidades consolidadas.
- [x] Sin métodos `RuntimeUIFBDMap::*` pendientes de definición.
- [x] Firmas contrastadas con `JW_MatrixButtons`.
- [x] `pressed()` se consume una sola vez por botón y evento.
- [x] `applyAxis()` recibe valores `uint32_t`.
- [x] No existe include hacia `RuntimeUIFBDMapVx` en la clase pública.

Esto no sustituye la compilación con Arduino CLI ni la prueba física.

# Checklist físico

## Compilación

- [ ] Compila desde checkout limpio.
- [ ] No aparecen errores de métodos faltantes o duplicados.
- [ ] No aparecen errores de acceso `private/protected` de clases Vx.
- [ ] El log compila los archivos consolidados.

## Botonera

Con `Trg` inactivo:

- [ ] MAPA responde a LEFT/RIGHT/UP/DOWN/OK.
- [ ] DETALLE responde a LEFT/RIGHT/UP/DOWN/OK/ESC.
- [ ] EDITAR IN responde sin depender de cambios del motor.
- [ ] EDITAR T responde sin depender de cambios del motor.

## Cabecera

- [ ] MAPA muestra dos filas sin traslape.
- [ ] DETALLE muestra dos filas sin posición provisional.
- [ ] EDITAR IN muestra dos filas sin `Bxx ... IN1/1` residual.
- [ ] EDITAR T muestra dos filas sin desplazamiento.
- [ ] `Bxx TIPO` no se limpia si permanece igual.
- [ ] RUN no se limpia si permanece igual.

## Transiciones

- [ ] IDLE -> MAPA presenta un único cambio normal.
- [ ] MAPA -> DETALLE no muestra barrido negro completo.
- [ ] DETALLE -> MAPA conserva la transición limpia aprobada.
- [ ] DETALLE -> EDITAR IN no muestra barrido negro completo.
- [ ] EDITAR IN -> DETALLE no conserva `EDITAR IN` ni una cabecera antigua.
- [ ] DETALLE -> EDITAR T presenta directamente los tres campos.
- [ ] EDITAR T -> DETALLE no presenta cabecera residual.

## DETALLE

- [ ] Trg y PARAM T cambian correctamente.
- [ ] El indicador amarillo queda completo.
- [ ] T estático no parpadea.
- [ ] Ta estático no parpadea.
- [ ] Durante temporización solo cambia Ta.

## EDITAR IN

- [ ] SOURCE y LOGIC cambian correctamente.
- [ ] ESC cancela sin modificar el programa.
- [ ] OK aplica y retorna a DETALLE.
- [ ] El retorno conserva la entrada seleccionada.

## EDITAR T

- [ ] SEG/CENT/BASE funcionan.
- [ ] UP y DOWN cambian BASE en direcciones opuestas.
- [ ] Solo cambia el valor del campo editado.
- [ ] Ta y el pie permanecen estables.
- [ ] ESC cancela.
- [ ] OK aplica y conserva la base.

## NUEVO BLOQUE

- [ ] El nodo + abre y cierra correctamente.
- [ ] DI se crea.
- [ ] NOT se crea.
- [ ] AND2 se crea.
- [ ] TON se crea y conserva base.
- [ ] DO se crea.
- [ ] ESC retorna un nivel en cada pantalla.

## Restricciones

- [ ] No escribe FRAM.
- [ ] No conmuta salidas físicas.
- [ ] No cambia el formato de 12 bytes.

## Resultado

```text
Compilación:
Botonera:
Cabecera:
Transiciones:
DETALLE:
EDITAR IN:
EDITAR T:
NUEVO BLOQUE:
Observaciones:
Decisión: APROBADA / REQUIERE CORRECCIÓN
```
