# Prueba física — Unified U1, estabilización visual MAPA/DETALLE

## Estado

```text
CANDIDATA / PENDIENTE DE VALIDAR EL MARCO T-ONLY EN HARDWARE
```

## Resultados físicos acumulados

- [x] IDLE -> MAPA operativo.
- [x] Navegación MAPA operativa.
- [x] MAPA -> DETALLE sin diagonal negra.
- [x] DETALLE -> MAPA operativo.
- [x] RUN permanece estable entre vistas.
- [x] DETALLE TON muestra T/Ta correctamente.
- [x] DETALLE AND/SR recorre entradas correctamente.
- [x] Botonera responde con Trg apagado.
- [x] Mover selección entre bloques apagados ya no borra sus textos.
- [x] `Bxx TIPO` se conserva MAPA <-> DETALLE sin microparpadeo apreciable.

## Estado RUN

Unified presenta:

```text
fondo gris COLOR_PANEL
borde según estado
texto según estado
```

Resultado recibido:

```text
Funcional: sí
Percepción visual: "sí, pero raro"
```

Se conserva como observación estética para una revisión posterior; no bloquea U1 mientras el estado sea legible y estable.

## Selección del mapa

Al mover la selección sin cambiar viewport:

```text
no se limpia el bloque anterior
no se limpia el bloque nuevo
solo se actualizan sus marcos
```

El redibujado completo se conserva únicamente cuando cambia la ventana horizontal o vertical.

## Cabecera Bxx TIPO

Las filas centrales tienen caché independiente:

```text
fila 1 = Bxx TIPO
fila 2 = ON/OFF, Trg, PARAM T, INx/n
```

Al pasar MAPA <-> DETALLE con el mismo bloque, solo cambia la fila 2.

Aclaración de prueba:

- `Trg -> PARAM T` se refiere a la segunda fila de la cabecera.
- La actualización de `Ta` durante la temporización es una prueba distinta.

## Regresión detectada en la selección TON

La primera corrección funcionaba en dos pasos:

```text
1. drawTonPanelFrame() dibujaba un marco grande T + Ta
2. jwplcNormalizeUnifiedTonFrame() lo borraba y dibujaba solo T
```

Resultado físico:

- [ ] Al entrar en PARAM T se veía momentáneamente el marco T + Ta.
- [ ] Al activar el TON, el marco completo reaparecía unos frames.
- [ ] Al apagar/completar el TON, ocurría lo mismo.

Esto confirmó que una corrección posterior no era aceptable.

## Fix estructural aplicado

La geometría nativa del panel cambió de:

```text
TON_PANEL_H = 31
```

a:

```text
TON_PANEL_H = 14
```

Por tanto, `drawTonPanelFrame()` solo puede dibujar desde el origen el marco de la fila T.

La función `jwplcNormalizeUnifiedTonFrame()` queda temporalmente como no-op para compatibilidad de enlace. Ya no borra ni redibuja píxeles.

Comportamiento esperado:

```text
┌──────────────┐
│ T 02:00s     │
└──────────────┘
  Ta 00:00s
```

No debe existir ningún frame intermedio T + Ta al:

- entrar a PARAM T;
- salir de PARAM T;
- iniciar temporización;
- completar temporización;
- apagar el TON.

## Prueba pendiente

- [ ] RUN continúa legible y estable.
- [x] Bloques apagados no parpadean al navegar.
- [x] `Bxx TIPO` permanece estable MAPA <-> DETALLE.
- [ ] Cambiar `Trg -> PARAM T` actualiza únicamente la segunda fila de cabecera.
- [ ] El marco amarillo nace directamente alrededor de T.
- [ ] Nunca aparece el marco T + Ta, ni siquiera por un frame.
- [ ] Activar el TON no altera la geometría del marco.
- [ ] Completar/apagar el TON no altera la geometría del marco.
- [ ] Ta permanece fuera del marco y actualiza únicamente su valor visible.

## Alcance de U1

En esta fase `OK` dentro de DETALLE todavía no abre editores:

```text
U2 -> EDITAR IN
U3 -> EDITAR T
```

Esto es deliberado y no constituye una falla del preview U1.

## Motor

Este fix no modifica:

```text
LogicV2EnginePrototype
LogicV2BlockRecord
LogicV2InputLink
scan TON
parameter
resource
sesión RAM
```
