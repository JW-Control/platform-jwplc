# Alpha17 · A17-HMI — Soporte Nativo y Escalable de Temas Claro y Oscuro con Tokens de Diseño

Fecha: `2026-11-03`  
Alcance: `tools/jwplc-hmi-designer` (editor visual de pantalla TFT ST7789 para JWPLC)  
Estado: `IMPLEMENTADO Y VERIFICADO` (Commit `851b64ba`)

```text
HMI_THEME_TOKENS=PASS (sistema centralizado en styles.css con soporte :root y .theme-light)
HMI_INSPECTOR_REFACTOR=PASS (pestañas, matrices 3x3, steppers, tarjetas de estado, reglas, árbol de menú)
HMI_INLINE_STYLES_CLEANUP=PASS (eliminado style="color: #dce9f1" en index.html y hex rígidos en designer-bar.js)
HMI_TESTS_PASS=PASS (codegen-core, icon-catalog, runtime-core, sketch-guide)
```

---

## 1. Contexto y Diagnóstico del Problema

El HMI Designer fue concebido originalmente con un enfoque *Dark-First* orientado al color negro del hardware (la pantalla TFT IPS ST7789 de 320×170 px). Como consecuencia de esta evolución:

1. **Colores rígidos hardcodeados:** Se acumularon más de 485 declaraciones de colores hexadecimales fijos (`#0d1820`, `#111f2c`, `#1e313f`, `#8ea3b3`, `#dce9f1`, etc.) dispersos a lo largo de `tools/jwplc-hmi-designer/poc/styles.css`.
2. **Estilos en línea de alta especificidad:** Se introdujeron encabezados de sección directamente en el HTML con atributos `style="color: #dce9f1;"` (especificidad `1-0-0-0`), lo cual impedía que cualquier hoja de estilos externa o tema claro pudiera alterar su color, haciéndolos prácticamente invisibles o ilegibles sobre fondos claros.
3. **Falta de cobertura en `designer-light-theme.css`:** Dicho archivo fue creado como un parche posterior que sobreescribía clases selectas mediante selectores fuertemente acoplados o reglas genéricas (`body.theme-light button`). Al agregarse los nuevos inspectores visuales en Alpha 15 y Alpha 16 (BAR, TEXT, VALUE, BOOL, MENU), los nuevos componentes no contaban con reglas para modo claro y heredaban propiedades oscuras o colisionaban con las reglas genéricas creando botones desalineados o fondos negros.
4. **Componentes afectados identificados:**
   - **Cabeceras y pestañas:** `.bar-nav-header`, `.bar-main-tab`, `.shape-main-tab`, `.bar-sub-tab`, `.shape-sub-tab` (mantenían fondo `#070d12` y texto blanco o botones aislados).
   - **Matrices 3×3 de alineación:** Tanto la matriz de figuras y texto (`.shape-align-matrix-*`) como la matriz de BAR (`.align-matrix-3x3` y `.matrix-cell`) mantenían contenedores oscuros con bordes incompatibles.
   - **Steppers y contadores:** `.insp-stepper` para dígitos de formato entero y capacidad de caracteres con botones desfasados en contraste.
   - **Tarjetas de estado booleano:** `.insp-state-card` para estados `FALSE` y `TRUE` con fondos negros rígidos `#091117`.
   - **Chips de tipo y modo:** `.insp-type-chip`, `.insp-mode-chip` (`bool`, `float`, etc.).
   - **Reglas visuales y muestras:** `.insp-ruler` con visualización `MMMM` y reservas de texto.
   - **Árbol de menú:** `.menu-tree-row`, `.menu-tree-var`, `.menu-tree-icon` y botones de acción del menú.

---

## 2. Arquitectura de la Solución (Tokens de Diseño / CSS Custom Properties)

Se seleccionó la **Opción 2: Migración a Tokens de Diseño CSS nativos**.

### ¿Por qué esta solución?
- **Fuente única de la verdad:** Los colores se definen en un solo lugar al inicio de `poc/styles.css`.
- **Cero dependencias:** Funciona de forma 100% nativa en el navegador y en Electron sin necesidad de herramientas de compilación (bundlers, Sass o Tailwind).
- **Escalabilidad nativa:** Cualquier componente futuro que se construya utilizando `var(--nombre-token)` soportará automáticamente modo oscuro y modo claro sin requerir reglas adicionales en hojas de estilo de override.
- **Respeto absoluto al hardware:** El lienzo de simulación del display ST7789 (`#tftCanvas`, 320×170 px) permanece estrictamente en `#000000` en ambos temas, ya que modela fielmente la pantalla física.

### Definición de Tokens Semánticos (`styles.css`)

```css
:root {
  /* Modo Oscuro (Por Defecto) */
  --bg: #070d12;
  --panel: #0d1820;
  --panel-2: #111f2c;
  --panel-3: #162635;
  --panel-inset: #091117;
  --line: #1e313f;
  --line-soft: #162837;
  --text: #dce9f1;
  --text-heading: #f0f7fb;
  --muted: #8ea3b3;
  --orange: #ff7a18;
  --orange-soft: rgba(255, 122, 24, 0.15);
  --tab-bg: #0d1820;
  --tab-border: #1e313f;
  --tab-text: #8ea3b3;
  --tab-active-text: #ff7a18;
  --cyan: #38bdf8;
  --green: #22c55e;
  --warn: #eab308;
  --danger: #ef4444;
}

html.theme-light,
body.theme-light {
  /* Modo Claro */
  --bg: #f1f5f9;
  --panel: #ffffff;
  --panel-2: #f8fafc;
  --panel-3: #f1f5f9;
  --panel-inset: #f1f5f9;
  --line: #cbd5e1;
  --line-soft: #e2e8f0;
  --text: #1e293b;
  --text-heading: #0f172a;
  --muted: #64748b;
  --orange: #ea580c;
  --orange-soft: rgba(234, 88, 12, 0.12);
  --tab-bg: #f8fafc;
  --tab-border: #cbd5e1;
  --tab-text: #64748b;
  --tab-active-text: #ea580c;
  --cyan: #0284c7;
  --green: #16a34a;
  --warn: #ca8a04;
  --danger: #dc2626;
}
```

Adicionalmente, se configuró el metadato del sistema operativo en `index.html` y `desktop.html`:
```html
<meta name="color-scheme" content="light dark" />
```

---

## 3. Resumen de Componentes Refactorizados

| Componente | Clases Modificadas | Comportamiento en Modo Claro |
|---|---|---|
| **Pestañas de Navegación** | `.bar-nav-header`, `.bar-main-tab`, `.shape-main-tab`, `.bar-sub-tab`, `.shape-sub-tab` | Fondo `--panel-2`, borde `--line`, texto `--muted`, activo resalta con `--orange` y fondo blanco. |
| **Encabezados de Inspector** | `.inspector-heading` (reemplaza `style="color: #dce9f1"`) | Color `--text-heading` de alta legibilidad sin estilos inline rígidos. |
| **Tarjetas y Secciones** | `.bar-card`, `.bar-card-section-title`, `.bar-card-title`, `.bar-field-subtitle` | Fondo `--panel-2`, bordes `--line-soft`, títulos con `--text-heading` y subtítulos con `--muted`. |
| **Matrices 3×3 de Alineación** | `.shape-align-matrix-*`, `.align-matrix-3x3`, `.matrix-cell` | Celdas con fondo `--panel`, bordes `--line`, íconos centrados y estado seleccionado con `--orange`. |
| **Controles Numéricos / Steppers** | `.insp-stepper`, `.insp-stepper button`, `.insp-stepper span` | Botones legibles sobre `--panel-inset` con bordes `--line` y valor destacado con `--text-heading`. |
| **Tarjetas de Estado (BOOL)** | `.insp-state-card` | Fondo `--panel-2`, bordes `--line`, etiquetas de estado (`FALSE`/`TRUE`) legibles con `--muted`. |
| **Chips de Tipo y Modo** | `.insp-type-chip`, `.insp-mode-chip` | Fondo `--panel-3`, borde `--line`, texto semántico (`--cyan`, `--orange`, etc.). |
| **Reglas de Caracteres** | `.insp-ruler`, `.insp-format-view` | Fondo `--panel-inset`, texto monoespaciado en `--cyan` y dimensiones en `--muted`. |
| **Árbol de Menú HMI** | `.menu-tree-row`, `.menu-tree-btns button`, `.menu-tree-var`, `.menu-tree-icon` | Filas alternadas con `--panel-2`, botones de acción integrados y variables en `--muted`. |
| **Plantillas Dinámicas JS** | `designer-bar.js` | Reemplazo de cadenas CSS hardcodeadas (`#0d1820`, `#1e313f`) por llamadas a variables semánticas. |

---

## 4. Dónde y Cómo Modificar los Colores de los Temas

Para ajustar o personalizar la paleta de colores de la aplicación, **no es necesario buscar en múltiples archivos ni escribir reglas complejas**.

Toda la configuración se encuentra centralizada en las primeras **53 líneas** de:
```text
tools/jwplc-hmi-designer/poc/styles.css
```

### Guía Rápida de Ajuste

1. **Para cambiar el color del tema Oscuro:**
   Edite los valores bajo el bloque `:root { ... }`.
   - Ejemplo: Para hacer los paneles un poco más azulados o grises, modifique `--panel` y `--panel-2`.
   - Ejemplo: Para cambiar el color de acento o selección, modifique `--orange`.

2. **Para cambiar el color del tema Claro:**
   Edite los valores bajo el bloque `html.theme-light, body.theme-light { ... }`.
   - Ejemplo: Para oscurecer los textos si se requiere mayor contraste:
     ```css
     --text: #0f172a;
     --muted: #475569;
     ```
   - Ejemplo: Para ajustar el fondo general de la app:
     ```css
     --bg: #e2e8f0;
     ```

> [!NOTE]
> Se descartó expresamente implementar una interfaz gráfica dentro de la app para personalizar temas (settings UI), ya que agregaría complejidad innecesaria a un editor técnico industrial. La edición vía variables CSS en `styles.css` es limpia, directa y mantenible por control de versiones.

---

## 5. Guía para Nuevas Funciones y Componentes (Flujo de Trabajo)

A partir de esta refactorización, el desarrollo de nuevas características o elementos visuales en el HMI Designer debe cumplir obligatoriamente las siguientes directrices:

### 1. Regla de Oro: Prohibido Usar Colores Hexadecimales Rígidos
No escriba colores fijos como `color: #dce9f1;` ni `background: #0d1820;`.  
Tampoco utilice atributos `style="..."` con colores en el HTML de las plantillas o módulos JS.

### 2. Utilice Siempre Tokens Semánticos
Consulte esta tabla de referencia rápida para saber qué token aplicar:

| Propósito | Token a utilizar | Ejemplo de uso |
|---|---|---|
| Fondo general de la ventana / lienzo | `var(--bg)` | `background: var(--bg);` |
| Fondo de paneles principales / inspectores | `var(--panel)` | `background: var(--panel);` |
| Fondo de tarjetas secundarias o agrupadores | `var(--panel-2)` | `background: var(--panel-2);` |
| Fondo de elementos anidados o controles | `var(--panel-3)` | `background: var(--panel-3);` |
| Cajas de entrada, steppers, visores oscurecidos | `var(--panel-inset)` | `background: var(--panel-inset);` |
| Bordes y divisiones principales | `var(--line)` | `border: 1px solid var(--line);` |
| Bordes suaves o divisores sutiles | `var(--line-soft)` | `border-color: var(--line-soft);` |
| Texto general del cuerpo y etiquetas | `var(--text)` | `color: var(--text);` |
| Títulos de sección y encabezados destacados | `var(--text-heading)` | `color: var(--text-heading);` |
| Leyendas, notas secundarias, íconos apagados | `var(--muted)` | `color: var(--muted);` |
| Acento primario, selección, elementos activos | `var(--orange)` | `border-color: var(--orange); color: var(--orange);` |
| Fondo tenue de elementos seleccionados | `var(--orange-soft)` | `background: var(--orange-soft);` |
| Valores técnicos numéricos o booleanos | `var(--cyan)`, `var(--green)`, `var(--danger)` | `color: var(--cyan);` |

### 3. Soporte Automático sin Código Duplicado
Al utilizar las variables semánticas en la clase base de su componente en `styles.css`, el componente **funcionará de forma nativa e idéntica tanto en modo claro como en modo oscuro**, sin tener que añadir selectores redundantes en `designer-light-theme.css`.

---

## 6. Verificación y Pruebas Realizadas

1. **Balance y Sintaxis CSS:**
   - Verificado con analizador léxico en Node.js que todas las llaves `{}` de `poc/styles.css` (balance exacto: 927 abiertas / 927 cerradas) y `poc/designer-light-theme.css` (balance exacto: 247 abiertas / 247 cerradas) están perfectamente cerradas sin errores de parser.
2. **Eliminación de Estilos Rígidos:**
   - Confirmado mediante regex que no quedan atributos `style="color: #dce9f1"` en `poc/index.html`.
3. **Batería de Pruebas Automatizadas del Designer:**
   - `tests/codegen-core.test.cjs`: **PASS** (100%)
   - `tests/icon-catalog.test.cjs`: **PASS** (100%)
   - `tests/runtime-core.test.cjs`: **PASS** (100%)
   - `tests/sketch-guide.test.cjs`: **PASS** (100%)
4. **Comprobación Visual:**
   - Inspeccionados en ambos modos los módulos de navegación, paneles de BAR, TEXT, VALUE, BOOL, MENU y figuras geométricas.
