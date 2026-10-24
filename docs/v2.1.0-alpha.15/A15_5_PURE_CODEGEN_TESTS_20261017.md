# Alpha15 · A15-5 — Codegen puro y golden tests

Fecha: `2026-10-17`

```text
A15_5_PURE_CODEGEN_TESTS=PASS_PARTIAL
GOLDEN=5 proyectos (full, fieldsOnly, shapesOnly, legacyPageGap, menu) idénticos byte a byte
CORE_NODE=designer-codegen-core.js + designer-menu.js ejecutables en node sin DOM (23 verificaciones)
PENDIENTE=estática por página, NAV_BAR, PixelMaps y ensamblado del header siguen en capas .replace()
```

## Qué se hizo `[ejecutado]`

1. **Golden tests de caracterización antes de tocar código.**
   - `tests/fixtures.cjs` define 5 proyectos:
     - las figuras, RAW_TEXT con FreeFont, 3 páginas y NAV_BAR;
     - campos sin gráfica estática (con fondo TRANSPARENT legado);
     - sólo figuras;
     - ids de página `[0, 2]`;
     - VALUE + MENU de 2 niveles.
   - `tests/codegen-golden.cjs` genera el header en el Designer y lo compara con `tests/golden/*.h`.
   - Los golden se generaron con el código anterior a la extracción.
2. **Núcleo puro** `poc/designer-codegen-core.js` (UMD: `window.JWPLCHMICodegenCore` y `require`).
   - Contiene `fieldBlock`, `valueFieldBlock`, `textFieldBlock`, `boolFieldBlock`, `barFieldBlock`, `variableDeclaration`, `setterCall`, `hex565`, `cppString`, `cppFloat` y `sanitizeSymbol`.
   - `app.js` (`fieldContract`), `designer-bool.js` y `designer-bar.js` delegan en él.
   - Se eliminaron las copias de `cppString`/`cppBool`/`cppFloat`/`hex565` que quedaron sin uso.
3. **MENU sin DOM**: `designer-menu.js` publica la parte pura (`generate`, `validate`, `layout`, `paint`, `renderToArray`) cuando no hay `document`.
4. `tests/codegen-core.test.cjs` corre en node sin navegador:
   - bloque VALUE exacto, escapes, `TRANSPARENT` → `0x0000` y BAR acotado;
   - codegen del MENU:
     - no redeclara variables de campos;
     - declara una sola vez la variable compartida;
     - símbolos únicos;
     - padre simbólico;
     - página simbólica;
   - validación: profundidad 4, conflicto de tipo con un campo, int32 con paso no entero, variable vacía, dos MENU en la misma página y `#error`;
   - layout;
   - con g++: compilación contra los headers reales y pintor del MENU == framebuffer del firmware en host.
5. `tests/run-all.cjs` ejecuta todo y omite lo que requiere g++ si no está instalado.

El golden detectó una regresión durante la extracción: al delegar BAR completo en `app.js`, `replaceHelperCall` de `designer-bar.js` tomaba el `(` del comentario de estilo. Se mantuvo el contrato de capas: `app.js` emite TEXT/VALUE, y BOOL/BAR salen como `JWPLC_UITextField` provisional que sus capas reemplazan.

## Pendiente (para cerrar D3 del plan)

```text
STATIC_DRAW=staticDrawCode()/generatedPageEnterBlock() (designer-codegen.js) -> mover al núcleo
NAV_BAR=paintNavBar es puro pero vive en designer-pages.js -> exponer sin DOM
PIXELMAP=designer-pixelmap-codegen-guard.js reescribe el header por texto
ASSEMBLY=header armado por capas de .replace() -> generate(model, profile) único
OPENPLC=el mismo generate() debe poder llamarlo el editor OpenPLC (D3/D4)
```

Cada paso se valida con `codegen-golden.cjs`: un cambio de salida no intencional falla.
