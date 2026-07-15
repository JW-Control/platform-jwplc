# Guía visual para pantallas USER del Logic Runtime

## Identidad

Todas las pantallas de `JWPLC_LogicRuntime_UI` deben seguir la identidad visual de JW Control:

```text
verde
blanco
negro
```

No se usarán tonos azules o cian como color principal de selección, encabezado o acento.

## Paleta base RGB565

Definida en:

```text
src/widgets/RuntimeUIWidgets.h
```

```cpp
COLOR_BACKGROUND = ST77XX_BLACK;
COLOR_TEXT       = ST77XX_WHITE;
COLOR_ACCENT     = 0x5FE0;  // verde de acento JW Control
COLOR_OK         = 0x07E0;  // verde de estado activo
COLOR_SELECTED   = 0x0200;  // verde oscuro de selección
COLOR_PANEL      = 0x1082;  // panel casi negro
COLOR_BORDER     = 0x7BEF;  // borde gris claro
COLOR_MUTED      = 0x9CF3;  // texto secundario
COLOR_WARNING    = 0xFFE0;  // amarillo
COLOR_ERROR      = ST77XX_RED;
```

## Uso semántico

### Verde de acento

Usar para:

- títulos de panel;
- borde del elemento seleccionado;
- estado `READY`;
- elementos interactivos normales;
- confirmaciones positivas no críticas.

### Verde activo

Usar para:

- estado `RUNNING`;
- operación completada correctamente;
- periférico activo;
- valor booleano verdadero cuando corresponda.

### Blanco

Usar para:

- texto principal;
- títulos principales;
- valores importantes;
- bordes neutrales.

### Negro y panel oscuro

Usar para:

- fondo general;
- fondo de paneles;
- fondo de campos;
- contraste con blanco y verde.

### Amarillo

Reservado para:

- `STOPPED`;
- advertencias;
- estados que requieren atención pero no son falla.

### Rojo

Reservado para:

- `FAULT`;
- errores críticos;
- metadata corrupta;
- acciones destructivas antes de confirmar.

## Selección

Un elemento seleccionado debe usar conjuntamente:

```text
fondo verde oscuro
+ borde verde de acento
+ texto verde o blanco de alto contraste
```

No se debe cambiar toda la pantalla de color al navegar. Solo se redibujan el elemento anterior y el actual.

## Consistencia

Las siguientes vistas deben reutilizar los widgets y colores centrales:

```text
RuntimeUIHome
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

No se deben declarar colores locales equivalentes salvo que representen un dato específico que no exista en la paleta común.

## Criterios de revisión

```text
[ ] No usa azul/cian como acento principal.
[ ] Selección visible con verde oscuro y borde verde.
[ ] READY y RUNNING se distinguen sin romper la identidad.
[ ] Amarillo solo indica advertencia o STOPPED.
[ ] Rojo solo indica error o peligro.
[ ] Texto blanco mantiene contraste sobre fondos oscuros.
[ ] Reutiliza RuntimeUIWidgets.h.
```

## Estado

```text
GUÍA APROBADA
APLICADA DESDE JWPLC_LogicRuntime_UI 0.2.0
```