# Índice: `cli/`

CLI `jwhmi` para crear, validar y generar proyectos `.jwhmi` sin la UI (agentes y terminal).
Guía de uso: [`../agent/GUIA_AGENTES.md`](../agent/GUIA_AGENTES.md).

| Archivo | Qué hace |
|---|---|
| `jwhmi.cjs` | Entrada de la CLI: `new`, `describe`, `normalize`, `validate`, `generate`, `preview`, `check`, `build`, `schema`. |
| `lib/spec.cjs` | **Fuente única** del formato: tipos, propiedades, valores por defecto, rangos y enumerados. |
| `lib/normalize.cjs` | Forma corta → `JWPLC_HMI_PROJECT` v1 completo (colores, páginas, ids, MENU anidado, layerOrder). Puro. |
| `lib/validate.cjs` | Errores y avisos con sugerencias. Puro; reutiliza `designer-menu.js` y `designer-cp437.js` en una caja de node. |
| `lib/designer.cjs` | El Designer real en Chromium headless: carga, header (`#codeOutput`), cajas reales y vistas previas PNG. |
| `lib/compile.cjs` | `g++ -fsyntax-only` del sketch base contra los headers del package + símbolos en las librerías. |
| `lib/schema.cjs` | Genera `schema/jwhmi.schema.json` y `agent/REFERENCIA_JWHMI.md` desde la spec. |

Al cambiar el formato del Designer (propiedad nueva, enumerado, valor por defecto): actualizar
`lib/spec.cjs`, correr `node cli/jwhmi.cjs schema` y `node tests/agent-cli.test.cjs`, que compara
la spec con lo que crea el Designer.
