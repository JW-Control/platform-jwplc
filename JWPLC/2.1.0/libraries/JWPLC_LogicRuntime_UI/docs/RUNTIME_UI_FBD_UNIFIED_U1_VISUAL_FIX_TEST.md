# Prueba física — Unified U1, estabilización visual MAPA/DETALLE

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

## Resultados recibidos antes del fix

- [x] IDLE -> MAPA operativo.
- [x] Navegación MAPA operativa.
- [x] MAPA -> DETALLE sin diagonal negra.
- [x] RUN permanece estable entre vistas.
- [x] DETALLE TON muestra T/Ta correctamente.
- [x] DETALLE AND/SR recorre entradas correctamente.
- [x] DETALLE -> MAPA operativo.
- [x] Botonera responde con Trg apagado.

Pendientes observados:

- [ ] RUN debe usar fondo gris, no negro.
- [ ] La navegación no debe hacer desaparecer momentáneamente bloques apagados.
- [ ] `Bxx TIPO` no debe borrarse al cambiar MAPA <-> DETALLE si continúa igual.
- [ ] La selección TON debe encerrar solo T; Ta es lectura.

## Cambios implementados

### Estado RUN

Unified ya no usa la insignia genérica rellena. Presenta:

```text
fondo gris COLOR_PANEL
borde verde COLOR_OK
texto RUN verde
```

La región solo se actualiza cuando cambia el estado o su color.

### Selección del mapa

Al mover la selección sin cambiar viewport:

```text
no se limpia el bloque anterior
no se limpia el bloque nuevo
solo se actualizan sus marcos
```

El redibujado completo se conserva únicamente cuando cambia la ventana horizontal o vertical.

### Cabecera Bxx TIPO

Las filas centrales tienen caché independiente:

```text
fila 1 = Bxx TIPO
fila 2 = ON/OFF, Trg, PARAM T, INx/n
```

Al pasar MAPA <-> DETALLE con el mismo bloque, solo cambia la fila 2.

### Selección TON

El marco amarillo encierra exclusivamente:

```text
T 02:00s
```

`Ta` queda fuera porque es un valor de lectura no editable.

## Prueba

- [ ] RUN aparece verde sobre fondo gris.
- [ ] RUN no parpadea MAPA <-> DETALLE.
- [ ] Mover selección entre bloques apagados no borra sus textos.
- [ ] Solo cambian el marco anterior y el nuevo.
- [ ] Un cambio real de viewport reconstruye el mapa una sola vez.
- [ ] MAPA -> DETALLE conserva `Bxx TIPO` sin microparpadeo.
- [ ] DETALLE -> MAPA conserva `Bxx TIPO` sin microparpadeo.
- [ ] Cambiar Trg -> PARAM T actualiza solo la fila 2.
- [ ] El marco amarillo encierra únicamente T.
- [ ] Ta permanece fuera del marco y sigue actualizándose normalmente.

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
