# Alpha15 · A15-7 — Rediseño de inspectores: cerrar BAR y migrar TEXT / VALUE / BOOL

Fecha: `2026-10-16`

```text
A15_7A_BAR_FINISH=PASS (ver A15_7_INSPECTOR_CLOSE_20261017.md)
A15_7B_VALUE=PASS
A15_7C_TEXT=PASS
A15_7D_BOOL=PASS
PRIORIDAD=ALTA (pedido explícito del jefe; va justo después de A15-1..A15-3)
```

Referencia visual: `tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/`.
Evidencia: capturas del Designer en Chromium headless sobre `cc66d69` `[ejecutado]`.

## 1. Estructura común (los cuatro tipos)

```text
Inspector — <NOMBRE>                               [duplicar] [eliminar]
[ Diseño ] [ Datos ] [ Técnico ]
  Diseño: [ Forma ] [ Contenido ] [ Apariencia ] [ Ubicación ]
```

| Pestaña | Contiene | No contiene |
|---|---|---|
| Diseño · Forma | layout, marco, tamaños de texto, variante | valores de prueba |
| Diseño · Contenido | etiqueta, unidad, textos, formato | colores |
| Diseño · Apariencia | colores (paleta RGB565 + muestra) | geometría |
| Diseño · Ubicación | página, X/Y, tamaño calculado | — |
| Datos | variable vinculada (C++ o PLC según modo), tipo, **valor de prueba / simulación** | estilo |
| Técnico | nombre, ID C++, contrato copiable, diagnóstico (reserva, región de valor, padding, gap) | controles editables duplicados |

Reglas:

1. Opción enumerable (≤ 6 valores) → **tarjeta visual** con mini-preview. Nada de `<select>`.
2. Números acotados (tamaño 1×-3×, decimales 0-3) → **chips** o stepper con preview.
3. Colores → muestra + paleta de presets RGB565 (`bar-palette-pill`); el hex queda como dato secundario editable.
4. Valor de prueba y simulación **sólo** en "Datos".
5. Cada control debe mapear a un parámetro que el codegen emite. Si no, no se muestra
   (o se muestra deshabilitado con la etiqueta "Requiere firmware").
6. Pestaña "Datos" según modo: Arduino = campo "Variable C++"; OpenPLC = selector
   "Variable PLC" filtrado por tipo (ver `ALPHA15_PLAN.md` D1/D2).
7. Mismos componentes CSS que BAR (`bar-card`, `bar-visual-btn`, `bar-sub-tab`…), renombrados
   a prefijo común (`insp-*`) para no depender de BAR.

## 2. Qué soporta el firmware (`JWPLC_UI.h`)

```text
Layout          INLINE | STACKED
Alineación val. LEFT | CENTER | RIGHT
Tamaños         labelTextSize, valueTextSize (escala GLCD)
Marco           bool
Colores         label, value, background, frame   (UN color de valor, no por estado)
Formato num.    integerDigits, decimalDigits, signedValue, leadingZeros
BOOL            falseText, trueText
BAR             rect (x, y, ancho, alto), range min/max, labelTextSize, frame, layout
TEXT            capacity
```

## 3. BAR — pendientes para cerrarlo

Estado observado: pestañas y sub-pestañas ya existen; Forma usa tarjetas.

| # | Hallazgo | Acción |
|---|---|---|
| BAR-1 | Slider "Simulación / Valor de prueba" aparece en **Forma** y también en Datos | Dejarlo sólo en Datos |
| BAR-2 | Quedan `<select>`: "Modo de ancho", "Tipo de relleno" (Forma), "Mostrar % de carga", "Tamaño etiqueta" (Contenido), "Página" (Ubicación) | Tarjetas / chips; página como pestañas mini |
| BAR-3 | Opciones sin soporte en firmware (sólo comentario C++ o nada): Vertical, Diagonales/Bloques/Puntos, Leve/Píldora, borde de barra, Degradado, Circular, alineación 3×3 de etiqueta (`alignH/alignV`), "% de carga" | Deshabilitadas con "Requiere firmware" hasta que Alpha13 las implemente |
| BAR-4 | "Grosor vertical (px)" no llega al firmware: el codegen emite `JWPLC_UIRect(x, y, ancho, JWPLC_UI_AUTO)` `[ejecutado]` | Emitir el alto, o quitar el control |
| BAR-5 | Pestaña "Datos PLC" y "Tipo C++" editable en modo Arduino | "Datos"; tipo como chip de sólo lectura (`float`) |
| BAR-6 | Contrato C++ truncado en Técnico (`float nivel2 = 0.0f; // Vía JWPLC_Displa…`) | Bloque multilínea con scroll |
| BAR-7 | Checklist de la spec (`…Especificacion_UI.md:429-445`) sin marcar | Recorrerla y marcar en este doc |

Variantes visibles tras el cierre (firmware actual): **Barra horizontal, sólida, rectangular,
con/sin marco, layout INLINE/STACKED**. El resto aparece como tarjeta deshabilitada.

## 4. VALUE — distribución propuesta

```text
Forma
  Layout:        [▭ Etiqueta 25.6 C ]  [▭ Etiqueta / 25.6 C ]      (INLINE | STACKED)
  Tamaño valor:  [1×] [2×] [3×]   (chip con alto en px: 7 / 14 / 21)
  Tamaño etiq.:  [1×] [2×]
  Marco:         [sin marco] [con marco]
Contenido
  Etiqueta [______]   Unidad [___]
  Formato:  enteros [-] 3 [+]   decimales [0][1][2][3]
            [± con signo]  [007 ceros a la izq.]
            Vista: reserva "888.8"  ->  resultado "25.6"
  Alineación del valor: [25.6··] [·25.6·] [··25.6]
Apariencia
  Etiqueta ● Valor ● Fondo ● Marco ●  (paleta común)
  Mini-preview del campo con los 4 colores
Ubicación
  Página (pestañas mini) · X / Y · tamaño calculado (sólo lectura)
Datos
  Variable C++ | Variable PLC (REAL/INT/DINT/UINT…)  · tipo: float
  Valor de prueba [25.6] + slider dentro de la reserva (−888.8 … 888.8)
Técnico
  Nombre · ID · contrato · reserva geométrica · región de valor · padding/gap
```

## 5. TEXT — distribución propuesta

```text
Forma       Layout INLINE|STACKED · Tamaño valor/etiqueta · Marco          (igual que VALUE)
Contenido   Etiqueta · Capacidad [-] 12 [+] con regla "MMMMMMMMMMMM" mostrando el ancho reservado
            Alineación del texto [izq] [centro] [der]
Apariencia  igual que VALUE
Ubicación   igual que VALUE
Datos       Variable C++ (char[N+1]) | Variable PLC (STRING, se trunca a capacidad con aviso)
            Valor de prueba [READY] con contador "5/12"
Técnico     igual que VALUE
```

## 6. BOOL — distribución propuesta

```text
Forma       Layout INLINE|STACKED · Tamaño valor/etiqueta · Marco
Contenido   Etiqueta
            Estados: [ OFF ] -> [ ON ]   (dos tarjetas editables lado a lado, con preview)
            Presets rápidos: OFF/ON · PARADO/MARCHA · CERRADO/ABIERTO · NO/SI
            Alineación [izq] [centro] [der]
Apariencia  Etiqueta ● Valor ● Fondo ● Marco ●
            (Color por estado ON/OFF: tarjeta deshabilitada "Requiere firmware":
             JWPLC_UIColors tiene un único color de valor)
Ubicación   igual que VALUE
Datos       Variable C++ (bool) | Variable PLC (BOOL)
            Estado de prueba: interruptor [OFF|ON] (no <select>)
Técnico     igual que VALUE
```

Quitar el campo **"Unidad"** de BOOL si el firmware no lo dibuja (verificar en A15-9 contra
la TFT; el codegen hoy lo pasa en `JWPLC_UIText(label, unit)`).

## 7. Peticiones al firmware (Alpha13) que salen de este rediseño

| Petición | Desbloquea |
|---|---|
| Color de valor por estado en BOOL | lámpara verde/roja |
| BAR vertical, alto explícito, radio de esquina, estilos | BAR-3/BAR-4 |
| BAR circular y degradado (`JWPLC_UICircularField`, `JWPLC_UIGradientBarField`) | B4 |
| Posición de etiqueta/porcentaje en BAR | alineación 3×3, "% de carga" |

## 7b. NAV_BAR (inspector de André, fusionado 2026-10-17)

El inspector ya sigue el patrón (Diseño / Páginas / Técnico, tarjetas visuales). Pendiente de
paridad con la TFT:

| # | Hallazgo `[visto]` | Acción |
|---|---|---|
| NAV-1 | `fontSize` micro (4x6) / mediana (7x9): el preview usa una microfuente propia; el codegen siempre emite GLCD 5x7 (`setTextSize(1)`) | Emitir glifos micro como bitmap o limitar a las escalas GLCD reales (5x7, 10x14) |
| NAV-2 | `textAlign` izquierda/derecha y `labelFormat` (`N°`, `N° + Nom`) sólo se aplican en el preview | Trasladarlos al codegen (`navBarFunctionBlock`) |
| NAV-3 | Presets y posiciones verticales: verificar en la TFT (A15-9) | Gate físico |

## 8. Criterio de cierre por tipo

```text
- Ningún <select> para opciones enumerables.
- Valor de prueba sólo en "Datos".
- Toda opción visible cambia el header generado; toda opción del header es editable.
- Proyectos v1 abren sin pérdida (mismas propiedades).
- Undo/redo, duplicar, guardar/abrir y generar C++ siguen funcionando.
- Usable a 1080p sin scroll horizontal en el panel derecho.
- Captura del lienzo == foto de la TFT (A15-9) para la variante por defecto.
```
