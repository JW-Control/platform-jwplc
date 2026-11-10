# Especificación de Diseño: Selector Visual de Tipografía y Escala Unificado (Opción A)

**Fecha:** 2026-11-10  
**Línea / Versión:** v2.1.0-alpha.17 (`tools/jwplc-hmi-designer`)  
**Estado:** Propuesta de Diseño aprobada por el usuario (Opción A)

---

## 1. Intención y Justificación

### Problema actual
En el inspector del HMI Designer (`Diseño > Forma`), la configuración tipográfica de los campos con texto (`VALUE`, `BOOL`, `TEXT`, etc.) está fragmentada en dos tarjetas separadas y redundantes:
1. **Tarjeta "Tamaño":** Muestra chips `[1×] [2×] [3×] [4×]` para la etiqueta y el valor. Sin embargo, este multiplicador solo es válido para la fuente matricial `GLCD 5×7`.
2. **Tarjeta "Tipografía":** Muestra controles `<select>` nativos de HTML con una lista plana de fuentes (`Bitmap 16 px`, `Bitmap 26 px`, `7-Segmentos 48 px`, `Sans 9pt`, etc.).

Esta duplicidad genera dos problemas críticos:
- **Confusión y redundancia:** Cuando el usuario selecciona una fuente con altura inherente fija (como `Bitmap 26 px` o `7-Segmentos 48 px`), los chips `1×..4×` siguen visibles pero no aplican en el firmware de la TFT (o alteran márgenes sin escalar la letra).
- **Estética pobre:** Los `<select>` estándar de HTML desentonan con el lenguaje visual de diseño del HMI Designer (basado en tarjetas, chips activos y tokens CSS semánticos).

### Solución acordada (Opción A)
Fusionar "Tamaño" y "Tipografía" en una **única tarjeta visual unificada** ("Tipografía"). Se eliminan los `<select>` nativos y se reemplazan por selectores visuales de dos niveles:
1. **Nivel 1 (Familia / Estilo):** Chips visuales horizontales (`Clásica 5×7`, `Bitmap`, `Sans`, `7-Seg`).
2. **Nivel 2 (Tamaños contextuales):** Fila de chips que muestra **únicamente los tamaños reales válidos** para la familia activa, eliminando cualquier opción incompatible o redundante.

---

## 2. Objetivos y No-Objetivos

### Objetivos
- Unificar tamaño y fuente en un solo bloque limpio, intuitivo y estético dentro de la pestaña `Forma`.
- Eliminar los elementos `<select>` nativos en favor de chips visuales táctiles con clase `.active`.
- Contextualizar los tamaños:
  - `Clásica 5×7`: Chips de escala `[ 1× (8px) ] [ 2× (16px) ] [ 3× (24px) ] [ 4× (32px) ]`.
  - `Bitmap`: Chips `[ 16 px ] [ 26 px ]`.
  - `7-Segmentos`: Chip fijo `[ 48 px ]` (exclusivo para valores numéricos en `VALUE`).
  - `Sans` / `Mono`: Chips `[ 9 pt (18px) ] [ 12 pt (24px) ] [ 18 pt (34px) ]` con toggle de negrita `[B]`.
- Indicador en vivo de altura real a la derecha del título de cada sección (ej. `8 px (GLCD 1×)` o `26 px (Bitmap)`).
- Aplicar de forma homogénea a todos los componentes con texto: `VALUE`, `TEXT`, `BOOL`, `BAR` y `MENU`.
- Mantener 100% de compatibilidad con el modelo de datos `.jwhmi`, el codegen C++ y el historial Undo/Redo.

### No-Objetivos
- No se alteran los archivos generados de fuentes en firmware (`JWPLC_TFTFont*.cpp`) ni las constantes C++ (`JWPLC_FONT_*`).
- No se agregan nuevas fuentes binarias en esta tarea; se reorganiza y potencia el catálogo existente de `designer-fonts.js`.

---

## 3. Enfoques Considerados y Selección

| Enfoque | Descripción | Ventajas | Desventajas | Decisión |
|---|---|---|---|---|
| **Opción A (Elegida)** | Bloque visual por Familia + Chips de altura dinámicos en un clic | Cero menús flotantes, todo visible de inmediato, muy rápido para uso industrial en taller | Ocupa algo más de altura en el inspector | **SELECCIONADA** por el usuario |
| **Opción B** | Cápsula visual compacta con popover/modal flotante de previsualización | Máximo ahorro de espacio vertical | Requiere clic adicional para abrir el menú flotante | Descartada |
| **Opción C** | Mantener `<select>` pero ocultar dinámicamente los chips `1×..4×` | Menor esfuerzo de código | Sigue viéndose poco atractiva con selects nativos | Descartada |

---

## 4. Diseño de la Interfaz (UI/UX)

### Mockup Visual del Inspector (`Diseño > Forma`)

```text
┌─ Tipografía ────────────────────────────────────────────────────────┐
│ Etiqueta                                           8 px (GLCD 1×)  │
│ [ Clásica 5×7 ● ]  [ Bitmap ]  [ Sans ]  [ Mono ]                  │
│   Tamaño: [ 1× (8px) ● ]  [ 2× (16px) ]  [ 3× (24px) ]  [ 4× (32px) ]│
│                                                                     │
│ Valor / Estado                                   26 px (Bitmap)    │
│ [ Clásica 5×7 ]  [ Bitmap ● ]  [ Sans ]  [ 7-Seg ]                  │
│   Tamaño: [ 16 px ]  [ 26 px ● ]                                    │
└─────────────────────────────────────────────────────────────────────┘
```

### Reglas de Visualización y Comportamiento:
1. **Etiqueta (`labelFont` + `labelSize`):**
   - Familias: `GLCD` (Clásica 5×7), `BITMAP` (Bitmap), `SANS` (Sans), `MONO` (Mono).
   - Oculta `7-Segmentos` (las etiquetas son texto alfabético).
   - Al tocar una familia:
     - Si `GLCD`: activa `labelFont = 'GLCD'` y conserva `labelSize` (1..4).
     - Si `BITMAP`: activa `labelFont = 'FONT_16'` (por omisión) o `FONT_26`.
     - Si `SANS`: activa `labelFont = 'SANS_9'` (por omisión) u otras escalas pt.
2. **Valor / Estado (`valueFont` + `valueSize`):**
   - En `VALUE`: incluye la familia `7-Seg` (`FONT_7SEG_48`).
   - En `TEXT` y `BOOL`: muestra `Clásica 5×7`, `Bitmap`, `Sans`, `Mono`.
3. **Indicador de altura real:**
   - Un `<span>` con clase `.insp-hint` situado en la cabecera que calcula la altura exacta en px según la fuente y escala activas (ej. `8 px`, `16 px`, `24 px`, `26 px`, `48 px`).
4. **Persistencia y Undo/Redo:**
   - Toda interacción dispara `setFieldProps()` registrando paso en el historial y sincronizando inmediatamente el lienzo canvas 2D y la pestaña de código generado.

---

## 5. Componentes Afectados

1. `tools/jwplc-hmi-designer/poc/designer-field-inspector.js`:
   - Reemplazo de las tarjetas de tamaño y selects por el nuevo componente tipográfico para `VALUE`, `TEXT` y `BOOL`.
2. `tools/jwplc-hmi-designer/poc/designer-bar.js`:
   - Adaptación de la tipografía para la etiqueta y unidad de la barra/medidor circular.
3. `tools/jwplc-hmi-designer/poc/designer-menu.js`:
   - Selector por chips para la fuente del menú (`GLCD`, `Bitmap 16`, `Bitmap 26`, `Sans`).
4. `tools/jwplc-hmi-designer/poc/styles.css`:
   - Clases CSS para la botonera de familias tipográficas y chips de tamaño adaptados a tokens de diseño claro/oscuro.

---

## 6. Verificación

1. **Pruebas visuales:**
   - Crear un campo `VALUE`: cambiar a `7-Segmentos 48 px`, verificar que se ocultan escalas 1x..4x y se dibuja el display digital.
   - Cambiar a `Bitmap 26 px`: verificar que la altura indica 26 px y la etiqueta se mantiene en GLCD 1x u otra fuente elegida.
   - Probar en modo oscuro y en modo claro (`theme-light`).
2. **Pruebas de codegen y esquema:**
   - Ejecutar `node tests/codegen-core.test.cjs`.
   - Verificar persistencia al guardar y reabrir proyectos `.jwhmi`.
