# Prueba física — renderer activo, cabecera unificada y campos TON

## Estado

```text
CANDIDATA / PENDIENTE DE REVALIDAR DOS TRANSICIONES
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

Los renderers históricos de MAPA (`V4`), DETALLE (`V5`) y el encabezado compacto (`V14`) quedan anulados en la ruta activa.

## Resultados físicos recibidos

- [x] Botones responden con Trg inactivo.
- [x] MAPA muestra contexto de dos filas sin traslape.
- [x] DETALLE muestra contexto de dos filas en posición estable.
- [x] EDITAR T muestra contexto de dos filas en posición estable.
- [x] DETALLE -> MAPA muestra un solo cambio visible.
- [x] DETALLE -> MAPA se percibe especialmente limpio.
- [x] DETALLE -> EDITAR T presenta directamente los tres campos.
- [x] EDITAR T ya no muestra el layout histórico `VALOR/UNIDAD`.
- [x] SEG cambia únicamente su valor.
- [x] CENT cambia únicamente su valor.
- [x] BASE conserva el comportamiento esperado.
- [x] Ta no parpadea al modificar SEG/CENT/BASE con TON detenido.
- [x] El pie `OK GUARDAR / ESC CANCELAR` permanece estable.

Pendientes observados antes del último fix:

- [ ] EDITAR T -> DETALLE mostraba durante algunos frames `B07 TON IN1/1` en una sola fila.
- [ ] MAPA -> DETALLE mostraba un barrido negro completo muy visible.

## Causa y corrección del último incremento

La cabecera residual no provenía de V5. `RuntimeUIFBDMapV4::drawDetail()` todavía escribía directamente:

```text
Bxx TIPO IN1/1
```

Ahora esa escritura pasa por el hook virtual `drawMapHeaderInfo()`, anulado por el renderer activo. La cabecera unificada es el único escritor.

El barrido MAPA -> DETALLE provenía de:

```cpp
clearScreen(tft);
```

dentro de `RuntimeUIFBDMapV4::drawDetailStatic()`. Se eliminó. MAPA y DETALLE comparten el panel exterior; ahora solo se limpia el interior mediante `clearMapArea()` y se dibuja inmediatamente el contenido final, igual que en la transición DETALLE -> MAPA.

## Revalidación necesaria

### EDITAR T -> DETALLE

- [ ] No aparece `Bxx TIPO IN1/1` en ningún frame.
- [ ] La primera cabecera visible ya es de dos filas.
- [ ] No existe desplazamiento horizontal antes de estabilizarse.
- [ ] `PARAM T` se conserva al volver porque era el foco de origen.

### MAPA -> DETALLE

- [ ] No aparece un barrido negro completo.
- [ ] El marco exterior permanece estable.
- [ ] Solo se sustituye el interior compartido del panel.
- [ ] El efecto se percibe comparable a DETALLE -> MAPA.
- [ ] No quedan nodos o cables del MAPA debajo del DETALLE.

## Regresión general

- [ ] IDLE -> MAPA mantiene un único cambio normal.
- [ ] DETALLE -> MAPA conserva la transición limpia aprobada.
- [ ] Los botones siguen respondiendo con Trg inactivo.
- [ ] T y Ta permanecen sin parpadeo en DETALLE.
- [ ] Ta y el pie permanecen sin parpadeo en EDITAR T.
- [ ] El nodo `+` mantiene su transición regional.

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
EDITAR T -> DETALLE:      APROBADA / FALLA
MAPA -> DETALLE:          APROBADA / FALLA
Regresión botonera:       APROBADA / FALLA
Regresión T/Ta:           APROBADA / FALLA
Decisión:                 CONSOLIDAR / REQUIERE CORRECCIÓN
```
