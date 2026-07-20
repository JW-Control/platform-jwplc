# Prueba física — renderer activo, cabecera unificada y campos TON

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

## Qué significan V5, V7, V13 y V14

No son versiones del motor lógico. Son revisiones históricas incrementales de la UI FBD:

| Capa | Aporte original |
|---|---|
| V5 | navegación Entrada/Parámetros y primer editor TON `VALOR/UNIDAD` |
| V7 | refresco regional, cachés y repeat acelerado |
| V13 | editor TON con base tipo LOGO! `ss:cc / mm:ss / hh:mm` |
| V14 | estabilización visual, formato de Ta y cachés anti-parpadeo |

El motor v2 permanece separado en `JWPLC_LogicRuntime_V2.h`. Estas capas pertenecen únicamente a `JWPLC_LogicRuntime_UI`.

La fachada pública no debe instanciar ninguna Vx. Usa:

```text
RuntimeUIFBDMap
  -> RuntimeUIFBDMapActiveRenderer
```

No se añadirá una V15. Después de validar este renderer, la deuda de herencia deberá aplanarse antes de ampliar sustancialmente el editor.

## Propiedad única de cabecera

MAPA, DETALLE y EDITAR T usan las mismas coordenadas:

```text
Título:       x=0..107
Contexto:     x=108..232
Estado RUN:   x=239..313
Fila 1:       y=3
Fila 2:       y=12
```

Formatos esperados:

```text
MAPA FBD     B07 TON          RUN
             ON

DETALLE      B07 TON          RUN
             Trg

DETALLE      B07 TON          RUN
             PARAM T

EDITAR T     B07 TON          RUN
             PARAM T
```

Los renderers históricos de MAPA (`V4`) y del encabezado compacto de DETALLE (`V14`) quedan anulados en la ruta activa.

## Pruebas recibidas antes de esta revisión

- [x] Botones responden con Trg inactivo.
- [x] DETALLE -> MAPA muestra un solo cambio visible.
- [x] DETALLE -> MAPA se percibe especialmente limpio.
- [x] EDITAR T ya no muestra el layout histórico `VALOR/UNIDAD`.
- [x] El encabezado de EDITAR T conserva conceptualmente dos filas.

## Cabecera unificada

- [ ] MAPA no muestra restos de `Trg` o `PARAM T`.
- [ ] MAPA no traslapa el título `MAPA FBD`.
- [ ] MAPA muestra `Bxx TIPO` en fila 1 y `ON/OFF` en fila 2.
- [ ] DETALLE muestra `Bxx TIPO` exactamente en las mismas coordenadas.
- [ ] DETALLE muestra entrada o parámetro en fila 2.
- [ ] EDITAR T usa exactamente las mismas coordenadas de contexto.
- [ ] Al salir de EDITAR T el texto no se desplaza a la izquierda.
- [ ] No aparece una posición provisional durante el retorno.
- [ ] RUN/READY/FAULT no se traslapa con el contexto.

## Campos de EDITAR T

Con TON detenido:

- [ ] Mantener UP/DOWN en SEG cambia únicamente `<nn>`.
- [ ] `SEG` permanece inmóvil.
- [ ] El valor de `Ta` permanece inmóvil y no parpadea.
- [ ] `Ta LECTURA` permanece inmóvil.
- [ ] `OK GUARDAR   ESC CANCELAR` permanece inmóvil.

Repetir en CENT:

- [ ] Solo cambia `<nn>`.
- [ ] No parpadean Ta ni el pie.

Cambiar BASE:

- [ ] Los rótulos cambian una sola vez porque cambia su significado.
- [ ] El valor de Ta cambia una sola vez a la nueva base.
- [ ] El pie no parpadea.

Con TON temporizando:

- [ ] Solo cambia el valor visible de Ta cuando corresponde.
- [ ] Los campos y el pie permanecen estables.

## Transiciones

- [ ] IDLE -> MAPA mantiene un único cambio normal.
- [ ] MAPA -> DETALLE no muestra cabecera provisional.
- [ ] DETALLE -> EDITAR T presenta directamente los tres campos.
- [ ] EDITAR T -> DETALLE no desplaza el contexto.
- [ ] DETALLE -> MAPA conserva la transición limpia ya aprobada.

## Contrato del motor

Este incremento no modifica:

```text
LogicV2EnginePrototype
LogicV2BlockRecord (12 bytes)
LogicV2InputLink (2 bytes)
parameter
resource
scan TON
sesión RAM
```

Los cambios pertenecen exclusivamente a la propiedad del render y navegación de `JWPLC_LogicRuntime_UI`.

## Criterio de cierre

```text
Compilación:              APROBADA / FALLA
Cabecera MAPA:            APROBADA / FALLA
Cabecera DETALLE:         APROBADA / FALLA
Cabecera EDITAR T:        APROBADA / FALLA
Campos sin parpadeo:      APROBADOS / FALLA
Ta sin parpadeo:          APROBADA / FALLA
Pie sin parpadeo:         APROBADO / FALLA
Transiciones:             APROBADAS / FALLA
Decisión:                 CONSOLIDAR / REQUIERE CORRECCIÓN
```
