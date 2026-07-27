# Prueba física — Unified U1, estabilización visual MAPA/DETALLE

## Estado

```text
APROBADA FÍSICAMENTE — 2026-07-20
```

## Resultados físicos aprobados

- [x] IDLE -> MAPA operativo.
- [x] Navegación MAPA operativa.
- [x] MAPA -> DETALLE sin diagonal negra.
- [x] DETALLE -> MAPA operativo.
- [x] RUN permanece estable entre vistas.
- [x] DETALLE TON muestra T/Ta correctamente.
- [x] DETALLE AND/SR recorre entradas correctamente.
- [x] Botonera responde con Trg apagado.
- [x] Mover selección entre bloques apagados no borra sus textos.
- [x] `Bxx TIPO` se conserva MAPA <-> DETALLE sin microparpadeo apreciable.
- [x] Cambiar `Trg -> PARAM T` actualiza la segunda fila de cabecera.
- [x] El marco amarillo nace directamente alrededor de T.
- [x] Nunca aparece el marco T + Ta, ni siquiera durante el cambio de estado.
- [x] Activar el TON no altera la geometría del marco.
- [x] Completar/apagar el TON no altera la geometría del marco.
- [x] Ta permanece fuera del marco y actualiza únicamente su valor visible.

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

Se conserva como observación estética no bloqueante para una revisión posterior.

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

## Selección TON

La geometría nativa es:

```text
TON_PANEL_H = 14
```

Por tanto, `drawTonPanelFrame()` solo puede dibujar desde el origen el marco de la fila T:

```text
┌──────────────┐
│ T 02:00s     │
└──────────────┘
  Ta 00:00s
```

No existe una segunda pasada correctiva sobre el marco.

## Motor

U1 no modifica:

```text
LogicV2EnginePrototype
LogicV2BlockRecord
LogicV2InputLink
scan TON
parameter
resource
sesión RAM
```

## Decisión

```text
U1 MAPA + DETALLE: CERRADA / APROBADA
Siguiente fase: U2 EDITAR IN + U3 EDITAR T
```
